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
 * FilePath: fmmu.c
 * Created Date: 2022-02-10 14:53:41
 * Last Modified: 2025-04-29 08:58:16
 * Description:  This file provides APIs for enabling/disabling MMU and setting the memory
 * attributes for sections, in the MMU translation table.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe     2021/7/3     first release
 * 1.1   huanghe     2024/06/06   add support for 4K granule size
 * 1.2   huanghe     2024/7/24    add mmu init for secondary cpu
 * 1.3   zhangyan   2024/08/15    fix misra_c_2012_rule_10_5/10_3
 * 1.4   huanghe     2025/04/22   add support for 16K/64K granule size
 */

#include "faarch.h"
#include "fcache.h"
#include <sys/errno.h>
#include "ftypes.h"
#include "fassert.h"
#include "fmmu.h"
#include "fkernel.h"
#include "fl3cache.h"

/**************************** Type Definitions *******************************/
extern const struct ArmMmuConfig code_mmu_config;
/***************** Macros (Inline Functions) Definitions *********************/

/************************** Constant Definitions *****************************/

/************************** Variable Definitions *****************************/

static struct ArmMmuPtables kernel_ptables;

/************************** Function Prototypes ******************************/
extern void AsmInvalidateTlbAll();

struct ArmMmuPtables *FMmuGetKernelPtables(void)
{
    return &kernel_ptables;
}

/* Returns a reference to a free table */
static u64 *NewTable(struct ArmMmuPtables *pt)
{
    u32 i;
    /* Look for a free table. */
    for (i = 0U; i < CONFIG_MAX_XLAT_TABLES; i++)
    {
        if (pt->ctx.xlat_use_count[i] == 0U)
        {
            pt->ctx.xlat_use_count[i] = 1U;
            return &pt->ctx.xlat_tables[i * LN_XLAT_NUM_ENTRIES];
        }
    }

    MMU_DEBUG("CONFIG_MAX_XLAT_TABLES, too small");
    return NULL;
}

static inline u32 TableIndex(struct ArmMmuPtables *pt, u64 *pte)
{
    u32 i = (pte - pt->ctx.xlat_tables) / LN_XLAT_NUM_ENTRIES;

    FASSERT_MSG(i < CONFIG_MAX_XLAT_TABLES, "table %p out of range", pte);
    return i;
}

/* Makes a table free for reuse. */
static void FreeTable(struct ArmMmuPtables *pt, u64 *table)
{
    u32 i = TableIndex(pt, table);

    MMU_DEBUG("freeing table [%d]%x\r\n", i, table);
    FASSERT_MSG(pt->ctx.xlat_use_count[i] == 2U, "table still in use");
    pt->ctx.xlat_use_count[i] = 0U;
}


static inline boolean FMmuBlcokAligned(u64 desc, u32 level_size)
{
    u64 mask = GENMASK_ULL(47, PAGE_SIZE_SHIFT);
    boolean aligned = (((desc & mask) & (level_size - 1)) == 0) ? TRUE : FALSE;

    if (!aligned)
    {
        MMU_DEBUG("misaligned desc 0x%016llx for block size 0x%x\n", desc, level_size);
    }

    return aligned;
}


/* Adjusts usage count and returns current count. */
static int TableUsage(struct ArmMmuPtables *pt, u64 *table, int adjustment)
{
    u32 i = TableIndex(pt, table);
    pt->ctx.xlat_use_count[i] += adjustment;

    FASSERT_MSG(pt->ctx.xlat_use_count[i] > 0, "usage count underflow");
    return pt->ctx.xlat_use_count[i];
}

static inline int IsTableUnused(struct ArmMmuPtables *pt, u64 *table)
{
    return TableUsage(pt, table, 0) == 2;
}

static inline int IsFreeDesc(u64 desc)
{
    return (desc & PTE_DESC_TYPE_MASK) == PTE_INVALID_DESC;
}

static inline int IsTableDesc(u64 desc, u32 level)
{
    return (level != XLAT_LAST_LEVEL) && (desc & PTE_DESC_TYPE_MASK) == PTE_TABLE_DESC;
}

static inline int IsBlockDesc(u64 desc)
{
    return (desc & PTE_DESC_TYPE_MASK) == PTE_BLOCK_DESC;
}

static inline u64 *PteDescTable(u64 desc)
{
    u64 address = desc & GENMASK_ULL(47, PAGE_SIZE_SHIFT);

    return (u64 *)address;
}

