/*
 * Copyright (C) 2022, Phytium Technology Co., Ltd.   All Rights Reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * FilePath: fcache.c
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:32:24
 * Description:  This file is for the arm cache functionality.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe     2021/7/3     first release
 */

#include "fcache.h"
#include "ftypes.h"
#include "faarch.h"
#include "fparameters.h"
#include "sdkconfig.h"


/***************************** Include Files *********************************/

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/

/***************** Macros (Inline Functions) Definitions *********************/

#define FREG_CONTROL_DCACHE_BIT (0x00000001U << 2U)
#define FREG_CONTROL_ICACHE_BIT (0x00000001U << 12U)
#define IRQ_FIQ_MASK            0xC0U /* Mask IRQ and FIQ interrupts in cpsr */

/************************** Function Prototypes ******************************/


/**
 * @name: FCacheDCacheEnable
 * @msg:  Enable the Data cache.
 */
void FCacheDCacheEnable(void)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* SCTLR_EL1/EL2 读写为特权操作，EL0 下直接返回。 */
    return;
#else
    u32 ctrl_reg;
#ifndef CONFIG_USE_EL2
    ctrl_reg = (u32)AARCH64_READ_SYSREG(SCTLR_EL1);
#else
    ctrl_reg = (u32)AARCH64_READ_SYSREG(SCTLR_EL2);
#endif

    if ((ctrl_reg & FREG_CONTROL_DCACHE_BIT) == 0x00000000U)
    {
        /* invalidate the Data cache */
        FCacheDCacheInvalidate();
        ctrl_reg |= FREG_CONTROL_DCACHE_BIT;
#ifndef CONFIG_USE_EL2
        AARCH64_WRITE_SYSREG(SCTLR_EL1, ctrl_reg);
#else
        AARCH64_WRITE_SYSREG(SCTLR_EL2, ctrl_reg);
#endif
    }
#endif
}

/**
 * @name: FCacheDCacheDisable
 * @msg:  Disable the Data cache.
 */
void FCacheDCacheDisable(void)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* SCTLR 读写与 set/way 操作为 EL1+，EL0 下直接返回。 */
    return;
#else
    register u32 csid_reg;
    register u32 c7reg;
    register u32 line_size;
    register u32 num_ways;
    register u32 way;
    register u32 way_index;
    register u32 way_adjust;
    register u32 set;
    register u32 set_index;
    register u32 num_set;
    register u32 cache_level;

    DSB();
#ifndef CONFIG_USE_EL2
    asm("mov 	x0, #0\n\t"
        "mrs	x0, sctlr_el1 \n\t"
        "and	w0, w0, #0xfffffffb\n\t"
        "msr	sctlr_el1, x0\n\t"
        "dsb sy\n\t"); /* Cacheability control, for data accesses. */
#else
    asm("mov 	x0, #0\n\t"
        "mrs	x0, sctlr_el2 \n\t"
        "and	w0, w0, #0xfffffffb\n\t"
        "msr	sctlr_el2, x0\n\t"
        "dsb sy\n\t"); /* Cacheability control, for data accesses. */
#endif


    /* Number of level of cache */
    cache_level = 0U;
    /* Select cache level 1 and D cache in CSSR */
    AARCH64_WRITE_SYSREG(CSSELR_EL1, cache_level);
    ISB();

    csid_reg = (u32)AARCH64_READ_SYSREG(CCSIDR_EL1);

    /* Get the cacheline size, way size, index size from csidr */
    line_size = (csid_reg & 0x00000007U) + 0x00000004U;

    /* Number of Ways */
    num_ways = (csid_reg & 0x00001FFFU) >> 3U;
    num_ways += 0x00000001U;

    /*Number of Set*/
    num_set = (csid_reg >> 13U) & 0x00007FFFU;
    num_set += 0x00000001U;

    way_adjust = CLZ(num_ways) - (u32)0x0000001FU;

    way = 0U;
    set = 0U;

    /* Flush all the cachelines */
    for (way_index = 0U; way_index < num_ways; way_index++)
    {
        for (set_index = 0U; set_index < num_set; set_index++)
        {
            c7reg = way | set | cache_level;
            MTCPDC(CISW, c7reg); /* (Data or unified Cache line Clean and Invalidate by Set/Way) */
            set += (0x00000001U << line_size);
        }
        set = 0U;
        way += (0x00000001U << way_adjust);
    }

    /* Wait for Flush to complete */
    DSB();

    /* Number of level of cache */
    cache_level = (0x00000001U << 1U);
    /* Select cache level 2 and D cache in CSSR */
    AARCH64_WRITE_SYSREG(CSSELR_EL1, cache_level);
    ISB();

    csid_reg = (u32)AARCH64_READ_SYSREG(CCSIDR_EL1);

    /* Get the cacheline size, way size, index size from csidr */
    line_size = (csid_reg & 0x00000007U) + 0x00000004U;

    /* Number of Ways */
    num_ways = (csid_reg & 0x00001FFFU) >> 3U;
    num_ways += 0x00000001U;

    /*Number of Set*/
    num_set = (csid_reg >> 13U) & 0x00007FFFU;
    num_set += 0x00000001U;

    way_adjust = CLZ(num_ways) - (u32)0x0000001FU;

    way = 0U;
    set = 0U;

    /* Flush all the cachelines */
    for (way_index = 0U; way_index < num_ways; way_index++)
    {
        for (set_index = 0U; set_index < num_set; set_index++)
        {
            c7reg = way | set | cache_level;
            MTCPDC(CISW, c7reg); /* (Data or unified Cache line Clean and Invalidate by Set/Way) */
            set += (0x00000001U << line_size);
        }
        set = 0U;
        way += (0x00000001U << way_adjust);
    }

    /* Wait for Flush to complete */
    DSB();

    asm("tlbi 	VMALLE1\n\t"
        "dsb sy\r\n"
        "isb\n\t"); /* Invalidate cached copies of translation table entries from TLBs */
