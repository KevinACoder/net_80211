/*	$NetBSD: gicv3_its.c,v 1.41 2025/01/28 21:20:45 jmcneill Exp $	*/

/*-
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jared McNeill <jmcneill@invisible.ca>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * GICv3 Interrupt Translation Service, derived from NetBSD
 * sys/arch/arm/cortex/gicv3_its.c (rev 1.41). Command queue, table
 * programming (with the shareability readback deciding explicit
 * cache maintenance) and the MAPC/MAPD/MAPTI/INV/SYNC sequences are
 * the upstream ones; the vmem/kmem allocators are replaced by static
 * buffers, the PCI MSI/MSI-X front ends by a plain LPI allocator.
 * The device table is used in direct mode.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdint.h>
#include <string.h>

#include "fcache.h"
#include "fsleep.h"

#include "gicv3_its.h"

/*
 * ITS translation table sizes
 */
#define	GITS_COMMANDS_SIZE	0x1000
#define	GITS_COMMANDS_ALIGN	0x10000

#define	GITS_ITT_ALIGN		0x100

/* command queue / tables, all in identity-mapped RAM (PA == VA) */
static uint8_t its_cmd_space[GITS_COMMANDS_SIZE]
    __attribute__((aligned(GITS_COMMANDS_ALIGN)));
static uint8_t its_devtab_space[512 * 1024]
    __attribute__((aligned(0x10000)));
static uint8_t its_colltab_space[4096]
    __attribute__((aligned(0x10000)));
static uint8_t its_itt_space[GICV3_ITS_MAX_DEVICES * GITS_ITT_ALIGN]
    __attribute__((aligned(GITS_ITT_ALIGN)));

static inline uint32_t
gits_read_4(struct gicv3_its *its, uint32_t reg)
{
	return *(volatile uint32_t *) (its->its_base + reg);
}

static inline void
gits_write_4(struct gicv3_its *its, uint32_t reg, uint32_t val)
{
	*(volatile uint32_t *) (its->its_base + reg) = val;
}

static inline uint64_t
gits_read_8(struct gicv3_its *its, uint32_t reg)
{
	return *(volatile uint64_t *) (its->its_base + reg);
}

static inline void
gits_write_8(struct gicv3_its *its, uint32_t reg, uint64_t val)
{
	*(volatile uint64_t *) (its->its_base + reg) = val;
}

static void
its_dcache_wb_range(uintptr_t va, size_t len)
{

	FCacheDCacheFlushRange((intptr_t) va, (uint32_t) len);
	dsb_sy();
}

static int
gits_command(struct gicv3_its *its, const struct gicv3_its_command *cmd)
{
	uint64_t cwriter, creadr;
	u_int woff;

	creadr = gits_read_8(its, GITS_CREADR);
	if (creadr & GITS_CREADR_Stalled) {
		return -1;
	}

	cwriter = gits_read_8(its, GITS_CWRITER);
	woff = cwriter & GITS_CWRITER_Offset;

	uint64_t *dw = (uint64_t *)(its->its_cmd_base + woff);
	for (int i = 0; i < 4; i++) {
		dw[i] = cmd->dw[i];
	}

	if (its->its_cmd_flush) {
		its_dcache_wb_range((uintptr_t) dw, sizeof(cmd->dw));
	}
	dsb_sy();

	woff += sizeof(cmd->dw);
	if (woff == GITS_COMMANDS_SIZE)
		woff = 0;

	gits_write_8(its, GITS_CWRITER, woff);

	return 0;
}

static int
gits_command_mapc(struct gicv3_its *its, uint16_t icid, uint64_t rdbase,
    bool v)
{
	struct gicv3_its_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.dw[0] = GITS_CMD_MAPC;
	cmd.dw[2] = icid;
	if (v) {
		cmd.dw[2] |= rdbase;
		cmd.dw[2] |= __BIT(63);
	}

	return gits_command(its, &cmd);
}