static inline int IsDescSuperset(u64 desc1, u64 desc2, u32 level)
{
    u64 mask = DESC_ATTRS_MASK | GENMASK_ULL(47, LEVEL_TO_VA_SIZE_SHIFT(level));

    return (desc1 & mask) == (desc2 & mask);
}


static void DebugShowPte(u64 *pte, u32 level)
{
#if defined(DUMP_PTE)
    MMU_DEBUG("%d ", level);
    MMU_DEBUG("%.*s", level * 2U, ". . . ");
    MMU_DEBUG("[%d]%x: ", TableIndex(pte), pte);

    if (IsFreeDesc(*pte))
    {
        MMU_DEBUG("---\r\n");
        return;
    }

    if (IsTableDesc(*pte, level))
    {
        u64 *table = PteDescTable(*pte);

        MMU_DEBUG("[Table] [%d]%x\r\n", TableIndex(pt, table), table);
        return;
    }

    if (IsBlockDesc(*pte))
    {
        MMU_DEBUG("[Block] ");
    }
    else
    {
        MMU_DEBUG("[Page] ");
    }

    uint8_t mem_type = (*pte >> 2) & MT_TYPE_MASK;

    MMU_DEBUG((mem_type == MT_NORMAL) ? "MEM" : ((mem_type == MT_NORMAL_NC) ? "NC" : "DEV"));
    MMU_DEBUG((*pte & PTE_BLOCK_DESC_AP_RO) ? "-RO" : "-RW");
    MMU_DEBUG((*pte & PTE_BLOCK_DESC_NS) ? "-NS" : "-S");
    MMU_DEBUG((*pte & PTE_BLOCK_DESC_AP_ELx) ? "-ELx" : "-ELh");
    MMU_DEBUG((*pte & PTE_BLOCK_DESC_PXN) ? "-PXN" : "-PX");
    MMU_DEBUG((*pte & PTE_BLOCK_DESC_UXN) ? "-UXN" : "-UX");
    MMU_DEBUG("\r\n");
#endif
    return;
}

static void SetPteTableDesc(u64 *pte, u64 *table, u32 level)
{
    /* Point pte to new table */
    *pte = PTE_TABLE_DESC | (u64)table;
    DebugShowPte(pte, level);
}

static void SetPteBlockDesc(u64 *pte, u64 desc, u32 level)
{
    if (desc)
    {
        desc |= (level == XLAT_LAST_LEVEL) ? PTE_PAGE_DESC : PTE_BLOCK_DESC;
    }
    *pte = desc;
    DebugShowPte(pte, level);
}

static u64 *ExpandToTable(struct ArmMmuPtables *pt, u64 *pte, u32 level)
{
    u64 *table;

    FASSERT_MSG(level < XLAT_LAST_LEVEL, "can't expand last level");

    table = NewTable(pt);
    if (!table)
    {
        return NULL;
    }

    if (!IsFreeDesc(*pte))
    {
        /*
         * If entry at current level was already populated
         * then we need to reflect that in the new table.
         */
        u64 desc = *pte;
        u32 i, stride_shift;

        MMU_DEBUG("expanding PTE 0x%016llx into table [%d]%x\r\n", desc,
                  TableIndex(pt, table), table);
        FASSERT_MSG(IsBlockDesc(desc), "desc is not vaild for expansion");

        if (level + 1 == XLAT_LAST_LEVEL)
        {
            desc |= PTE_PAGE_DESC;
        }

        stride_shift = LEVEL_TO_VA_SIZE_SHIFT(level + 1);
        for (i = 0U; i < LN_XLAT_NUM_ENTRIES; i++)
        {
            table[i] = desc | (i << stride_shift);
        }
        TableUsage(pt, table, LN_XLAT_NUM_ENTRIES);
    }
    else
    {
        /*
         * Adjust usage count for parent table's entry
         * that will no longer be free.
         */
        TableUsage(pt, pte, 1);
    }

    /* Link the new table in place of the pte it replaces */
    SetPteTableDesc(pte, table, level);
    TableUsage(pt, table, 1);

    return table;
}


