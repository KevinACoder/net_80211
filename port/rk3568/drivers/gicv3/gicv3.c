/*	$NetBSD: gicv3.c,v 1.54.12.1 2025/09/05 09:21:33 martin Exp $	*/

/*-
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * GICv3 distributor/redistributor driver, derived from NetBSD
 * sys/arch/arm/cortex/gicv3.c (rev 1.54.12.1). The pic framework,
 * multiprocessor support and the intrsource bookkeeping are replaced
 * by the bare-metal single-PE layer in intr.c; the register
 * sequences - including the shareability readback that decides
 * whether LPI table writes need an explicit cache clean - are the
 * upstream ones.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdint.h>

#include "fcache.h"

#include "gicv3.h"

#define	GIC_PRIO_SHIFT_NS	4
#define	GIC_PRIO_SHIFT_S	3

static inline uint32_t
gicd_read_4(struct gicv3_softc *sc, uint32_t reg)
{
	return *(volatile uint32_t *) (sc->sc_gicd_base + reg);
}

static inline void
gicd_write_4(struct gicv3_softc *sc, uint32_t reg, uint32_t val)
{
	*(volatile uint32_t *) (sc->sc_gicd_base + reg) = val;
}

static inline uint64_t
gicd_read_8(struct gicv3_softc *sc, uint32_t reg)
{
	return *(volatile uint64_t *) (sc->sc_gicd_base + reg);
}

static inline void
gicd_write_8(struct gicv3_softc *sc, uint32_t reg, uint64_t val)
{
	*(volatile uint64_t *) (sc->sc_gicd_base + reg) = val;
}

static inline uint32_t
gicr_read_4(struct gicv3_softc *sc, uintptr_t frame, uint32_t reg)
{
	(void) sc;
	return *(volatile uint32_t *) (frame + reg);
}

static inline void
gicr_write_4(struct gicv3_softc *sc, uintptr_t frame, uint32_t reg,
    uint32_t val)
{
	(void) sc;
	*(volatile uint32_t *) (frame + reg) = val;
}

static inline uint64_t
gicr_read_8(struct gicv3_softc *sc, uintptr_t frame, uint32_t reg)
{
	(void) sc;
	return *(volatile uint64_t *) (frame + reg);
}

static inline void
gicr_write_8(struct gicv3_softc *sc, uintptr_t frame, uint32_t reg,
    uint64_t val)
{
	(void) sc;
	*(volatile uint64_t *) (frame + reg) = val;
}

/* LPI config/pending tables (identity-mapped RAM: PA == VA) */
static uint8_t gicv3_lpiconf[GICV3_LPI_MAXSOURCES]
    __attribute__((aligned(0x1000)));
static uint8_t gicv3_lpipend[(GIC_LPI_BASE + GICV3_LPI_MAXSOURCES) / 8]
    __attribute__((aligned(0x10000)));

static void
gicv3_dcache_wb_range(uintptr_t va, size_t len)
{

	FCacheDCacheFlushRange((intptr_t) va, (uint32_t) len);
	dsb_sy();
}

void
gicv3_establish_irq(struct gicv3_softc *sc, u_int irq, int is_edge)
{
	const u_int group = irq / 32;
	const u_int icfg_shift = (irq & 0xf) * 2;
	const u_int ipriority_shift = (irq & 0x3) * 8;
	uint32_t icfg, ipriority;
	uint64_t irouter;

	if (group == 0) {
		icfg = gicr_read_4(sc, sc->sc_redist_base,
		    GICR_ICFGRn(irq / 16));
		icfg &= ~(0x2U << icfg_shift);
		if (is_edge)
			icfg |= (0x2U << icfg_shift);
		gicr_write_4(sc, sc->sc_redist_base, GICR_ICFGRn(irq / 16),
		    icfg);

		ipriority = gicr_read_4(sc, sc->sc_redist_base,
		    GICR_IPRIORITYRn(irq / 4));
	} else {
		/* Route SPIs to the primary (only) PE. */
		irouter = gicr_read_8(sc, sc->sc_redist_base, GICR_TYPER);
		irouter = __SHIFTOUT(irouter,
		    GICR_TYPER_Affinity_Value) & 0xffffffffULL;
		gicd_write_8(sc, GICD_IROUTER(irq), irouter);

		icfg = gicd_read_4(sc, GICD_ICFGRn(irq / 16));
		icfg &= ~(0x2U << icfg_shift);
		if (is_edge)
			icfg |= (0x2U << icfg_shift);
		gicd_write_4(sc, GICD_ICFGRn(irq / 16), icfg);

		ipriority = gicd_read_4(sc, GICD_IPRIORITYRn(irq / 4));
	}
	ipriority &= ~(0xffU << ipriority_shift);
	if (group == 0) {
		gicr_write_4(sc, sc->sc_redist_base,
		    GICR_IPRIORITYRn(irq / 4), ipriority);
	} else {
		gicd_write_4(sc, GICD_IPRIORITYRn(irq / 4), ipriority);
	}
}