static int
gits_command_mapd(struct gicv3_its *its, uint32_t deviceid, uint64_t itt_addr,
    u_int size, bool v)
{
	struct gicv3_its_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.dw[0] = GITS_CMD_MAPD | ((uint64_t)deviceid << 32);
	if (v) {
		cmd.dw[1] = (u_int) (size > 1 ? size : 1) - 1;
		cmd.dw[2] = itt_addr | __BIT(63);
	}

	return gits_command(its, &cmd);
}

static int
gits_command_mapti(struct gicv3_its *its, uint32_t deviceid, uint32_t eventid,
    uint32_t pintid, uint16_t icid)
{
	struct gicv3_its_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.dw[0] = GITS_CMD_MAPTI | ((uint64_t)deviceid << 32);
	cmd.dw[1] = eventid | ((uint64_t)pintid << 32);
	cmd.dw[2] = icid;

	return gits_command(its, &cmd);
}

static int
gits_command_inv(struct gicv3_its *its, uint32_t deviceid, uint32_t eventid)
{
	struct gicv3_its_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.dw[0] = GITS_CMD_INV | ((uint64_t)deviceid << 32);
	cmd.dw[1] = eventid;

	return gits_command(its, &cmd);
}

static int
gits_command_invall(struct gicv3_its *its, uint16_t icid)
{
	struct gicv3_its_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.dw[0] = GITS_CMD_INVALL;
	cmd.dw[2] = icid;

	return gits_command(its, &cmd);
}

static int
gits_command_sync(struct gicv3_its *its, uint64_t rdbase)
{
	struct gicv3_its_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.dw[0] = GITS_CMD_SYNC;
	cmd.dw[2] = rdbase;

	return gits_command(its, &cmd);
}

static int
gits_wait(struct gicv3_its *its)
{
	u_int woff, roff;
	int retry;

	/*
	 * The ITS command queue is empty when CWRITER and CREADR specify
	 * the same base address offset value.
	 */
	for (retry = 1000; retry > 0; retry--) {
		woff = gits_read_8(its, GITS_CWRITER) & GITS_CWRITER_Offset;
		roff = gits_read_8(its, GITS_CREADR) & GITS_CREADR_Offset;
		if (woff == roff)
			break;
		fsleep_microsec(100);
	}
	if (retry == 0) {
		return -2;
	}

	return 0;
}

static void
gicv3_its_command_init(struct gicv3_softc *sc, struct gicv3_its *its)
{
	uint64_t cbaser, tmp;

	(void) sc;

	its->its_cmd_base = its_cmd_space;
	its->its_cmd_pa = (uint64_t) (uintptr_t) its_cmd_space;
	its_dcache_wb_range((uintptr_t) its->its_cmd_base,
	    GITS_COMMANDS_SIZE);

	cbaser = its->its_cmd_pa;
	cbaser |= __SHIFTIN((GITS_COMMANDS_SIZE / 4096) - 1, GITS_CBASER_Size);
	cbaser |= GITS_CBASER_Valid;

	cbaser |= __SHIFTIN(GITS_Cache_NORMAL_WA_WB, GITS_CBASER_InnerCache);
	cbaser |= __SHIFTIN(GITS_Shareability_IS, GITS_CBASER_Shareability);
	gits_write_8(its, GITS_CBASER, cbaser);

	tmp = gits_read_8(its, GITS_CBASER);
	if (__SHIFTOUT(tmp, GITS_CBASER_Shareability) != GITS_Shareability_IS) {
		if (__SHIFTOUT(tmp, GITS_CBASER_Shareability) ==
		    GITS_Shareability_NS) {
			cbaser &= ~GITS_CBASER_InnerCache;
			cbaser |= __SHIFTIN(GITS_Cache_NORMAL_NC,
			    GITS_CBASER_InnerCache);
			cbaser &= ~GITS_CBASER_Shareability;
			cbaser |= __SHIFTIN(GITS_Shareability_NS,
			    GITS_CBASER_Shareability);
			gits_write_8(its, GITS_CBASER, cbaser);
		}

		its->its_cmd_flush = true;
	}

	gits_write_8(its, GITS_CWRITER, 0);
}