#endif
}

/**
 * @name: FCacheDCacheInvalidate
 * @msg:  Invalidate the Data cache. The contents present in the cache are
 *          cleaned and invalidated.
 */
void FCacheDCacheInvalidate(void)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* dc isw（set/way）为 EL1+，EL0 下直接返回。 */
    return;
#else
    register u32 csid_reg, c7reg;
    u32 line_size, num_ways;
    u32 way, way_index, way_adjust, set, set_index, num_set, cache_level;
    u32 current_mask;

    current_mask = MFCPSR();
    MTCPSR(current_mask | IRQ_FIQ_MASK);

    /* Number of level of cache */

    cache_level = 0U;
    /* Select cache level 1 and D cache in CSSR */
    AARCH64_WRITE_SYSREG(CSSELR_EL1, cache_level);
    ISB();

    csid_reg = (u32)AARCH64_READ_SYSREG(CCSIDR_EL1);

    /* Get the cacheline size, way size, index size from csidr */
    line_size = (csid_reg & 0x00000007U) + 0x00000004U;

    /* Number of Ways */
    num_ways = (csid_reg & 0x00001FFFU) >> 3U;
    num_ways += 0x00000001U;

    /*Number of Set*/
    num_set = (csid_reg >> 13U) & 0x00007FFFU;
    num_set += 0x00000001U;

    way_adjust = CLZ(num_ways) - (u32)0x0000001FU;

    way = 0U;
    set = 0U;

    /* Invalidate all the cachelines */
    for (way_index = 0U; way_index < num_ways; way_index++)
    {
        for (set_index = 0U; set_index < num_set; set_index++)
        {
            c7reg = way | set | cache_level;
            MTCPDC(ISW, c7reg); /* Invalidate data cache by set/way */
            set += (0x00000001U << line_size);
        }
        set = 0U;
        way += (0x00000001U << way_adjust);
    }

    /* Wait for Flush to complete */
    DSB();

    /* Select cache level 2 and D cache in CSSR */
    cache_level = (0x00000001U << 1U);
    AARCH64_WRITE_SYSREG(CSSELR_EL1, cache_level);
    ISB();

    csid_reg = (u32)AARCH64_READ_SYSREG(CCSIDR_EL1);

    /* Get the cacheline size, way size, index size from csidr */
    line_size = (csid_reg & 0x00000007U) + 0x00000004U;

    /* Number of Ways */
    num_ways = (csid_reg & 0x00001FFFU) >> 3U;
    num_ways += 0x00000001U;

    /*Number of Set*/
    num_set = (csid_reg >> 13U) & 0x00007FFFU;
    num_set += 0x00000001U;

    way_adjust = CLZ(num_ways) - (u32)0x0000001FU;

    way = 0U;
    set = 0U;

    /* Invalidate all the cachelines */
    for (way_index = 0U; way_index < num_ways; way_index++) /* Way is collection of lines */
    {
        for (set_index = 0U; set_index < num_set; set_index++) /* the same line form each way */
        {
            c7reg = way | set | cache_level;
            MTCPDC(ISW, c7reg);
            set += (0x00000001U << line_size);
        }
        set = 0U;
        way += (0x00000001U << way_adjust);
    }

    /* Wait for invalidate to complete */
    DSB();
    MTCPSR(current_mask);