void
gicv3_set_irq_priority(struct gicv3_softc *sc, u_int irq, uint8_t prio8)
{
	const u_int group = irq / 32;
	const u_int ipriority_shift = (irq & 0x3) * 8;
	uint32_t ipriority;

	if (group == 0) {
		ipriority = gicr_read_4(sc, sc->sc_redist_base,
		    GICR_IPRIORITYRn(irq / 4));
		ipriority &= ~(0xffU << ipriority_shift);
		ipriority |= (uint32_t) prio8 << ipriority_shift;
		gicr_write_4(sc, sc->sc_redist_base,
		    GICR_IPRIORITYRn(irq / 4), ipriority);
	} else {
		ipriority = gicd_read_4(sc, GICD_IPRIORITYRn(irq / 4));
		ipriority &= ~(0xffU << ipriority_shift);
		ipriority |= (uint32_t) prio8 << ipriority_shift;
		gicd_write_4(sc, GICD_IPRIORITYRn(irq / 4), ipriority);
	}
}

void
gicv3_unblock_irq(struct gicv3_softc *sc, u_int irq)
{
	const u_int group = irq / 32;
	const uint32_t mask = __BIT(irq % 32);

	if (group == 0) {
		sc->sc_enabled_sgippi |= mask;
		gicr_write_4(sc, sc->sc_redist_base, GICR_ISENABLER0, mask);
		while (gicr_read_4(sc, sc->sc_redist_base, GICR_CTLR) &
		    GICR_CTLR_RWP)
			;
	} else {
		gicd_write_4(sc, GICD_ISENABLERn(group), mask);
		while (gicd_read_4(sc, GICD_CTRL) & GICD_CTRL_RWP)
			;
	}
}

void
gicv3_block_irq(struct gicv3_softc *sc, u_int irq)
{
	const u_int group = irq / 32;
	const uint32_t mask = __BIT(irq % 32);

	if (group == 0) {
		sc->sc_enabled_sgippi &= ~mask;
		gicr_write_4(sc, sc->sc_redist_base, GICR_ICENABLER0, mask);
		while (gicr_read_4(sc, sc->sc_redist_base, GICR_CTLR) &
		    GICR_CTLR_RWP)
			;
	} else {
		gicd_write_4(sc, GICD_ICENABLERn(group), mask);
		while (gicd_read_4(sc, GICD_CTRL) & GICD_CTRL_RWP)
			;
	}
}

void
gicv3_lpi_establish(struct gicv3_softc *sc, u_int lpi, uint8_t prio8)
{

	sc->sc_lpiconf[lpi] = (prio8 & (uint8_t) GIC_LPICONF_Priority) |
	    (uint8_t) GIC_LPICONF_Res1;

	if (sc->sc_lpiconf_flush)
		gicv3_dcache_wb_range((uintptr_t) &sc->sc_lpiconf[lpi], 1);
	else
		dsb_ishst();
}

void
gicv3_lpi_unblock(struct gicv3_softc *sc, u_int lpi)
{

	sc->sc_lpiconf[lpi] |= (uint8_t) GIC_LPICONF_Enable;

	if (sc->sc_lpiconf_flush)
		gicv3_dcache_wb_range((uintptr_t) &sc->sc_lpiconf[lpi], 1);
	else
		dsb_ishst();
}