static int SetMapping(struct ArmMmuPtables *pt, uintptr virt, fsize_t size, u64 desc, boolean may_overwrite)
{
    u64 *pte, *ptes[XLAT_LAST_LEVEL + 1];
    u64 level_size;
    u64 *table = pt->base_xlat_table;
    u32 level = BASE_XLAT_LEVEL;
    int ret = 0;

    while (size)
    {
        FASSERT_MSG(level <= XLAT_LAST_LEVEL,
                    "max translation table level exceeded\r\n");

        /* Locate PTE for given virtual address and page table level */
        pte = &table[XLAT_TABLE_VA_IDX(virt, level)];
        ptes[level] = pte;

        if (IsTableDesc(*pte, level))
        {
            /* Move to the next translation table level */
            level++;
            table = PteDescTable(*pte);
            continue;
        }

        if (!may_overwrite && !IsFreeDesc(*pte))
        {
            /* the entry is already allocated */
            MMU_DEBUG("entry already in use: "
                      "level %d pte %x *pte 0x%016llx",
                      level, pte, *pte);
            ret = -EBUSY;
            break;
        }

        level_size = 1ULL << LEVEL_TO_VA_SIZE_SHIFT(level);

        if (IsDescSuperset(*pte, desc, level))
        {
            /* This block already covers our range */
            level_size -= (virt & (level_size - 1));
            if (level_size > size)
            {
                level_size = size;
            }
            goto move_on;
        }

        if ((size < level_size) || (virt & (level_size - 1)) ||
            !FMmuBlcokAligned(desc, level_size)) /* Block table must be aligned */
        {

            /* Range doesn't fit, create subtable */
            table = ExpandToTable(pt, pte, level);
            if (!table)
            {
                ret = -ENOMEM;
                break;
            }
            level++;
            continue;
        }

        /* Adjust usage count for corresponding table */
        if (IsFreeDesc(*pte))
        {
            TableUsage(pt, pte, 1);
        }

        if (!desc)
        {
            TableUsage(pt, pte, -1);
        }

        /* Create (or erase) block/page descriptor */
        SetPteBlockDesc(pte, desc, level);

        /* recursively free unused tables if any */
        while (level != BASE_XLAT_LEVEL && IsTableUnused(pt, pte))
        {
            FreeTable(pt, pte);
            pte = ptes[--level];
            SetPteBlockDesc(pte, 0, level);
            TableUsage(pt, pte, -1);
        }

    move_on:
        virt += level_size;
        desc += desc ? level_size : 0;
        size -= level_size;

        /* Range is mapped, start again for next range */
        table = pt->base_xlat_table;
        level = BASE_XLAT_LEVEL;
        MMU_DEBUG("virt %p \r\n", virt);
    }

    return ret;
}

static u64 GetRegionDesc(u32 attrs)
{
    u32 mem_type;
    u64 desc = 0U;

    /* NS bit for security memory access from secure state */
    desc |= (attrs & MT_NS) ? PTE_BLOCK_DESC_NS : 0;

    /*
     * AP bits for EL0 / ELh Data access permission
     *
     *   AP[2:1]   ELh  EL0
     * +--------------------+
     *     00      RW   NA
     *     01      RW   RW
     *     10      RO   NA
     *     11      RO   RO
     */

    /* AP bits for Data access permission */
    desc |= (attrs & MT_RW) ? PTE_BLOCK_DESC_AP_RW : PTE_BLOCK_DESC_AP_RO;

    /* Mirror permissions to EL0 */
    desc |= (attrs & MT_RW_AP_ELX) ? PTE_BLOCK_DESC_AP_ELx : PTE_BLOCK_DESC_AP_EL_HIGHER;

    /* the access flag */
    desc |= PTE_BLOCK_DESC_AF;

    /* memory attribute index field */
    mem_type = MT_TYPE(attrs);
    desc |= PTE_BLOCK_DESC_MEMTYPE(mem_type);

    switch (mem_type)
    {
        case MT_DEVICE_NGNRNE:
        case MT_DEVICE_NGNRE:
        case MT_DEVICE_GRE:
            /* Access to Device memory and non-cacheable memory are coherent
             * for all observers in the system and are treated as
             * Outer shareable, so, for these 2 types of memory,
             * it is not strictly needed to set shareability field
             */
            desc |= PTE_BLOCK_DESC_OUTER_SHARE;
            /* Map device memory as execute-never */
            desc |= PTE_BLOCK_DESC_PXN;
            desc |= PTE_BLOCK_DESC_UXN;
            break;
        case MT_NORMAL_NC:
        case MT_NORMAL:
            /* Make Normal RW memory as execute never */
            if (attrs & MT_P_EXECUTE_NEVER)
            {
                desc |= PTE_BLOCK_DESC_PXN;
            }

            if (((attrs & MT_RW) && (attrs & MT_RW_AP_ELX)) || (attrs & MT_U_EXECUTE_NEVER))
            {
                desc |= PTE_BLOCK_DESC_UXN;
            }

            if (mem_type == MT_NORMAL)
            {
                desc |= PTE_BLOCK_DESC_INNER_SHARE;
            }
            else
            {
                desc |= PTE_BLOCK_DESC_OUTER_SHARE;
            }
    }

    return desc;
}