#endif
}

/**
 * @name: FCacheDCacheInvalidateLine
 * @msg:  Invalidate a Data cache line. The cacheline is cleaned and
 *        invalidated.
 * @param {intptr} adr 64bit address of the data to be flushed.
 */
void FCacheDCacheInvalidateLine(intptr adr)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* dc ivac 为 EL1+ 且 dc civac（clean+invalidate）语义不匹配，直接返回。 */
    return;
#else
    u32 currmask;
    currmask = MFCPSR();
    MTCPSR(currmask | IRQ_FIQ_MASK);

    MTCPDC(IVAC, (adr & (~CACHE_LINE_ADDR_MASK))); /* Invalidate data cache by address to Point of Coherency */
    /* Wait for invalidate to complete */
    DSB();
    MTCPSR(currmask);
#endif
}

/**
 * @name: FCacheDCacheInvalidateRange
 * @msg:
 * @param {intptr} adr 64bit start address of the range to be invalidated.
 * @param {intptr} len Length of the range to be invalidated in bytes.
 * @note:
 */
void FCacheDCacheInvalidateRange(intptr adr, fsize_t len)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* dc civac（VA-based）EL0 可用，去掉 DAIF 保护。 */
    const intptr cacheline = CACHE_LINE;
    intptr end = adr + (intptr)len;
    adr = adr & (~CACHE_LINE_ADDR_MASK);
    if (len != 0U)
    {
        while (adr < end)
        {
            MTCPDC(CIVAC, adr);
            adr += cacheline;
        }
    }
    DSB();
#else
    const intptr cacheline = CACHE_LINE;
    intptr end = adr + (intptr)len;
    adr = adr & (~CACHE_LINE_ADDR_MASK);
    u32 currmask = MFCPSR();
    MTCPSR(currmask | IRQ_FIQ_MASK);
    if (len != 0U)
    {
        while (adr < end)
        {
            MTCPDC(CIVAC, adr); /* Clean and Invalidate data cache by address to Point of Coherency */
            adr += cacheline;
        }
    }
    /* Wait for invalidate to complete */
    DSB();
    MTCPSR(currmask);
#endif
}


void FCacheDCacheFlush(void)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* dc cisw（set/way）为 EL1+，EL0 下直接返回。 */
    return;
#else
    register u32 csid_reg, c7reg;
    u32 line_size, num_ways;
    u32 way, way_index, way_adjust, set, set_index, num_set, cache_level;
    u32 currmask;

    currmask = MFCPSR();
    MTCPSR(currmask | IRQ_FIQ_MASK);

    /* Number of level of cache*/
    cache_level = 0U;
    /* Select cache level 1 and D cache in CSSR */
    AARCH64_WRITE_SYSREG(CSSELR_EL1, cache_level);
    ISB();

    csid_reg = (u32)AARCH64_READ_SYSREG(CCSIDR_EL1);

    /* Get the cacheline size, way size, index size from csidr */
    line_size = (csid_reg & 0x00000007U) + 0x00000004U;

    /* Number of Ways */
    num_ways = (csid_reg & 0x00001FFFU) >> 3U;
    num_ways += 0x00000001U;

    /*Number of Set*/
    num_set = (csid_reg >> 13U) & 0x00007FFFU;
    num_set += 0x00000001U;

    way_adjust = CLZ(num_ways) - (u32)0x0000001FU;

    way = 0U;
    set = 0U;

    /* Flush all the cachelines */
    for (way_index = 0U; way_index < num_ways; way_index++)
    {
        for (set_index = 0U; set_index < num_set; set_index++)
        {
            c7reg = way | set | cache_level;
            MTCPDC(CISW, c7reg); /* (Data or unified Cache line Clean and Invalidate by Set/Way) */
            set += (0x00000001U << line_size);
        }
        set = 0U;
        way += (0x00000001U << way_adjust);
    }

    /* Wait for Flush to complete */
    DSB();

    /* Select cache level 2 and D cache in CSSR */
    cache_level = (0x00000001U << 1U);
    AARCH64_WRITE_SYSREG(CSSELR_EL1, cache_level);
    ISB();

    csid_reg = (u32)AARCH64_READ_SYSREG(CCSIDR_EL1);

    /* Get the cacheline size, way size, index size from csidr */
    line_size = (csid_reg & 0x00000007U) + 0x00000004U;

    /* Number of Ways */
    num_ways = (csid_reg & 0x00001FFFU) >> 3U;
    num_ways += 0x00000001U;

    /* Number of Sets */
    num_set = (csid_reg >> 13U) & 0x00007FFFU;
    num_set += 0x00000001U;

    way_adjust = CLZ(num_ways) - (u32)0x0000001FU;

    way = 0U;
    set = 0U;

    /* Flush all the cachelines */
    for (way_index = 0U; way_index < num_ways; way_index++)
    {
        for (set_index = 0U; set_index < num_set; set_index++)
        {
            c7reg = way | set | cache_level;
            MTCPDC(CISW, c7reg);
            set += (0x00000001U << line_size);
        }
        set = 0U;
        way += (0x00000001U << way_adjust);
    }
    /* Wait for Flush to complete */
    DSB();

    MTCPSR(currmask);
