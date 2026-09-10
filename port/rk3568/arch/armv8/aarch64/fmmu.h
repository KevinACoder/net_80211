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
 * FilePath: fmmu.h
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:33:43
 * Description:  This file provides APIs for enabling/disabling MMU and setting the memory
 * attributes for sections, in the MMU translation table.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * 1.0   huanghe     2021/7/3     first release
 */


#ifndef FMMU_H
#define FMMU_H

/***************************** Include Files *********************************/

#include "fprintk.h"
#include "ftypes.h"
#include "sdkconfig.h"
#include "fparameters.h"
#include "fkernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

/***************** Macros (Inline Functions) Definitions *********************/


/******************************
 *  mmu config define
 ******************************/
#define FCPU_CONFIG_ARM64_VA_BITS 48


/* 对外提供接口 */

/* More flags from user's perpective are supported using remaining bits
 * of "attrs" field, i.e. attrs[31:3], underlying code will take care
 * of setting PTE fields correctly.
 *
 * current usage of attrs[31:3] is:
 * attrs[3] : Access Permissions
 * attrs[4] : Memory access from secure/ns state
 * attrs[5] : Execute Permissions privileged mode (PXN)
 * attrs[6] : Execute Permissions unprivileged mode (UXN)
 * attrs[7] : Mirror RO/RW permissions to EL0
 * attrs[8] : Overwrite existing mapping if any
 *
 */
#define MT_PERM_SHIFT             3U /* Selects between read-only and read/write access */
#define MT_SEC_SHIFT \
    4U /* Non-secure bit. For memory accesses from Secure state, specifies whether the output address is in the Secure or Non-secure address map */
#define MT_P_EXECUTE_SHIFT \
    5U /* The Privileged execute-never bit. Determines whether the region is executable at EL1 */
#define MT_U_EXECUTE_SHIFT 6U /*  */
#define MT_RW_AP_SHIFT \
    7U /* Selects between Application level (EL0) control and the higher Exception level control. */
#define MT_NO_OVERWRITE_SHIFT 8U

#define MT_RO                 (0U << MT_PERM_SHIFT) /* Selects read-only access */
#define MT_RW                 (1U << MT_PERM_SHIFT) /* Selects read/write access */

#define MT_RW_AP_EL_HIGHER    (0U << MT_RW_AP_SHIFT) /* EL0 can't access */
#define MT_RW_AP_ELX          (1U << MT_RW_AP_SHIFT) /* EL0 can access */

#define MT_SECURE             (0U << MT_SEC_SHIFT) /* Access the Secure PA space. */
#define MT_NS                 (1U << MT_SEC_SHIFT) /* Access the Non-secure PA space. */

#define MT_P_EXECUTE          (0U << MT_P_EXECUTE_SHIFT)
#define MT_P_EXECUTE_NEVER    (1U << MT_P_EXECUTE_SHIFT)

#define MT_U_EXECUTE          (0U << MT_U_EXECUTE_SHIFT)
#define MT_U_EXECUTE_NEVER    (1U << MT_U_EXECUTE_SHIFT)

#define MT_NO_OVERWRITE       (1U << MT_NO_OVERWRITE_SHIFT)

/* MT_P_RW_U_RW: Read/Write permissions for both privileged and unprivileged levels,
 * no execution allowed in any mode. */
#define MT_P_RW_U_RW          (MT_RW | MT_RW_AP_ELX | MT_P_EXECUTE_NEVER | MT_U_EXECUTE_NEVER)
/* MT_P_RW_U_NA: Read/Write permissions for privileged level only, no access from
 * unprivileged level, no execution allowed in any mode. */
#define MT_P_RW_U_NA \
    (MT_RW | MT_RW_AP_EL_HIGHER | MT_P_EXECUTE_NEVER | MT_U_EXECUTE_NEVER)
/* MT_P_RO_U_RO: Read-Only permissions for both privileged and unprivileged levels,
 * no execution allowed in any mode. */
#define MT_P_RO_U_RO (MT_RO | MT_RW_AP_ELX | MT_P_EXECUTE_NEVER | MT_U_EXECUTE_NEVER)
/* MT_P_RO_U_NA: Read-Only permission for privileged level only, no access from
 * unprivileged level, no execution allowed in any mode. */
#define MT_P_RO_U_NA \
    (MT_RO | MT_RW_AP_EL_HIGHER | MT_P_EXECUTE_NEVER | MT_U_EXECUTE_NEVER)