static int AddMap(struct ArmMmuPtables *pt, const char *name, uintptr phys,
                  uintptr virt, fsize_t size, u32 attrs)
{
    u64 desc = GetRegionDesc(attrs);
    boolean may_overwrite = ((attrs & MT_NO_OVERWRITE) == 0) ? TRUE : FALSE;

    MMU_DEBUG("mmap [%s]: virt %p phys %p size %p attr %p\r\n", name, virt, phys, size, desc);
    FASSERT_MSG(((virt | phys | size) & (CONFIG_MMU_PAGE_SIZE - 1)) == 0,
                "address/size are not page aligned\r\n");
    desc |= phys;
    return SetMapping(pt, virt, size, desc, may_overwrite);
}


static int RemoveMap(struct ArmMmuPtables *ptables, const char *name, uintptr virt, fsize_t size)
{
    int ret;

    MMU_DEBUG("unmmap [%s]: virt %p size %p\r\n", name, virt, size);
    FASSERT_MSG(((virt | size) & (CONFIG_MMU_PAGE_SIZE - 1)) == 0,
                "address/size are not page aligned\r\n");

    ret = SetMapping(ptables, virt, size, 0, 1);
    return ret;
}

static inline void AddArmMmuRegion(struct ArmMmuPtables *pt,
                                   const struct ArmMmuRegion *region, u32 extra_flags)
{
    if (region->size || region->attrs)
    {
        /* MMU not yet active: must use unlocked version */
        AddMap(pt, region->name, region->base_pa, region->base_va, region->size,
               region->attrs | extra_flags);
    }
}


static void SetupPageTables(struct ArmMmuPtables *pt)
{
    u32 index;
    const struct ArmMmuRegion *region;
    struct ArmMmuConfig *region_config = NULL;
    uintptr max_va = 0, max_pa = 0;

    MMU_DEBUG("xlat tables:\r\n");
    for (index = 0U; index < CONFIG_MAX_XLAT_TABLES; index++)
    {
        MMU_DEBUG("%d: %x\r\n", index, pt->ctx.xlat_tables + index * LN_XLAT_NUM_ENTRIES);
    }

    region_config = (struct ArmMmuConfig *)(&mmu_config); /* 拿到SOC配置的内存映射表定义 */
    /* 从不同的board 中获取，内存映射表中地址范围 */
    for (index = 0U; index < region_config->num_regions; index++)
    {
        region = (const struct ArmMmuRegion *)(&region_config->mmu_regions[index]);
        max_va = max(max_va, region->base_va + region->size);
        max_pa = max(max_pa, region->base_pa + region->size);
    }

    FASSERT_MSG(max_va <= (1ULL << FCPU_CONFIG_ARM64_VA_BITS),
                "Maximum VA not supported\r\n");
    FASSERT_MSG(max_pa <= (1ULL << FCPU_CONFIG_ARM64_PA_BITS),
                "Maximum PA not supported\r\n");

    /* setup translation table for execution regions */
    for (index = 0U; index < mmu_config.num_regions; index++)
    {
        region = (&mmu_config.mmu_regions[index]);
        AddArmMmuRegion(pt, region, 0);
    }
    /* setup translation table for kernel regions */
    for (index = 0U; index < code_mmu_config.num_regions; index++)
    {
        region = &code_mmu_config.mmu_regions[index];
        AddArmMmuRegion(pt, region, 0);
    }

    AsmInvalidateTlbAll();
}

/* Translation table control register settings */
u64 GetTcr(int el)
{
    u64 tcr;
    u64 va_bits = FCPU_CONFIG_ARM64_VA_BITS;
    u64 tcr_ps_bits;

    tcr_ps_bits = TCR_PS_BITS;

    if (el == 1)
    {
        tcr = (tcr_ps_bits << TCR_EL1_IPS_SHIFT);
        /*
         * TCR_EL1.EPD1: Disable translation table walk for addresses
         * that are translated using TTBR1_EL1.
         */
        tcr |= TCR_EPD1_DISABLE;
    }
    else
    {
        tcr = (tcr_ps_bits << TCR_EL3_PS_SHIFT);
    }

    tcr |= TCR_T0SZ(va_bits);
    /*
     * Translation table walk is cacheable, inner/outer WBWA and
     * inner shareable
     */
    tcr |= TCR_GRANULE_CONFIG;

    return tcr;
}