static void
gicv3_its_table_init(struct gicv3_softc *sc, struct gicv3_its *its)
{
	u_int page_size, table_align;
	u_int devbits, innercache, share;
	uint64_t baser;
	int tab;

	const uint64_t typer = gits_read_8(its, GITS_TYPER);

	/* Default values */
	devbits = __SHIFTOUT(typer, GITS_TYPER_Devbits) + 1;
	if (devbits > 16)
		devbits = 16;	/* the direct table covers IDs 0..65535 */
	innercache = GITS_Cache_NORMAL_WA_WB;
	share = GITS_Shareability_IS;
	its->its_devbits = devbits;

	for (tab = 0; tab < 8; tab++) {
		uint64_t l1_entry_size;
		uint64_t table_size;
		const char *table_type;
		uint8_t **tab_base;
		uint64_t *tab_pa;

		baser = gits_read_8(its, GITS_BASERn(tab));

		l1_entry_size = __SHIFTOUT(baser, GITS_BASER_Entry_Size) + 1;

		switch (__SHIFTOUT(baser, GITS_BASER_Page_Size)) {
		case GITS_Page_Size_64KB:
			page_size = 65536;
			break;
		case GITS_Page_Size_16KB:
			page_size = 16384;
			break;
		case GITS_Page_Size_4KB:
		default:
			page_size = 4096;
		}
		table_align = page_size;

		switch (__SHIFTOUT(baser, GITS_BASER_Type)) {
		case GITS_Type_Devices:
			/*
			 * Table size scales with the width of the
			 * DeviceID. The table is used in direct mode.
			 */
			table_size = l1_entry_size << devbits;
			if (table_size > sizeof(its_devtab_space))
				table_size = sizeof(its_devtab_space);
			table_type = "Devices";
			tab_base = &its->its_tab_device;
			tab_pa = &its->its_tab_device_pa;
			its->its_tab_device_pagesize = page_size;
			break;
		case GITS_Type_InterruptCollections:
			/*
			 * Allocate space for one interrupt collection
			 * per CPU (this system has one).
			 */
			table_size = l1_entry_size;
			table_type = "Collections";
			tab_base = &its->its_tab_coll;
			tab_pa = &its->its_tab_coll_pa;
			its->its_tab_coll_pagesize = page_size;
			break;
		default:
			table_size = 0;
			tab_base = NULL;
			tab_pa = NULL;
			break;
		}

		if (table_size == 0)
			continue;

		if (strcmp(table_type, "Devices") == 0) {
			*tab_base = its_devtab_space;
			*tab_pa = (uint64_t) (uintptr_t) its_devtab_space;
		} else {
			*tab_base = its_colltab_space;
			*tab_pa = (uint64_t) (uintptr_t) its_colltab_space;
		}
		memset(*tab_base, 0, table_size);
		its_dcache_wb_range((uintptr_t) *tab_base, table_size);

		baser &= ~GITS_BASER_Size;
		baser |= __SHIFTIN((table_size + page_size - 1) / page_size - 1,
		    GITS_BASER_Size);
		baser &= ~GITS_BASER_Physical_Address;
		baser |= *tab_pa;
		baser &= ~GITS_BASER_InnerCache;
		baser |= __SHIFTIN(innercache, GITS_BASER_InnerCache);
		baser &= ~GITS_BASER_Shareability;
		baser |= __SHIFTIN(share, GITS_BASER_Shareability);
		baser |= GITS_BASER_Valid;
		/* direct device table: never set GITS_BASER_Indirect */

		gits_write_8(its, GITS_BASERn(tab), baser);

		baser = gits_read_8(its, GITS_BASERn(tab));
		if (__SHIFTOUT(baser, GITS_BASER_Shareability) ==
		    GITS_Shareability_NS) {
			baser &= ~GITS_BASER_InnerCache;
			baser |= __SHIFTIN(GITS_Cache_NORMAL_NC,
			    GITS_BASER_InnerCache);

			gits_write_8(its, GITS_BASERn(tab), baser);

			baser = gits_read_8(its, GITS_BASERn(tab));
		}

		if (__SHIFTOUT(baser, GITS_BASER_Type) == GITS_Type_Devices) {
			its->its_tab_device_shareable =
			    __SHIFTOUT(baser, GITS_BASER_Shareability) !=
			    GITS_Shareability_NS;
		}
		(void) table_align;
	}
	(void) sc;
}