/* MT_P_RO_U_RX: Read-Only for privileged, Read and Execute for unprivileged,
 * execution allowed only in unprivileged mode. */
#define MT_P_RO_U_RX     (MT_RO | MT_RW_AP_ELX | MT_P_EXECUTE_NEVER | MT_U_EXECUTE)
/* MT_P_RX_U_RX: Read and Execute permissions for both privileged and unprivileged levels. */
#define MT_P_RX_U_RX     (MT_RO | MT_RW_AP_ELX | MT_P_EXECUTE | MT_U_EXECUTE)
/* MT_P_RX_U_NA: Read and Execute permissions for privileged level, no access for
 * unprivileged level, execution allowed only in privileged mode. */
#define MT_P_RX_U_NA     (MT_RO | MT_RW_AP_EL_HIGHER | MT_P_EXECUTE | MT_U_EXECUTE_NEVER)

#define MT_P_RWX_U_NA    (MT_RW | MT_RW_AP_EL_HIGHER)

/* ELx extension */
#define MT_ELX_RW        (MT_RW | MT_RW_AP_ELX | MT_U_EXECUTE_NEVER)
#define MT_ELX_RO        (MT_RO | MT_RW_AP_ELX | MT_U_EXECUTE_NEVER)
#define MT_ELX_RX        (MT_RO | MT_RW_AP_ELX | MT_U_EXECUTE)

/* Following Memory types supported through MAIR encodings can be passed
 * by user through "attrs"(attributes) field of specified memory region.
 * As MAIR supports such 8 encodings, we will reserve attrs[2:0];
 * so that we can provide encodings upto 7 if needed in future.
 */

#define MT_DEVICE_NGNRNE 0U
#define MT_DEVICE_NGNRE  1U
#define MT_DEVICE_GRE    2U
#define MT_NORMAL_NC     3U
#define MT_NORMAL        4U
#define MT_NORMAL_WT     5U


/* Convenience macros to represent the ARMv8-A-specific
* configuration for memory access permission and
* cache-ability attribution.
*/

#define MMU_REGION_ENTRY(_name, _base_pa, _base_va, _size, _attrs)                               \
    {                                                                                            \
        .name = _name, .base_pa = _base_pa, .base_va = _base_va, .size = _size, .attrs = _attrs, \
    }

#define MMU_REGION_FLAT_ENTRY(name, adr, sz, attrs) \
    MMU_REGION_ENTRY(name, adr, adr, sz, attrs)


#ifdef CONFIG_MMU_DEBUG_PRINTS
/* To dump page table entries while filling them, set DUMP_PTE macro */
#define DUMP_PTE            1
#define MMU_DEBUG(fmt, ...) f_printk(fmt, ##__VA_ARGS__)
#else
#define MMU_DEBUG(...)
#endif

#define MMU_WRNING(fmt, ...) f_printk(fmt, ##__VA_ARGS__)


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) ((long)((sizeof(array) / sizeof((array)[0]))))
#endif


/* mmu config define */

#ifndef CONFIG_MMU_PAGE_SIZE
#define CONFIG_MMU_PAGE_SIZE 0x4000
#endif


#define MT_TYPE_MASK  0x7U
#define MT_TYPE(attr) ((attr)&MT_TYPE_MASK)

/* memory attributes */
#define MEMORY_ATTRIBUTES                                                 \
    ((0x00 << (MT_DEVICE_NGNRNE * 8)) | (0x04 << (MT_DEVICE_NGNRE * 8)) | \
     (0x0c << (MT_DEVICE_GRE * 8)) | (0x44 << (MT_NORMAL_NC * 8)) |       \
     (0xffUL << (MT_NORMAL * 8)) | (0xbbUL << (MT_NORMAL_WT * 8)))

/*
 * PTE descriptor can be Block descriptor or Table descriptor
 * or Page descriptor.
 */
#define PTE_DESC_TYPE_MASK          3U
#define PTE_BLOCK_DESC              1U
#define PTE_TABLE_DESC              3U
#define PTE_PAGE_DESC               3U
#define PTE_INVALID_DESC            0U


/*
 * Block and Page descriptor attributes fields
 */