void
gicv3_lpi_block(struct gicv3_softc *sc, u_int lpi)
{

	sc->sc_lpiconf[lpi] &= ~(uint8_t) GIC_LPICONF_Enable;

	if (sc->sc_lpiconf_flush)
		gicv3_dcache_wb_range((uintptr_t) &sc->sc_lpiconf[lpi], 1);
	else
		dsb_ishst();
}

static void
gicv3_dist_enable(struct gicv3_softc *sc)
{
	uint32_t gicd_ctrl;
	u_int n;

	/* Disable the distributor */
	gicd_ctrl = gicd_read_4(sc, GICD_CTRL);
	gicd_ctrl &= ~(GICD_CTRL_EnableGrp1A | GICD_CTRL_ARE_NS);
	gicd_write_4(sc, GICD_CTRL, gicd_ctrl);

	/* Wait for register write to complete */
	while (gicd_read_4(sc, GICD_CTRL) & GICD_CTRL_RWP)
		;

	/* Clear all INTID enable bits */
	for (n = 32; n < 1020; n += 32)
		gicd_write_4(sc, GICD_ICENABLERn(n / 32), ~0);

	/* Set default priorities to lowest */
	for (n = 32; n < 1020; n += 4)
		gicd_write_4(sc, GICD_IPRIORITYRn(n / 4), ~0);

	/* Set all interrupts to G1NS */
	for (n = 32; n < 1020; n += 32) {
		gicd_write_4(sc, GICD_IGROUPRn(n / 32), ~0);
		gicd_write_4(sc, GICD_IGRPMODRn(n / 32), 0);
	}

	/* Set all interrupts level-sensitive by default */
	for (n = 32; n < 1020; n += 16)
		gicd_write_4(sc, GICD_ICFGRn(n / 16), 0);

	/* Wait for register writes to complete */
	while (gicd_read_4(sc, GICD_CTRL) & GICD_CTRL_RWP)
		;

	/* Enable Affinity routing and G1NS interrupts */
	gicd_ctrl = GICD_CTRL_EnableGrp1A | GICD_CTRL_ARE_NS;
	gicd_write_4(sc, GICD_CTRL, gicd_ctrl);
}

static void
gicv3_redist_enable(struct gicv3_softc *sc)
{
	const uintptr_t frame = sc->sc_redist_base;
	u_int n;

	/* Clear INTID enable bits */
	gicr_write_4(sc, frame, GICR_ICENABLER0, ~0);

	/* Wait for register write to complete */
	while (gicr_read_4(sc, frame, GICR_CTLR) & GICR_CTLR_RWP)
		;

	/* Set default priorities to lowest */
	for (n = 0; n < 32; n += 4)
		gicr_write_4(sc, frame, GICR_IPRIORITYRn(n / 4), ~0);

	/* Set all interrupts to G1NS */
	gicr_write_4(sc, frame, GICR_IGROUPR0, ~0);
	gicr_write_4(sc, frame, GICR_IGRPMODR0, 0);

	/* All PPIs level-sensitive by default; establish_irq refines. */
	gicr_write_4(sc, frame, GICR_ICFGRn(1), 0);

	/* Restore current enable bits */
	gicr_write_4(sc, frame, GICR_ISENABLER0, sc->sc_enabled_sgippi);

	/* Wait for register writes to complete */
	while (gicr_read_4(sc, frame, GICR_CTLR) & GICR_CTLR_RWP)
		;
}

static uint64_t
gicv3_cpu_identity(void)
{
	const uint64_t mpidr = cpu_mpidr_aff_read();

	return __SHIFTIN(__SHIFTOUT(mpidr, __BITS(7, 0)),
	    GICR_TYPER_Affinity_Value_Aff0) |
	       __SHIFTIN(__SHIFTOUT(mpidr, __BITS(15, 8)),
	    GICR_TYPER_Affinity_Value_Aff1) |
	       __SHIFTIN(__SHIFTOUT(mpidr, __BITS(23, 16)),
	    GICR_TYPER_Affinity_Value_Aff2) |
	       __SHIFTIN(__SHIFTOUT(mpidr, __BITS(31, 24)),
	    GICR_TYPER_Affinity_Value_Aff3);
}