static void
gicv3_its_enable(struct gicv3_softc *sc, struct gicv3_its *its)
{
	uint32_t ctlr;

	(void) sc;

	ctlr = gits_read_4(its, GITS_CTLR);
	ctlr |= GITS_CTLR_Enabled;
	gits_write_4(its, GITS_CTLR, ctlr);
}

static void
gicv3_its_cpu_init(struct gicv3_softc *sc, struct gicv3_its *its)
{
	uint64_t rdbase;

	const uint64_t typer = gits_read_8(its, GITS_TYPER);
	if (typer & GITS_TYPER_PTA) {
		rdbase = sc->sc_redist_base;
	} else {
		rdbase = (uint64_t) sc->sc_processor_id << 16;
	}
	its->its_rdbase = rdbase;
	its->its_icid = 0;

	/*
	 * Map collection ID of this CPU to this CPU's redistributor.
	 */
	gits_command_mapc(its, its->its_icid, rdbase, true);
	gits_command_invall(its, its->its_icid);
	gits_wait(its);
}

uint64_t
gicv3_its_trans_addr(struct gicv3_its *its)
{

	return its->its_base + GITS_TRANSLATER;
}

int
gicv3_its_device_map(struct gicv3_its *its, uint32_t devid, u_int count)
{
	struct gicv3_its_device *dev = NULL;
	u_int vectors;
	int error;

	vectors = count > 2 ? count : 2;
	while ((vectors & (vectors - 1)) != 0)
		vectors++;

	const uint64_t typer = gits_read_8(its, GITS_TYPER);
	const u_int itt_entry_size =
	    __SHIFTOUT(typer, GITS_TYPER_ITT_entry_size) + 1;
	const u_int itt_size = ((vectors * itt_entry_size) + GITS_ITT_ALIGN - 1)
	    & ~(GITS_ITT_ALIGN - 1);

	if (devid >= (uint32_t) (1U << its->its_devbits))
		return -1;

	for (u_int i = 0; i < GICV3_ITS_MAX_DEVICES; i++) {
		if (!its->its_devices[i].dev_valid) {
			if (dev == NULL)
				dev = &its->its_devices[i];
			continue;
		}
		if (its->its_devices[i].dev_id == devid)
			return itt_size <= its->its_devices[i].dev_itt_size ?
			    0 : -1;
	}
	if (dev == NULL)
		return -1;

	/* carve the ITT out of the arena */
	{
		uint8_t *arena = its_itt_space;
		uint8_t *free_base = NULL;
		u_int used = 0;

		for (u_int i = 0; i < GICV3_ITS_MAX_DEVICES; i++) {
			if (its->its_devices[i].dev_valid)
				used += its->its_devices[i].dev_itt_size;
		}
		if (used + itt_size > sizeof(its_itt_space))
			return -1;
		free_base = arena + used;
		dev->dev_itt = free_base;
		dev->dev_itt_pa = (uint64_t) (uintptr_t) free_base;
	}
	dev->dev_id = devid;
	dev->dev_itt_size = itt_size;
	dev->dev_valid = true;

	if (its->its_cmd_flush) {
		its_dcache_wb_range((uintptr_t) dev->dev_itt, itt_size);
	}
	dsb_sy();

	/*
	 * Map the device to the ITT
	 */
	const u_int size = __SHIFTOUT(typer, GITS_TYPER_ID_bits) + 1;
	error = gits_command_mapd(its, devid, dev->dev_itt_pa, size, true);
	if (error == 0) {
		error = gits_wait(its);
	}

	return error;
}