#define PTE_BLOCK_DESC_MEMTYPE(x)   (x << 2)
#define PTE_BLOCK_DESC_NS           (1ULL << 5)
#define PTE_BLOCK_DESC_AP_ELx       (1ULL << 6)
#define PTE_BLOCK_DESC_AP_EL_HIGHER (0ULL << 6)
#define PTE_BLOCK_DESC_AP_RO        (1ULL << 7)
#define PTE_BLOCK_DESC_AP_RW        (0ULL << 7)
#define PTE_BLOCK_DESC_NON_SHARE    (0ULL << 8)
#define PTE_BLOCK_DESC_OUTER_SHARE  (2ULL << 8)
#define PTE_BLOCK_DESC_INNER_SHARE  (3ULL << 8)
#define PTE_BLOCK_DESC_AF           (1ULL << 10)
#define PTE_BLOCK_DESC_NG           (1ULL << 11)
#define PTE_BLOCK_DESC_PXN          (1ULL << 53)
#define PTE_BLOCK_DESC_UXN          (1ULL << 54)

/*
 * 48-bit address with 4KB granule size:
 *
 * +------------+------------+------------+------------+-----------+
 * | VA [47:39] | VA [38:30] | VA [29:21] | VA [20:12] | VA [11:0] |
 * +---------------------------------------------------------------+
 * |     L0     |     L1     |     L2     |     L3     | block off |
 * +------------+------------+------------+------------+-----------+
 */


/*
 * 48-bit address with 16KB granule size:
 *
 * +------------+------------+------------+------------+-----------+
 * | VA [47]    | VA [46:36] | VA [35:25] | VA [24:14] | VA [13:0] |
 * +---------------------------------------------------------------+
 * |     L0     |     L1     |     L2     |     L3     | block off |
 * +------------+------------+------------+------------+-----------+
 */


/*
 * 48-bit address with 64KB granule size:
 *
 * +------------+------------+------------+------------+-----------+
 * |     -      | VA [51:42] | VA [41:29] | VA [28:16] | VA [15:0] |
 * +---------------------------------------------------------------+
 * |     L0     |     L1     |     L2     |     L3     | block off |
 * +------------+------------+------------+------------+-----------+
 */


#if (CONFIG_MMU_PAGE_SIZE == 0x1000) /* 4KB Granule */
#define PAGE_SIZE_SHIFT 12U
#define XLAT_LAST_LEVEL 3U /* 4级页表 L0-L3 */
#define TCR_TGx_GRANULE TCR_TG0_4K
#elif (CONFIG_MMU_PAGE_SIZE == 0x4000) /* 16KB Granule */
#define PAGE_SIZE_SHIFT 14U
#define XLAT_LAST_LEVEL 3U /* 3级页表 L0-L3 */
#define TCR_TGx_GRANULE TCR_TG0_16K
#elif (CONFIG_MMU_PAGE_SIZE == 0x10000) /* 64KB Granule */
#define PAGE_SIZE_SHIFT 16U
#define XLAT_LAST_LEVEL 2U /* 2级页表 L0-L2 */
#define TCR_TGx_GRANULE TCR_TG0_64K
#else
#error "Unsupported page size configuration"
#endif

/* Only 4K granule is supported */

/* 48-bit VA address */
#define VA_SIZE_SHIFT_MAX     48U

/* The VA shift of L3 depends on the granule size */
#define L3_XLAT_VA_SIZE_SHIFT PAGE_SIZE_SHIFT
/* Number of VA bits to assign to each table (9 bits) */
#define LN_XLAT_VA_SIZE_SHIFT (PAGE_SIZE_SHIFT - 3)

/* Starting bit in the VA address for each level */
#define L2_XLAT_VA_SIZE_SHIFT (L3_XLAT_VA_SIZE_SHIFT + LN_XLAT_VA_SIZE_SHIFT) /* 21 */
#define L1_XLAT_VA_SIZE_SHIFT (L2_XLAT_VA_SIZE_SHIFT + LN_XLAT_VA_SIZE_SHIFT) /* 30 */
#define L0_XLAT_VA_SIZE_SHIFT (L1_XLAT_VA_SIZE_SHIFT + LN_XLAT_VA_SIZE_SHIFT) /* 39 */

#define LEVEL_TO_VA_SIZE_SHIFT(level) \
    (PAGE_SIZE_SHIFT + (LN_XLAT_VA_SIZE_SHIFT * (XLAT_LAST_LEVEL - (level)))) /* 12 + (9*(3-level)) */

/* Number of entries for each table (512) */
#define LN_XLAT_NUM_ENTRIES ((1U << PAGE_SIZE_SHIFT) / 8U)