#ifndef CONFIG_USE_EL2
static void EnableMmuEl1(struct ArmMmuPtables *pt, u32 flags)
{

    /* Set MAIR, TCR and TBBR registers */
    __asm__ volatile("msr mair_el1, %0" : : "r"(MEMORY_ATTRIBUTES) : "memory", "cc");
    __asm__ volatile("msr tcr_el1, %0" : : "r"(GetTcr(1)) : "memory", "cc");
    __asm__ volatile("msr ttbr0_el1, %0"
                     :
                     : "r"((u64)pt->base_xlat_table_p)
                     : "memory", "cc");

    /* Ensure these changes are seen before MMU is enabled */
    ISB();

    /* Invalidate all data caches before enable them */
    FCacheDCacheInvalidate();

    /* Ensure the MMU enable takes effect immediately */
    ISB();

    MMU_DEBUG("MMU enabled with dcache\r\n");
}
#endif

#ifdef CONFIG_USE_EL2
static void EnableMmuEl2(struct ArmMmuPtables *pt, u32 flags)
{

    /* Set MAIR, TCR and TBBR registers */
    __asm__ volatile("msr mair_el2, %0" : : "r"(MEMORY_ATTRIBUTES) : "memory", "cc");
    __asm__ volatile("msr tcr_el2, %0" : : "r"(GetTcr(2)) : "memory", "cc");
    __asm__ volatile("msr ttbr0_el2, %0"
                     :
                     : "r"((u64)pt->base_xlat_table_p)
                     : "memory", "cc");

    /* Ensure these changes are seen before MMU is enabled */
    ISB();

    /* Invalidate all data caches before enable them */
    FCacheDCacheInvalidate();

    /* Ensure the MMU enable takes effect immediately */
    ISB();

    MMU_DEBUG("MMU enabled with dcache\r\n");
}
#endif


/* ARM MMU Driver Initial Setup */

_WEAK void FCacheL3CacheFlush(void)
{
    return;
}
_WEAK void FCacheL3CacheDisable(void)
{
    return;
}

/**
 * @name: MmuInit
 * @msg:  This function provides the default configuration mechanism for the Memory
 * Management Unit (MMU)
 * @return {*}
 */
void MmuInit(void)
{
    u32 flags = 0U;
    u64 val = 0U;

    /* 增加粒度判断 */
    val = AARCH64_READ_SYSREG(ID_AA64MMFR0_EL1);

#if defined(CONFIG_MMU_PAGE_SIZE_4K)
    FASSERT_MSG(!(val & ID_AA64MMFR0_EL1_4K_NO_SUPPORT),
                "Only 4K page size is supported\r\n");
#elif defined(CONFIG_MMU_PAGE_SIZE_16K)
    FASSERT_MSG((val & ID_AA64MMFR0_EL1_16K_SUPPORT),
                "Only 16K page size is supported\r\n");
#elif defined(CONFIG_MMU_PAGE_SIZE_64K)
    FASSERT_MSG(!(val & ID_AA64MMFR0_EL1_64K_NO_SUPPORT),
                "Only 64K page size is supported\r\n");
#endif

    /* MMU init follows the configured runtime exception level. */
    val = AARCH64_READ_SYSREG(CurrentEL);

#ifdef CONFIG_USE_EL2
    FASSERT_MSG(GET_EL(val) == MODE_EL2, "Exception level not EL2, MMU not enabled!\n");

    /* Ensure that MMU is already not enabled */
    val = AARCH64_READ_SYSREG(sctlr_el2);
#else
    FASSERT_MSG(GET_EL(val) == MODE_EL1, "Exception level not EL1, MMU not enabled!\n");

    /* Ensure that MMU is already not enabled */
    val = AARCH64_READ_SYSREG(sctlr_el1);
#endif

    FASSERT_MSG((val & SCTLR_ELx_M) == 0, "MMU is already enabled\n");

    kernel_ptables.base_xlat_table = NewTable(&kernel_ptables);
    kernel_ptables.base_xlat_table_p = kernel_ptables.base_xlat_table;

    SetupPageTables(&kernel_ptables);

#ifdef CONFIG_DISABLE_L3CACHE
    FCacheL3CacheDisable();
#else
    FCacheL3CacheFlush();
#endif

    /* Enable MMU */
#ifdef CONFIG_USE_EL2
    EnableMmuEl2(&kernel_ptables, flags);
#else
    EnableMmuEl1(&kernel_ptables, flags);
#endif
}

