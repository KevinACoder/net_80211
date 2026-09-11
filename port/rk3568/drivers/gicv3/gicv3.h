/*	$NetBSD: gicv3.h,v 1.11 2021/01/16 21:05:15 jmcneill Exp $	*/

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
 * sys/arch/arm/cortex/gicv3.h. The pic/bus/cpu_info framework is
 * replaced by the bare-metal single-PE interrupt layer in intr.c;
 * the register sequences are the upstream ones.
 */

#ifndef _DRIVERS_GICV3_GICV3_H_
#define _DRIVERS_GICV3_GICV3_H_

#include <stdbool.h>
#include <stdint.h>

#include "gic_reg.h"

/* minimum LPI sources required by the GICv3 spec (sc_lpi_maxsources) */
#define	GICV3_LPI_MAXSOURCES	8192

struct gicv3_softc {
	uintptr_t	sc_gicd_base;
	uintptr_t	sc_gicr_base;	/* first redistributor frame */
	u_int		sc_gicr_stride;
	u_int		sc_gicr_count;

	uint32_t	sc_gicd_typer;
	u_int		sc_priority_shift;
	u_int		sc_pmr_shift;

	uint32_t	sc_enabled_sgippi;

	/* current CPU's redistributor frame (single-PE system) */
	uintptr_t	sc_redist_base;

	/* LPI configuration table (shared by all redistributors) */
	uint8_t		*sc_lpiconf;
	uint64_t	sc_lpiconf_pa;
	bool		sc_lpiconf_flush;

	/* LPI pending table (one per PE) */
	uint8_t		*sc_lpipend;
	uint64_t	sc_lpipend_pa;

	u_int		sc_lpi_maxsources;

	/* processor number used by the ITS when GITS_TYPER.PTA == 0 */
	u_int		sc_processor_id;
};

int	gicv3_init(struct gicv3_softc *sc);

/* interrupt configuration (the pic establish/block/unblock ops) */
void	gicv3_establish_irq(struct gicv3_softc *sc, u_int irq, int is_edge);
void	gicv3_unblock_irq(struct gicv3_softc *sc, u_int irq);
void	gicv3_block_irq(struct gicv3_softc *sc, u_int irq);

/* LPI configuration table access */
void	gicv3_lpi_establish(struct gicv3_softc *sc, u_int lpi,
	    uint8_t prio8);
void	gicv3_lpi_unblock(struct gicv3_softc *sc, u_int lpi);
void	gicv3_lpi_block(struct gicv3_softc *sc, u_int lpi);

#endif	/* _DRIVERS_GICV3_GICV3_H_ */
