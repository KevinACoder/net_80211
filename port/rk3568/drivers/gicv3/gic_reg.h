/*	$NetBSD: gic_reg.h,v 1.21.12.1 2025/08/31 15:53:52 martin Exp $	*/

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
 * GICv3 (GICD/GICR/ITS) register map and CPU-interface system
 * register accessors, derived from NetBSD sys/arch/arm/cortex/
 * gic_reg.h. Trimmed to what this driver uses.
 */

#ifndef _DRIVERS_GICV3_GIC_REG_H_
#define _DRIVERS_GICV3_GIC_REG_H_

#include <stdint.h>

/* the BSD-ism the derived code keeps */
typedef unsigned int u_int;

#define	__BIT(n)		(1UL << (n))
#define	__BITS(hi, lo)		(((1ULL << ((hi) + 1)) - 1) & ~((1ULL << (lo)) - 1))
#define	__SHIFTIN(v, m)		(((uint64_t)(v) & (m >> __builtin_ctzll(m))) << __builtin_ctzll(m))
#define	__SHIFTOUT(v, m)	(((v) >> __builtin_ctzll(m)) & (m >> __builtin_ctzll(m)))

/* Distributor */
#define	GICD_CTRL		0x0000
#define	GICD_TYPER		0x0004
#define	GICD_IIDR		0x0008
#define	GICD_IGROUPRn(n)	(0x0080 + 4 * (n))
#define	GICD_ISENABLERn(n)	(0x0100 + 4 * (n))
#define	GICD_ICENABLERn(n)	(0x0180 + 4 * (n))
#define	GICD_ISPENDRn(n)	(0x0200 + 4 * (n))
#define	GICD_ICPENDRn(n)	(0x0280 + 4 * (n))
#define	GICD_IPRIORITYRn(n)	(0x0400 + 4 * (n))
#define	GICD_ICFGRn(n)		(0x0C00 + 4 * (n))
#define	GICD_IGRPMODRn(n)	(0x0D00 + 4 * (n))
#define	GICD_IROUTER(n)		(0x6000 + 8 * (n))

#define	GICD_CTRL_RWP			__BIT(31)
#define	GICD_CTRL_DS			__BIT(6)
#define	GICD_CTRL_ARE_S			__BIT(5)
#define	GICD_CTRL_ARE_NS		__BIT(4)
#define	GICD_CTRL_EnableGrp1S		__BIT(2)
#define	GICD_CTRL_EnableGrp1A		__BIT(1)
#define	GICD_CTRL_EnableGrp0		__BIT(0)

#define	GICD_TYPER_No1N			__BIT(25)
#define	GICD_TYPER_IDbits		__BITS(23, 19)
#define	GICD_TYPER_LPIS			__BIT(17)
#define	GICD_TYPER_ITLinesNumber	__BITS(4, 0)
#define	GICD_TYPER_LINES(n)		\
	(32 * (__SHIFTOUT((n), GICD_TYPER_ITLinesNumber) + 1) < 1020 ? \
	 32 * (__SHIFTOUT((n), GICD_TYPER_ITLinesNumber) + 1) : 1020)

#define	GICD_IROUTER_Interrupt_Routing_mode __BIT(31)

/* Redistributor (SGI/PPI frame offsets include the 64K SGI page) */
#define	GICR_CTLR		0x0000
#define	GICR_TYPER		0x0008
#define	GICR_WAKER		0x0014
#define	GICR_PROPBASER		0x0070
#define	GICR_PENDBASER		0x0078
#define	GICR_IGROUPR0		0x10080
#define	GICR_ISENABLER0		0x10100
#define	GICR_ICENABLER0		0x10180
#define	GICR_IPRIORITYRn(n)	(0x10400 + 4 * (n))
#define	GICR_ICFGRn(n)		(0x10C00 + 4 * (n))
#define	GICR_IGRPMODR0		0x10D00