#define TCR_GRANULE_CONFIG \
    (TCR_TGx_GRANULE | TCR_SHARED_INNER | TCR_ORGN_WBWA | TCR_IRGN_WBWA)

/* Virtual Address Index within a given translation table level */
#define XLAT_TABLE_VA_IDX(va_addr, level) \
    ((va_addr >> LEVEL_TO_VA_SIZE_SHIFT(level)) & (LN_XLAT_NUM_ENTRIES - 1))

/*
 * Calculate the initial translation table level from FCPU_CONFIG_ARM64_VA_BITS
 * For a 4 KB page size:
 *
 * (va_bits <= 21)   - base level 3
 * (22 <= va_bits <= 30) - base level 2
 * (31 <= va_bits <= 39) - base level 1
 * (40 <= va_bits <= 48) - base level 0
 */


#if CONFIG_MMU_PAGE_SIZE == 0x1000
#define GET_BASE_XLAT_LEVEL(va_bits) \
    ((va_bits > 39) ? 0U : (va_bits > 30) ? 1U : (va_bits > 21) ? 2U : 3U)

#elif CONFIG_MMU_PAGE_SIZE == 0x4000
#define GET_BASE_XLAT_LEVEL(va_bits) \
    ((va_bits > 47) ? 0U : (va_bits > 36) ? 1U : (va_bits > 25) ? 2U : 3U)

#elif CONFIG_MMU_PAGE_SIZE == 0x10000
#define GET_BASE_XLAT_LEVEL(va_bits) ((va_bits > 42) ? 0U : (va_bits > 29) ? 1U : 2U)

#endif

/* Level for the base XLAT */
#define BASE_XLAT_LEVEL GET_BASE_XLAT_LEVEL(FCPU_CONFIG_ARM64_VA_BITS)

#if (FCPU_CONFIG_ARM64_PA_BITS == 48)
#define TCR_PS_BITS TCR_PS_BITS_256TB
#elif (FCPU_CONFIG_ARM64_PA_BITS == 44)
#define TCR_PS_BITS TCR_PS_BITS_16TB
#elif (FCPU_CONFIG_ARM64_PA_BITS == 42)
#define TCR_PS_BITS TCR_PS_BITS_4TB
#elif (FCPU_CONFIG_ARM64_PA_BITS == 40)
#define TCR_PS_BITS TCR_PS_BITS_1TB
#elif (FCPU_CONFIG_ARM64_PA_BITS == 36)
#define TCR_PS_BITS TCR_PS_BITS_64GB
#else
#define TCR_PS_BITS TCR_PS_BITS_4GB
#endif

/* Upper and lower attributes mask for page/block descriptor */
#define DESC_ATTRS_UPPER_MASK GENMASK_ULL(63, 51)
#define DESC_ATTRS_LOWER_MASK GENMASK_ULL(11, 2)

#define DESC_ATTRS_MASK       (DESC_ATTRS_UPPER_MASK | DESC_ATTRS_LOWER_MASK)

#define SCTLR_M_BIT           BIT(0)
#define SCTLR_A_BIT           BIT(1)
#define SCTLR_C_BIT           BIT(2)
#define SCTLR_SA_BIT          BIT(3)
#define SCTLR_I_BIT           BIT(12)

/*
 * TCR definitions.
 */
#define TCR_EL1_IPS_SHIFT     32U
#define TCR_EL2_PS_SHIFT      16U
#define TCR_EL3_PS_SHIFT      16U

#define TCR_T0SZ_SHIFT        0U
#define TCR_T0SZ(x)           ((64 - (x)) << TCR_T0SZ_SHIFT)

#define TCR_IRGN_NC           (0ULL << 8)
#define TCR_IRGN_WBWA         (1ULL << 8)
#define TCR_IRGN_WT           (2ULL << 8)
#define TCR_IRGN_WBNWA        (3ULL << 8)
#define TCR_IRGN_MASK         (3ULL << 8)
#define TCR_ORGN_NC           (0ULL << 10)
#define TCR_ORGN_WBWA         (1ULL << 10)
#define TCR_ORGN_WT           (2ULL << 10)
#define TCR_ORGN_WBNWA        (3ULL << 10)
#define TCR_ORGN_MASK         (3ULL << 10)
#define TCR_SHARED_NON        (0ULL << 12)
#define TCR_SHARED_OUTER      (2ULL << 12)
#define TCR_SHARED_INNER      (3ULL << 12)
#define TCR_TG0_4K            (0ULL << 14)
#define TCR_TG0_64K           (1ULL << 14)
#define TCR_TG0_16K           (2ULL << 14)
#define TCR_EPD1_DISABLE      (1ULL << 23)