#endif
}

/**
 * @name: FCacheDCacheFlushLine
 * @msg:  Flush a Data cache line. If the byte specified by the address (adr)
 *        is cached by the Data cache, the cacheline containing that byte is
 *        invalidated. If the cacheline is modified (dirty), the entire
 *        contents of the cacheline are written to system memory before the
 *        line is invalidated.
 * @param {intptr} adr 64bit start address of the range to be flush.
 * @return {*}
 */
void FCacheDCacheFlushLine(intptr adr)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* dc cvac（VA-based）EL0 可用，去掉 DAIF 保护。 */
    MTCPDC(CVAC, (adr & (~CACHE_LINE_ADDR_MASK)));
    DSB();
#else
    u32 currmask;
    currmask = MFCPSR();
    MTCPSR(currmask | IRQ_FIQ_MASK);

    MTCPDC(CVAC, (adr & (~CACHE_LINE_ADDR_MASK))); /* Clean data cache by address to Point of Coherency. */
    /* Wait for flush to complete */
    DSB();
    MTCPSR(currmask);
#endif
}


void FCacheDCacheFlushRange(intptr adr, fsize_t len)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* dc cvac（VA-based）EL0 可用，去掉 DAIF 保护。 */
    const intptr cacheline = CACHE_LINE;
    intptr end = adr + (intptr)len;
    adr = adr & (~CACHE_LINE_ADDR_MASK);
    if (len != 0U)
    {
        while (adr < end)
        {
            MTCPDC(CVAC, adr);
            adr += cacheline;
        }
    }
    DSB();
#else
    const intptr cacheline = CACHE_LINE;
    intptr end = adr + (intptr)len;
    adr = adr & (~CACHE_LINE_ADDR_MASK);
    u32 currmask = MFCPSR();
    MTCPSR(currmask | IRQ_FIQ_MASK);
    if (len != 0U)
    {
        while (adr < end)
        {
            MTCPDC(CVAC, adr); /* Clean data cache by address to Point of Coherency */
            adr += cacheline;
        }
    }
    /* Wait for Clean to complete */
    DSB();
    MTCPSR(currmask);
#endif
}

/* Icache */

/**
 * @name: FCacheICacheEnable
 * @msg:  Enable the instruction cache.
 * @note:
 */
void FCacheICacheEnable(void)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* SCTLR_EL1/EL2 读写为特权操作，EL0 下直接返回。 */
    return;
#else
    u32 ctrl_reg;
#ifndef CONFIG_USE_EL2
    ctrl_reg = (u32)AARCH64_READ_SYSREG(SCTLR_EL1);
#else
    ctrl_reg = (u32)AARCH64_READ_SYSREG(SCTLR_EL2);
#endif

    /* enable caches only if they are disabled */
    if ((ctrl_reg & FREG_CONTROL_ICACHE_BIT) == 0x00000000U)
    {
        /* invalidate the instruction cache */
        FCacheICacheInvalidate();

        ctrl_reg |= FREG_CONTROL_ICACHE_BIT;

        /* enable the instruction cache for el1*/
#ifndef CONFIG_USE_EL2
        AARCH64_WRITE_SYSREG(SCTLR_EL1, ctrl_reg);
#else
        AARCH64_WRITE_SYSREG(SCTLR_EL2, ctrl_reg);
#endif
    }
#endif
}

/**
 * @name: FCacheICacheDisable
 * @msg:  Disable the instruction cache.
 * @note:
 */
void FCacheICacheDisable(void)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* SCTLR_EL1/EL2 读写为特权操作，EL0 下直接返回。 */
    return;
#else
    u32 ctrl_reg;