#define	GICR_CTLR_RWP			__BIT(3)
#define	GICR_CTLR_Enable_LPIs		__BIT(0)

#define	GICR_TYPER_Affinity_Value_Aff3	__BITS(63, 56)
#define	GICR_TYPER_Affinity_Value_Aff2	__BITS(55, 48)
#define	GICR_TYPER_Affinity_Value_Aff1	__BITS(47, 40)
#define	GICR_TYPER_Affinity_Value_Aff0	__BITS(39, 32)
#define	GICR_TYPER_Affinity_Value	__BITS(63, 32)
#define	GICR_TYPER_Processor_Number	__BITS(23, 8)
#define	GICR_TYPER_Last			__BIT(4)
#define	GICR_TYPER_PLPIS		__BIT(0)

#define	GICR_WAKER_ChildrenAsleep	__BIT(2)
#define	GICR_WAKER_ProcessorSleep	__BIT(1)

#define	GICR_PROPBASER_Physical_Address	__BITS(51, 12)
#define	GICR_PROPBASER_Shareability	__BITS(11, 10)
#define	GICR_PROPBASER_InnerCache	__BITS(9, 7)
#define	GICR_PROPBASER_IDbits		__BITS(4, 0)

#define	GICR_PENDBASER_PTZ		__BIT(62)
#define	GICR_PENDBASER_Physical_Address	__BITS(51, 16)
#define	GICR_PENDBASER_Shareability	__BITS(11, 10)
#define	GICR_PENDBASER_InnerCache	__BITS(9, 7)

#define	GICR_Shareability_NS		0
#define	GICR_Shareability_IS		1
#define	GICR_Shareability_OS		2

#define	GICR_Cache_NORMAL_NC		1
#define	GICR_Cache_NORMAL_RA_WA_WB	7

/* LPIs */
#define	GIC_LPI_BASE			0x2000

#define	GIC_LPICONF_Priority		__BITS(7, 2)
#define	GIC_LPICONF_Res1		__BIT(1)
#define	GIC_LPICONF_Enable		__BIT(0)

/* ITS */
#define	GITS_CTLR		0x00000
#define	GITS_IIDR		0x00004
#define	GITS_TYPER		0x00008
#define	GITS_CBASER		0x00080
#define	GITS_CWRITER		0x00088
#define	GITS_CREADR		0x00090
#define	GITS_BASERn(n)		(0x00100 + 8 * (n))
#define	GITS_TRANSLATER		0x10040

#define	GITS_CTLR_Quiescent		__BIT(31)
#define	GITS_CTLR_Enabled		__BIT(0)

#define	GITS_TYPER_PTA			__BIT(19)
#define	GITS_TYPER_Devbits		__BITS(17, 13)
#define	GITS_TYPER_ID_bits		__BITS(12, 8)
#define	GITS_TYPER_ITT_entry_size	__BITS(7, 4)
#define	GITS_TYPER_Physical		__BIT(0)

#define	GITS_CBASER_Valid		__BIT(63)
#define	GITS_CBASER_InnerCache		__BITS(61, 59)
#define	GITS_CBASER_Shareability	__BITS(11, 10)
#define	GITS_CBASER_Size		__BITS(7, 0)

#define	GITS_CWRITER_Offset		__BITS(19, 5)
#define	GITS_CWRITER_Retry		__BIT(0)

#define	GITS_CREADR_Offset		__BITS(19, 5)
#define	GITS_CREADR_Stalled		__BIT(0)

#define	GITS_BASER_Valid		__BIT(63)
#define	GITS_BASER_Indirect		__BIT(62)
#define	GITS_BASER_InnerCache		__BITS(61, 59)
#define	GITS_BASER_Type			__BITS(58, 56)
#define	GITS_BASER_Entry_Size		__BITS(52, 48)
#define	GITS_BASER_Physical_Address	__BITS(47, 12)
#define	GITS_BASER_Shareability		__BITS(11, 10)
#define	GITS_BASER_Page_Size		__BITS(9, 8)
#define	GITS_BASER_Size			__BITS(7, 0)

