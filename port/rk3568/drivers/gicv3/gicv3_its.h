/*	$NetBSD: gicv3_its.h,v 1.7 2021/12/09 17:15:55 jmcneill Exp $	*/

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
 * sys/arch/arm/cortex/gicv3_its.h. Exposes LPI allocation and
 * device/event mapping to the bare-metal interrupt layer; the PCI
 * MSI/MSI-X front ends stay in the host environment.
 */

#ifndef _DRIVERS_GICV3_GICV3_ITS_H_
#define _DRIVERS_GICV3_GICV3_ITS_H_

#include <stdbool.h>
#include <stdint.h>

#include "gicv3.h"

/* per-device interrupt translation table bounds */
#define	GICV3_ITS_MAX_DEVICES	8
#define	GICV3_ITS_MAX_EVENTS	16	/* events per device */

struct gicv3_its {
	struct gicv3_softc *its_gic;

	uintptr_t	its_base;

	/* command queue */
	uint8_t		*its_cmd_base;
	uint64_t	its_cmd_pa;
	bool		its_cmd_flush;

	/* device table (direct) and collection table */
	uint8_t		*its_tab_device;
	uint64_t	its_tab_device_pa;
	u_int		its_tab_device_pagesize;
	u_int		its_devbits;
	bool		its_tab_device_shareable;

	uint8_t		*its_tab_coll;
	uint64_t	its_tab_coll_pa;
	u_int		its_tab_coll_pagesize;

	/* devices and their ITTs */
	struct gicv3_its_device {
		uint32_t	dev_id;
		uint8_t		*dev_itt;
		uint64_t	dev_itt_pa;
		u_int		dev_itt_size;
		bool		dev_valid;
	} its_devices[GICV3_ITS_MAX_DEVICES];

	/* LPI bookkeeping: allocation bitmap and owning device */
	uint8_t		its_lpi_used[GICV3_LPI_MAXSOURCES / 8];
	uint16_t	its_lpi_devid[GICV3_LPI_MAXSOURCES];

	uint16_t	its_icid;	/* collection id of this PE */
	uint64_t	its_rdbase;
};

int	gicv3_its_init(struct gicv3_softc *sc, uintptr_t its_base,
	    struct gicv3_its *its);

/* map a device (or grow its ITT); returns 0 on success */
int	gicv3_its_device_map(struct gicv3_its *its, uint32_t devid,
	    u_int count);

/* allocate a free LPI INTID, or -1 */
int	gicv3_its_lpi_alloc(struct gicv3_its *its, uint32_t devid);
void	gicv3_its_lpi_free(struct gicv3_its *its, int lpi);

/* map event -> LPI (MAPTI + SYNC) */
int	gicv3_its_event_map(struct gicv3_its *its, uint32_t devid,
	    uint32_t eventid, u_int lpi);

/* LPI configuration table enable/priority (INV after the change) */
void	gicv3_its_lpi_enable(struct gicv3_its *its, u_int lpi);
void	gicv3_its_lpi_disable(struct gicv3_its *its, u_int lpi);
void	gicv3_its_lpi_set_priority(struct gicv3_its *its, u_int lpi,
	    uint8_t prio8);

/* doorbell address programmed into an endpoint's MSI capability */
uint64_t gicv3_its_trans_addr(struct gicv3_its *its);

/* diagnostics: read back an LPI's property and pending bytes */
void	gicv3_its_dump_lpi(struct gicv3_its *its, u_int lpi,
	    uint8_t *propp, uint8_t *pendp);

#endif	/* _DRIVERS_GICV3_GICV3_ITS_H_ */