#ifndef CONFIG_USE_EL2
    ctrl_reg = (u32)AARCH64_READ_SYSREG(SCTLR_EL1);
#else
    ctrl_reg = (u32)AARCH64_READ_SYSREG(SCTLR_EL2);
#endif

    /* invalidate the instruction cache */
    FCacheICacheInvalidate();
    ctrl_reg &= ~(FREG_CONTROL_ICACHE_BIT);

    /* disable the instruction cache */
#ifndef CONFIG_USE_EL2
    AARCH64_WRITE_SYSREG(SCTLR_EL1, ctrl_reg);
#else
    AARCH64_WRITE_SYSREG(SCTLR_EL2, ctrl_reg);
#endif
#endif
}

/**
 * @name: FCacheICacheInvalidate
 * @msg:  Invalidate the entire instruction cache.
 * @note:
 */
void FCacheICacheInvalidate(void)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* ic iallu 为 EL1+，EL0 下直接返回。按 VA 操作请用 FCacheICacheInvalidateRange。 */
    return;
#else
    unsigned int currmask;
    currmask = MFCPSR();
    MTCPSR(currmask | IRQ_FIQ_MASK);
    AARCH64_WRITE_SYSREG(CSSELR_EL1, 0x1);
    DSB();
    /* invalidate the instruction cache */
    MTCPICALL(IALLU); /* Invalidate all instruction caches to Point of Unification. */
    /* Wait for invalidate to complete */
    DSB();
    MTCPSR(currmask);
#endif
}

/**
 * @name: FCacheICacheInvalidateLine
 * @msg:  Invalidate an instruction cache line. If the instruction specified
 *          by the parameter adr is cached by the instruction cache, the
 *          cacheline containing that instruction is invalidated.
 * @param {intptr} adr 64bit address of the instruction to be invalidated.
 */
void FCacheICacheInvalidateLine(intptr adr)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* ic ivau（VA-based）EL0 可用，去掉 DAIF/CSSELR；补 ISB 同步指令流。 */
    MTCPIC(IVAU, adr & (~CACHE_LINE_ADDR_MASK));
    DSB();
    ISB();
#else
    unsigned int currmask;
    currmask = MFCPSR();
    MTCPSR(currmask | IRQ_FIQ_MASK);
    AARCH64_WRITE_SYSREG(CSSELR_EL1, 0x1);
    /* invalidate the instruction cache */
    MTCPIC(IVAU, adr & (~CACHE_LINE_ADDR_MASK)); /* Invalidate instruction cache by address to Point of Unification. */
    /* Wait for invalidate to complete */
    DSB();
    MTCPSR(currmask);
#endif
}

/**
 * @name: FCacheICacheInvalidateRange
 * @msg:  Invalidate the instruction cache for the given address range.
*         If the instructions specified by the address range are cached by
*         the instrunction cache, the cachelines containing those
*         instructions are invalidated.
 * @param {intptr} adr 64bit start address of the range to be invalidated.
 * @param {intptr} len Length of the range to be invalidated in bytes.
 */
void FCacheICacheInvalidateRange(intptr adr, intptr len)
{
#if defined(CONFIG_CACHE_EL0_SAFE)
    /* ic ivau（VA-based）EL0 可用，去掉 DAIF/CSSELR；补 ISB 同步指令流。 */
    const intptr cache_line = CACHE_LINE;
    intptr end;
    intptr tempadr = adr;
    intptr tempend;
    if (len != 0x00000000U)
    {
        end = tempadr + len;
        tempend = end;
        tempadr &= ~(cache_line - 0x00000001U);
        while (tempadr < tempend)
        {
            MTCPIC(IVAU, adr & (~0x3F));
            tempadr += cache_line;
        }
    }
    DSB();
    ISB();
#else
    const intptr cache_line = CACHE_LINE;
    intptr end;
    intptr tempadr = adr;
    intptr tempend;
    u32 currmask;
    currmask = MFCPSR();
    MTCPSR(currmask | IRQ_FIQ_MASK);

    if (len != 0x00000000U)
    {
        end = tempadr + len;
        tempend = end;
        tempadr &= ~(cache_line - 0x00000001U);

        /* Select cache Level 1 I-cache in CSSR */
        AARCH64_WRITE_SYSREG(CSSELR_EL1, 0x1);
        while (tempadr < tempend)
        {
            /*Invalidate I Cache line*/
            MTCPIC(IVAU, adr & (~0x3F));

            tempadr += cache_line;
        }
    }
    /* Wait for invalidate to complete */
    DSB();
    MTCPSR(currmask);
#endif
}