#define	GITS_Shareability_NS		0
#define	GITS_Shareability_IS		1

#define	GITS_Cache_NORMAL_NC		1
#define	GITS_Cache_NORMAL_WA_WB		5
#define	GITS_Cache_NORMAL_RA_WA_WB	7

#define	GITS_Type_Unimplemented		0
#define	GITS_Type_Devices		1
#define	GITS_Type_InterruptCollections	4

#define	GITS_Page_Size_4KB		0
#define	GITS_Page_Size_16KB		1
#define	GITS_Page_Size_64KB		2

struct gicv3_its_command {
	uint64_t	dw[4];
};

#define	GITS_CMD_MOVI			0x01
#define	GITS_CMD_SYNC			0x05
#define	GITS_CMD_MAPD			0x08
#define	GITS_CMD_MAPC			0x09
#define	GITS_CMD_MAPTI			0x0A
#define	GITS_CMD_INV			0x0C
#define	GITS_CMD_INVALL			0x0D

/* ICC system registers (accessed with the assembler register names) */
static inline uint32_t
icc_pmr_read(void)
{
	uint32_t v;
	__asm volatile("mrs %0, ICC_PMR_EL1" : "=r"(v));
	return v;
}

static inline void
icc_pmr_write(uint32_t v)
{
	__asm volatile("msr ICC_PMR_EL1, %0" :: "r"(v));
}

static inline uint32_t
icc_bpr1_read(void)
{
	uint32_t v;
	__asm volatile("mrs %0, ICC_BPR1_EL1" : "=r"(v));
	return v;
}

static inline void
icc_bpr1_write(uint32_t v)
{
	__asm volatile("msr ICC_BPR1_EL1, %0" :: "r"(v));
}

static inline uint32_t
icc_ctlr_read(void)
{
	uint32_t v;
	__asm volatile("mrs %0, ICC_CTLR_EL1" : "=r"(v));
	return v;
}

static inline void
icc_ctlr_write(uint32_t v)
{
	__asm volatile("msr ICC_CTLR_EL1, %0" :: "r"(v));
}

static inline void
icc_igrpen1_write(uint32_t v)
{
	__asm volatile("msr ICC_IGRPEN1_EL1, %0" :: "r"(v));
}

#define	ICC_IGRPEN_EL1_Enable	__BIT(0)

static inline uint32_t
icc_sre_read(void)
{
	uint32_t v;
	__asm volatile("mrs %0, ICC_SRE_EL1" : "=r"(v));
	return v;
}

static inline void
icc_sre_write(uint32_t v)
{
	__asm volatile("msr ICC_SRE_EL1, %0" :: "r"(v));
}

#define	ICC_SRE_EL1_SRE	__BIT(0)
#define	ICC_SRE_EL1_DFB	__BIT(1)
#define	ICC_SRE_EL1_DIB	__BIT(2)

static inline uint32_t
icc_rpr_read(void)
{
	uint32_t v;
	__asm volatile("mrs %0, ICC_RPR_EL1" : "=r"(v));
	return v;
}

static inline void
dsb_sy(void)
{
	__asm volatile("dsb sy" ::: "memory");
}

static inline void
dsb_ishst(void)
{
	__asm volatile("dsb ishst" ::: "memory");
}

static inline void
isb_sy(void)
{
	__asm volatile("isb" ::: "memory");
}

/* read MPIDR affinity fields (cpu_mpidr_aff_read upstream) */
static inline uint64_t
cpu_mpidr_aff_read(void)
{
	uint64_t mpidr;
	__asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
	return mpidr & 0x00ffffffULL;	/* Aff3:Aff2:Aff1:Aff0 */
}

#endif	/* _DRIVERS_GICV3_GIC_REG_H_ */