#define TCR_PS_BITS_4GB       0x0ULL
#define TCR_PS_BITS_64GB      0x1ULL
#define TCR_PS_BITS_1TB       0x2ULL
#define TCR_PS_BITS_4TB       0x3ULL
#define TCR_PS_BITS_16TB      0x4ULL
#define TCR_PS_BITS_256TB     0x5ULL

/*
 * Caching mode definitions. These are mutually exclusive.
 */

/** No caching. Most drivers want this. */
#define K_MEM_CACHE_NONE      2

/** Write-through caching. Used by certain drivers. */
#define K_MEM_CACHE_WT        1

/** Full write-back caching. Any RAM mapped wants this. */
#define K_MEM_CACHE_WB        0

/** Reserved bits for cache modes in k_map() flags argument */
#define K_MEM_CACHE_MASK      (BIT(3) - 1)

/*
 * Region permission attributes. Default is read-only, no user, no exec
 */

/** Region will have read/write access (and not read-only) */
#define K_MEM_PERM_RW         BIT(3)

/** Region will be executable (normally forbidden) */
#define K_MEM_PERM_EXEC       BIT(4)

/** Region will be accessible to user mode (normally supervisor-only) */
#define K_MEM_PERM_USER       BIT(5)

/**************************** Type Definitions *******************************/

/* Region definition data structure */
struct ArmMmuRegion
{
    /* Region Base Physical Address */
    uintptr_t base_pa;
    /* Region Base Virtual Address */
    uintptr_t base_va;
    /* Region size */
    size_t size;
    /* Region Name */
    const char *name;
    /* Region Attributes */
    uint32_t attrs;
};

/* MMU configuration data structure */
struct ArmMmuConfig
{
    /* Number of regions */
    unsigned int num_regions;
    /* Regions */
    const struct ArmMmuRegion *mmu_regions;
};

typedef struct FMmuContext
{
    u64 xlat_tables[CONFIG_MAX_XLAT_TABLES * LN_XLAT_NUM_ENTRIES] __aligned(LN_XLAT_NUM_ENTRIES *
                                                                            sizeof(u64));
    u16 xlat_use_count[CONFIG_MAX_XLAT_TABLES];
} FMmuContext;

typedef union
{
    u16 asid;
    u16 vmid;
} FMmuId;


struct ArmMmuPtables
{
    FMmuContext ctx;
    uint64_t *base_xlat_table;   /* 页表虚拟地址，用于CPU访问 */
    uint64_t *base_xlat_table_p; /* 页表物理地址 */
    FMmuId id;
};

/* Reference to the MMU configuration.
*
* This struct is defined and populated for each SoC (in the SoC definition),
* and holds the build-time configuration information for the fixed MMU
* regions enabled during kernel initialization.
*/
extern const struct ArmMmuConfig mmu_config;

void MmuInit(void);
void MmuInitSecondary(void);

void FSetTlbAttributes(uintptr addr, fsize_t size, u32 attrib);

int FMmuMap(uintptr virt, uintptr phys, fsize_t size, u32 flags);

int FMmuUnMap(uintptr virt, fsize_t size);


int FMmuPrintReconstructedTables(int print_detailed);
void DisplayMmuUsage(void);

void FMmuMapRetopology(void);

fsize_t FMmuGetMappingsByPA(uintptr_t pa, struct ArmMmuRegion *mmu_region_out, fsize_t max_mappings);
fsize_t FMmuGetMappingByVA(uintptr_t va, struct ArmMmuRegion *mmu_region_out);


fsize_t FMmuGetAllVaByPA(uintptr_t pa, uintptr_t *va_out, fsize_t max_va);
fsize_t FMmuGetPAByVA(uintptr_t va, uintptr_t *pa_out);

boolean IsAddressMapped(uintptr_t addr, fsize_t size);
boolean FMmuGetMemoryAttribute(uintptr_t addr, fsize_t size, u64 *attr);
boolean IsNormalMemoryType(uintptr_t addr, fsize_t size);

#ifdef __cplusplus
}
#endif

#endif