/**
 * @name: MmuInitSecondary
 * @msg:  This function provides the default configuration mechanism for the Secondary core
 * Memory Management Unit (MMU)
 * @return {*}
 */
void MmuInitSecondary(void)
{
    u32 flags = 0U;
    u64 val = 0U;

    /* 增加粒度判断 */
    val = AARCH64_READ_SYSREG(ID_AA64MMFR0_EL1);

#if defined(CONFIG_MMU_PAGE_SIZE_4K)
    FASSERT_MSG(!(val & ID_AA64MMFR0_EL1_4K_NO_SUPPORT),
                "Only 4K page size is supported\r\n");
#elif defined(CONFIG_MMU_PAGE_SIZE_16K)
    FASSERT_MSG((val & ID_AA64MMFR0_EL1_16K_SUPPORT),
                "Only 16K page size is supported\r\n");
#elif defined(CONFIG_MMU_PAGE_SIZE_64K)
    FASSERT_MSG(!(val & ID_AA64MMFR0_EL1_64K_NO_SUPPORT),
                "Only 64K page size is supported\r\n");
#endif

    val = AARCH64_READ_SYSREG(CurrentEL);

    /* Ensure that MMU is already not enabled */
#ifdef CONFIG_USE_EL2
    FASSERT_MSG(GET_EL(val) == MODE_EL2, "Exception level not EL2, MMU not enabled!\n");
    val = AARCH64_READ_SYSREG(sctlr_el2);
#else
    FASSERT_MSG(GET_EL(val) == MODE_EL1, "Exception level not EL1, MMU not enabled!\n");
    val = AARCH64_READ_SYSREG(sctlr_el1);
#endif

    FASSERT_MSG((val & SCTLR_ELx_M) == 0, "MMU is already enabled\n");
    FASSERT_MSG(kernel_ptables.base_xlat_table != NULL,
                "Kernel page table is not initialized\n");
    FASSERT_MSG(kernel_ptables.base_xlat_table_p != NULL,
                "Kernel page table PA is not initialized\n");

#ifdef CONFIG_DISABLE_L3CACHE
    FCacheL3CacheDisable();
#else
    FCacheL3CacheFlush();
#endif

    /* Enable MMU */
#ifdef CONFIG_USE_EL2
    EnableMmuEl2(&kernel_ptables, flags);
#else
    EnableMmuEl1(&kernel_ptables, flags);
#endif
}

static int ArchMemMap(uintptr virt, uintptr phys, fsize_t size, u32 flags)
{
    int ret = AddMap(&kernel_ptables, "dynamic", phys, virt, size, flags);

    if (ret)
    {
        return ret;
    }
    else
    {
        AsmInvalidateTlbAll();
    }

    return 0;
}

static fsize_t MemRegionAlign(uintptr *aligned_addr, fsize_t *aligned_size,
                              uintptr addr, fsize_t size, fsize_t align)
{
    fsize_t addr_offset;

    *aligned_addr = rounddown(addr, align);
    addr_offset = addr - *aligned_addr;
    *aligned_size = roundup(size +, align);

    return addr_offset;
}

/**
 * @name: FSetTlbAttributes
 * @msg:  This function sets the memory attributes for a section
 * @param {uintptr} addr is 32-bit address for which the attributes need to be set.
 * @param {fsize_t} size of the mapped memory region in bytes
 * @param {u32} attrib or the specified memory region. fmmu.h contains commonly used memory attributes definitions which can be
 *          utilized for this function.
 * @return {*}
 */
void FSetTlbAttributes(uintptr addr, fsize_t size, u32 attrib)
{
    uintptr_t aligned_phys;
    fsize_t aligned_size;
    MemRegionAlign(&aligned_phys, &aligned_size, addr, size, CONFIG_MMU_PAGE_SIZE);

    FASSERT_MSG(aligned_size != 0U, "0-length mapping at 0x%lx", aligned_phys);
    FASSERT_MSG(aligned_phys < (aligned_phys + (aligned_size - 1)),
                "wraparound for physical address 0x%lx (size %zu)", aligned_phys, aligned_size);

    MMU_DEBUG("addr %p,size %d,aligned_phys %p,aligned_size %d \r\n", addr, size,
              aligned_phys, aligned_size);

    ArchMemMap(aligned_phys, aligned_phys, aligned_size, attrib);
}