int
gicv3_its_lpi_alloc(struct gicv3_its *its, uint32_t devid)
{

	for (u_int n = 0; n < GICV3_LPI_MAXSOURCES; n++) {
		if ((its->its_lpi_used[n / 8] & __BIT(n % 8)) == 0) {
			its->its_lpi_used[n / 8] |= (uint8_t) __BIT(n % 8);
			its->its_lpi_devid[n] = (uint16_t) devid;
			return (int) (GIC_LPI_BASE + n);
		}
	}

	return -1;
}

void
gicv3_its_lpi_free(struct gicv3_its *its, int lpi)
{
	const u_int n = (u_int) lpi - GIC_LPI_BASE;

	if (n >= GICV3_LPI_MAXSOURCES)
		return;

	its->its_lpi_used[n / 8] &= (uint8_t) ~__BIT(n % 8);
	its->its_lpi_devid[n] = 0;
}

int
gicv3_its_event_map(struct gicv3_its *its, uint32_t devid, uint32_t eventid,
    u_int lpi)
{
	int error;

	error = gits_command_mapti(its, devid, eventid, lpi, its->its_icid);
	if (error == 0)
		error = gits_command_sync(its, its->its_rdbase);
	if (error == 0)
		error = gits_wait(its);

	return error;
}

void
gicv3_its_lpi_set_priority(struct gicv3_its *its, u_int lpi, uint8_t prio8)
{

	gicv3_lpi_establish(its->its_gic, lpi - GIC_LPI_BASE, prio8);
	gits_command_inv(its, its->its_lpi_devid[lpi - GIC_LPI_BASE],
	    lpi - GIC_LPI_BASE);
	gits_wait(its);
}

void
gicv3_its_lpi_enable(struct gicv3_its *its, u_int lpi)
{

	gicv3_lpi_unblock(its->its_gic, lpi - GIC_LPI_BASE);
	gits_command_inv(its, its->its_lpi_devid[lpi - GIC_LPI_BASE],
	    lpi - GIC_LPI_BASE);
	gits_wait(its);
}

void
gicv3_its_lpi_disable(struct gicv3_its *its, u_int lpi)
{

	gicv3_lpi_block(its->its_gic, lpi - GIC_LPI_BASE);
	gits_command_inv(its, its->its_lpi_devid[lpi - GIC_LPI_BASE],
	    lpi - GIC_LPI_BASE);
	gits_wait(its);
}

void
gicv3_its_dump_lpi(struct gicv3_its *its, u_int lpi, uint8_t *propp,
    uint8_t *pendp)
{
	struct gicv3_softc *sc = its->its_gic;
	const u_int n = lpi - GIC_LPI_BASE;

	if (propp != NULL) {
		*propp = sc->sc_lpiconf[n];
	}
	if (pendp != NULL && sc->sc_lpipend != NULL) {
		FCacheDCacheInvalidateRange(
		    (intptr_t) &sc->sc_lpipend[n / 8], 1);
		*pendp = (sc->sc_lpipend[n / 8] & __BIT(n % 8)) != 0;
	}
}

int
gicv3_its_init(struct gicv3_softc *sc, uintptr_t its_base,
    struct gicv3_its *its)
{

	const uint64_t typer = *(volatile uint64_t *) (its_base + GITS_TYPER);
	if ((typer & GITS_TYPER_Physical) == 0)
		return -1;

	memset(its, 0, sizeof(*its));
	its->its_gic = sc;
	its->its_base = its_base;

	gicv3_its_command_init(sc, its);
	gicv3_its_table_init(sc, its);

	gicv3_its_enable(sc, its);

	gicv3_its_cpu_init(sc, its);

	return 0;
}