static uintptr_t
gicv3_find_redist(struct gicv3_softc *sc)
{
	const uint64_t cpu_identity = gicv3_cpu_identity();
	uint64_t gicr_typer;
	uintptr_t frame;

	for (frame = sc->sc_gicr_base;
	    frame < sc->sc_gicr_base + sc->sc_gicr_count * sc->sc_gicr_stride;
	    frame += sc->sc_gicr_stride) {
		gicr_typer = gicr_read_8(sc, frame, GICR_TYPER);
		if ((gicr_typer & GICR_TYPER_Affinity_Value) == cpu_identity) {
			/* remember the PE number for the ITS */
			sc->sc_processor_id =
			    __SHIFTOUT(gicr_typer, GICR_TYPER_Processor_Number);
			return frame;
		}
		if (gicr_typer & GICR_TYPER_Last)
			break;
	}

	return 0;
}

static bool
gicv3_cpuif_is_nonsecure(struct gicv3_softc *sc)
{

	/*
	 * Write 0 to bit7 and see if it sticks. This is only possible if
	 * we have a non-secure view of the PMR register.
	 */
	const uint32_t opmr = icc_pmr_read();
	icc_pmr_write(0);
	const uint32_t npmr = icc_pmr_read();
	icc_pmr_write(opmr);

	(void) sc;
	return (npmr & __BIT(7)) == 0;
}

static bool
gicv3_dist_is_nonsecure(struct gicv3_softc *sc)
{
	const uint32_t gicd_ctrl = gicd_read_4(sc, GICD_CTRL);

	/*
	 * If security is enabled, we have a non-secure view of the
	 * IPRIORITYRn registers and LPI configuration priority fields.
	 */
	return (gicd_ctrl & GICD_CTRL_DS) == 0;
}

static void
gicv3_cpu_init(struct gicv3_softc *sc)
{
	uint32_t icc_sre, icc_ctlr, gicr_waker;

	/* Enable System register access and disable IRQ/FIQ bypass */
	icc_sre = ICC_SRE_EL1_SRE | ICC_SRE_EL1_DFB | ICC_SRE_EL1_DIB;
	icc_sre_write(icc_sre);

	/* Mark the connected PE as being awake */
	gicr_waker = gicr_read_4(sc, sc->sc_redist_base, GICR_WAKER);
	gicr_waker &= ~GICR_WAKER_ProcessorSleep;
	gicr_write_4(sc, sc->sc_redist_base, GICR_WAKER, gicr_waker);
	while (gicr_read_4(sc, sc->sc_redist_base, GICR_WAKER) &
	    GICR_WAKER_ChildrenAsleep)
		;

	/* Set initial priority mask */
	icc_pmr_write(((0xffU - 0xffU) << sc->sc_pmr_shift) & 0xffU);

	/* Set the binary point field to the minimum value */
	icc_bpr1_write(0);

	/* Enable group 1 interrupt signaling */
	icc_igrpen1_write(ICC_IGRPEN_EL1_Enable);

	/* Set EOI mode: EOIR1 write does both priority drop and
	 * deactivation */
	icc_ctlr = icc_ctlr_read();
	icc_ctlr &= ~__BIT(1);	/* ICC_CTLR_EL1_EOImode */
	icc_ctlr_write(icc_ctlr);

	/* Enable redistributor */
	gicv3_redist_enable(sc);
}