/**
 * @name: FMemMap
 * @msg: 映射虚拟地址到物理地址，设置内存属性。
 * @return {int}: 成功返回 0；失败返回错误代码。
 * @note:
 * 此函数负责将指定的虚拟地址范围映射到物理地址范围，并根据传入的标志设置内存属性。
 * 该函数会检查地址和大小是否对齐，确保它们满足页面大小的要求。
 * 
 * @param {uintptr} virt: 要映射的虚拟地址起始位置。
 * @param {uintptr} phys: 要映射的物理地址起始位置。
 * @param {fsize_t} size: 要映射的内存区域大小，单位为字节。
 * @param {u32} flags: 用于设置内存属性的标志。
 * 
 * 详细描述:
 * - 该函数首先检查虚拟地址、物理地址和大小是否都按照页面大小对齐。如果不对齐，会触发断言失败。
 * - 确保映射大小大于零，且虚拟地址和物理地址在合法范围内。
 * - 调用 `ArchMemMap` 函数执行实际的内存映射操作。
 * 
 * 例外:
 * - 如果任何参数不满足对齐要求，会触发断言失败，终止程序执行。
 * - 如果内存映射操作失败，`ArchMemMap` 函数会返回相应的错误代码。
 */
int FMmuMap(uintptr virt, uintptr phys, fsize_t size, u32 flags)
{
    /* 检查地址对齐 */
    FASSERT((virt & (CONFIG_MMU_PAGE_SIZE - 1)) == 0);
    FASSERT((phys & (CONFIG_MMU_PAGE_SIZE - 1)) == 0);
    FASSERT((size & (CONFIG_MMU_PAGE_SIZE - 1)) == 0);
    FASSERT(size > 0);
    FASSERT(virt < (1ULL << FCPU_CONFIG_ARM64_VA_BITS));
    FASSERT(phys < (1ULL << FCPU_CONFIG_ARM64_PA_BITS));

    return ArchMemMap(virt, phys, size, flags);
}


/**
 * @name: FMmuUnMap
 * @msg: 取消映射虚拟地址到物理地址。
 * @return {int}: 成功返回 0；失败返回错误代码。
 * @note:
 * 此函数负责将指定的虚拟地址范围取消映射，并根据传入的标志设置内存属性。
 * 该函数会检查地址和大小是否对齐，确保它们满足页面大小的要求。
 * 
 * @param {uintptr} virt: 要取消映射的虚拟地址起始位置。
 * @param {fsize_t} size: 要取消映射的内存区域大小，单位为字节。
 */
int FMmuUnMap(uintptr virt, fsize_t size)
{
    int ret;
    /* 检查地址对齐 */
    FASSERT((virt & (CONFIG_MMU_PAGE_SIZE - 1)) == 0);
    FASSERT((size & (CONFIG_MMU_PAGE_SIZE - 1)) == 0);
    FASSERT(size > 0);
    FASSERT(virt < (1ULL << FCPU_CONFIG_ARM64_VA_BITS));

    ret = RemoveMap(&kernel_ptables, "dynamic", (uintptr)virt, size);

    if (ret)
    {
        MMU_WRNING("RemoveMap() returned %d", ret);
        return ret;
    }
    else
    {
        AsmInvalidateTlbAll();
    }
    return 0;
}

/**
 * @name: IsAddressMapped
 * @msg:  检查指定虚拟地址范围是否已建立页表映射
 * @param {uintptr_t} addr 要检查的起始地址
 * @param {fsize_t} size 要检查的地址范围大小（字节）
 * @return {boolean} TRUE: 地址范围已完全映射, FALSE: 地址范围存在未映射页面或无效页表项
 */