static void
gicv3_lpi_cpu_init(struct gicv3_softc *sc)
{
	uint64_t propbase, pendbase;
	uint32_t ctlr;

	/* If physical LPIs are not supported on this redistributor, just
	 * return. */
	const uint64_t typer = gicr_read_8(sc, sc->sc_redist_base,
	    GICR_TYPER);
	if ((typer & GICR_TYPER_PLPIS) == 0)
		return;

	/* Disable LPIs before making changes */
	ctlr = gicr_read_4(sc, sc->sc_redist_base, GICR_CTLR);
	ctlr &= ~GICR_CTLR_Enable_LPIs;
	gicr_write_4(sc, sc->sc_redist_base, GICR_CTLR, ctlr);
	dsb_sy();

	/* Setup the LPI configuration table */
	propbase = sc->sc_lpiconf_pa |
	    __SHIFTIN(13, GICR_PROPBASER_IDbits) |
	    __SHIFTIN(GICR_Shareability_IS, GICR_PROPBASER_Shareability) |
	    __SHIFTIN(GICR_Cache_NORMAL_RA_WA_WB, GICR_PROPBASER_InnerCache);
	gicr_write_8(sc, sc->sc_redist_base, GICR_PROPBASER, propbase);
	propbase = gicr_read_8(sc, sc->sc_redist_base, GICR_PROPBASER);
	if (__SHIFTOUT(propbase, GICR_PROPBASER_Shareability) !=
	    GICR_Shareability_IS) {
		if (__SHIFTOUT(propbase, GICR_PROPBASER_Shareability) ==
		    GICR_Shareability_NS) {
			propbase &= ~GICR_PROPBASER_Shareability;
			propbase |= __SHIFTIN(GICR_Shareability_NS,
			    GICR_PROPBASER_Shareability);
			propbase &= ~GICR_PROPBASER_InnerCache;
			propbase |= __SHIFTIN(GICR_Cache_NORMAL_NC,
			    GICR_PROPBASER_InnerCache);
			gicr_write_8(sc, sc->sc_redist_base, GICR_PROPBASER,
			    propbase);
		}
		sc->sc_lpiconf_flush = true;
	}

	/* Setup the LPI pending table */
	pendbase = sc->sc_lpipend_pa |
	    __SHIFTIN(GICR_Shareability_IS, GICR_PENDBASER_Shareability) |
	    __SHIFTIN(GICR_Cache_NORMAL_RA_WA_WB, GICR_PENDBASER_InnerCache);
	gicr_write_8(sc, sc->sc_redist_base, GICR_PENDBASER, pendbase);
	pendbase = gicr_read_8(sc, sc->sc_redist_base, GICR_PENDBASER);
	if (__SHIFTOUT(pendbase, GICR_PENDBASER_Shareability) ==
	    GICR_Shareability_NS) {
		pendbase &= ~GICR_PENDBASER_Shareability;
		pendbase |= __SHIFTIN(GICR_Shareability_NS,
		    GICR_PENDBASER_Shareability);
		pendbase &= ~GICR_PENDBASER_InnerCache;
		pendbase |= __SHIFTIN(GICR_Cache_NORMAL_NC,
		    GICR_PENDBASER_InnerCache);
		gicr_write_8(sc, sc->sc_redist_base, GICR_PENDBASER, pendbase);
	}

	/* Enable LPIs */
	ctlr = gicr_read_4(sc, sc->sc_redist_base, GICR_CTLR);
	ctlr |= GICR_CTLR_Enable_LPIs;
	gicr_write_4(sc, sc->sc_redist_base, GICR_CTLR, ctlr);
	dsb_sy();
}

int
gicv3_init(struct gicv3_softc *sc)
{

	sc->sc_gicd_typer = gicd_read_4(sc, GICD_TYPER);

	/*
	 * We don't always have a consistent view of priorities between
	 * the CPU interface (ICC_PMR_EL1) and the GICD/GICR registers.
	 * Detect if we are making secure or non-secure accesses to each,
	 * and adjust the values that we write to each accordingly.
	 */
	const bool dist_ns = gicv3_dist_is_nonsecure(sc);
	sc->sc_priority_shift = dist_ns ? GIC_PRIO_SHIFT_NS : GIC_PRIO_SHIFT_S;
	const bool cpuif_ns = gicv3_cpuif_is_nonsecure(sc);
	sc->sc_pmr_shift = cpuif_ns ? GIC_PRIO_SHIFT_NS : GIC_PRIO_SHIFT_S;

	sc->sc_lpiconf = gicv3_lpiconf;
	sc->sc_lpiconf_pa = (uint64_t) (uintptr_t) gicv3_lpiconf;
	sc->sc_lpipend = gicv3_lpipend;
	sc->sc_lpipend_pa = (uint64_t) (uintptr_t) gicv3_lpipend;
	sc->sc_lpi_maxsources = GICV3_LPI_MAXSOURCES;

	sc->sc_redist_base = gicv3_find_redist(sc);
	if (sc->sc_redist_base == 0)
		return -1;

	gicv3_dist_enable(sc);

	gicv3_cpu_init(sc);
	if ((sc->sc_gicd_typer & GICD_TYPER_LPIS) != 0)
		gicv3_lpi_cpu_init(sc);

	return 0;
}