boolean IsAddressMapped(uintptr_t addr, fsize_t size)
{
    struct ArmMmuPtables *pt = FMmuGetKernelPtables();
    u64 *table = pt->base_xlat_table;
    u32 level = BASE_XLAT_LEVEL;
    uintptr_t end_addr = addr + size;

    /* 按页大小对齐地址范围 */
    uintptr_t aligned_addr = PALIGN_DOWN(addr, CONFIG_MMU_PAGE_SIZE);
    uintptr_t aligned_end = PALIGN_UP(end_addr, CONFIG_MMU_PAGE_SIZE);

    /* 遍历地址范围内的每个页面 */
    for (uintptr_t page_addr = aligned_addr; page_addr < aligned_end; page_addr += CONFIG_MMU_PAGE_SIZE)
    {

        /* 重新从根页表开始，避免状态污染 */
        table = pt->base_xlat_table;
        level = BASE_XLAT_LEVEL;

        u64 *pte = &table[XLAT_TABLE_VA_IDX(page_addr, level)];

        /* 检查页表项 */
        if (IsFreeDesc(*pte))
        {
            /* 页表项为空，地址未映射 */
            return FALSE;
        }

        /* 遍历页表层次 */
        while (level < XLAT_LAST_LEVEL)
        {
            if (IsTableDesc(*pte, level))
            {
                /* 是表描述符，需要进入下一级页表 */
                table = PteDescTable(*pte);
                level++;
                pte = &table[XLAT_TABLE_VA_IDX(page_addr, level)];

                if (IsFreeDesc(*pte))
                {
                    return FALSE;
                }
            }
            else if (IsBlockDesc(*pte))
            {
                /* 是块描述符，说明已映射 */
                break;
            }
            else
            {
                /* 无效描述符 */
                return FALSE;
            }
        }

        /* 检查最后一级 */
        if (level == XLAT_LAST_LEVEL)
        {
            if (IsFreeDesc(*pte))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

/**
 * @name: FMmuGetMemoryAttribute
 * @msg:  获取指定地址范围的页表内存属性
 * @param {uintptr_t} addr 要查询的起始地址
 * @param {fsize_t} size 要查询的地址范围大小
 * @param {u64 *} attr 输出的内存属性
 * @return {boolean} TRUE: 内存属性获取成功, FALSE: 内存属性获取失败
 */
boolean FMmuGetMemoryAttribute(uintptr_t addr, fsize_t size, u64 *attr)
{
    struct ArmMmuPtables *pt = FMmuGetKernelPtables();
    u64 *table = pt->base_xlat_table;
    u32 level = BASE_XLAT_LEVEL;
    uintptr_t end_addr = addr + size;

    /* 按页大小对齐地址范围 */
    uintptr_t aligned_addr = PALIGN_DOWN(addr, CONFIG_MMU_PAGE_SIZE);
    uintptr_t aligned_end = PALIGN_UP(end_addr, CONFIG_MMU_PAGE_SIZE);

    /* 遍历地址范围内的每个页面 */
    for (uintptr_t page_addr = aligned_addr; page_addr < aligned_end; page_addr += CONFIG_MMU_PAGE_SIZE)
    {

        /* 每次页面遍历时都从根页表开始 */
        table = pt->base_xlat_table;
        level = BASE_XLAT_LEVEL;

        u64 *pte = &table[XLAT_TABLE_VA_IDX(page_addr, level)];

        /* 检查页表项 */
        if (IsFreeDesc(*pte))
        {
            /* 页表项为空，地址未映射 */
            return FALSE;
        }

        /* 遍历页表层次 */
        while (level < XLAT_LAST_LEVEL)
        {
            if (IsTableDesc(*pte, level))
            {
                /* 是表描述符，需要进入下一级页表 */
                table = PteDescTable(*pte);
                level++;
                pte = &table[XLAT_TABLE_VA_IDX(page_addr, level)];

                if (IsFreeDesc(*pte))
                {
                    return FALSE;
                }
            }
            else if (IsBlockDesc(*pte))
            {
                /* 是块描述符，获取属性 */
                *attr = (*pte & DESC_ATTRS_MASK);
                return TRUE;
            }
            else
            {
                /* 无效描述符 */
                return FALSE;
            }
        }

        /* 检查最后一级 */
        if (level == XLAT_LAST_LEVEL)
        {
            if (IsFreeDesc(*pte))
            {
                return FALSE;
            }
            *attr = (*pte & DESC_ATTRS_MASK);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief 检查是否为Normal内存类型
 */
boolean IsNormalMemoryType(uintptr_t addr, fsize_t size)
{
    u64 attrs = 0;
    if (FMmuGetMemoryAttribute(addr, size, &attrs))
    {
        u32 mem_type = MT_TYPE(attrs >> 2);
        return (mem_type == MT_NORMAL || mem_type == MT_NORMAL_NC || mem_type == MT_NORMAL_WT);
    }
    else
    {
        return FALSE;
    }
}
