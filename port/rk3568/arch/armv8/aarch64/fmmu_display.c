/*
 * Copyright (C) 2025, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fmmu_display.c
 * Created Date: 2025-04-21 15:27:32
 * Last Modified: 2025-04-28 17:56:41
 * Description:  This file is for display the mapping information of the FMMU.
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0   huanghe    2025/04/22       init commit
 */
#include "fmmu.h"
#include <stdio.h>
#include "faarch.h"
#include "fkernel.h"
#include <stdlib.h>
#include "fassert.h"


struct MappingInfo
{
    uintptr start_virt;
    uintptr end_virt;
    uintptr start_phys;
    uintptr end_phys;
    fsize_t size;
    u64 attrs;
    fsize_t count;
};

static char formatted_attr_str[300];
static struct MappingInfo all_mappings[CONFIG_MAX_XLAT_TABLES * LN_XLAT_NUM_ENTRIES];
static fsize_t map_num = 0;
static struct MappingInfo compressed_mappings[1000];
extern struct ArmMmuPtables *FMmuGetKernelPtables(void);


int ArchPagePhysGet(uintptr virt, uintptr *phys)
{
    u64 par;

    __asm__ volatile("at S1E1R, %0" : : "r"(virt));
    ISB();
    par = AARCH64_READ_SYSREG(PAR_EL1);

    if (par & BIT(0))
    {
        return -1;
    }

    if (phys)
    {
        *phys = par & GENMASK_ULL(47, 12);
    }
    return 0;
}


static fsize_t FMmuCompressMappingInfo(struct MappingInfo *mappings, fsize_t count,
                                       struct MappingInfo *compressed)
{
    fsize_t compressed_count = 0;

    for (fsize_t i = 0; i < count; i++)
    {
        if (compressed_count == 0 ||
            mappings[i].attrs != compressed[compressed_count - 1].attrs ||
            mappings[i].size != compressed[compressed_count - 1].size ||
            mappings[i].start_phys != compressed[compressed_count - 1].end_phys + 1 ||
            mappings[i].start_virt != compressed[compressed_count - 1].end_virt + 1)
        {

            compressed[compressed_count] = mappings[i];
            compressed[compressed_count].end_virt = mappings[i].start_virt + mappings[i].size - 1;
            compressed[compressed_count].end_phys = mappings[i].start_phys + mappings[i].size - 1;
            compressed[compressed_count].count = 1;
            compressed_count++;
        }
        else
        {
            compressed[compressed_count - 1].end_virt = mappings[i].start_virt +
                                                        mappings[i].size - 1;
            compressed[compressed_count - 1].end_phys = mappings[i].start_phys +
                                                        mappings[i].size - 1;
            compressed[compressed_count - 1].count++;
        }
    }

    return compressed_count;
}


static void FMmuCollectMappingInfo(u64 *table, u32 level, uintptr_t va_start,
                                   struct MappingInfo *info, fsize_t *count)
{
    for (fsize_t i = 0; i < LN_XLAT_NUM_ENTRIES; i++)
    {
        u64 *pte = &table[i];
        if ((*pte & PTE_DESC_TYPE_MASK) == PTE_INVALID_DESC)
        {
            continue;
        }

        uintptr_t va = va_start + (i << LEVEL_TO_VA_SIZE_SHIFT(level));
        if (level == XLAT_LAST_LEVEL || (*pte & PTE_DESC_TYPE_MASK) == PTE_BLOCK_DESC)
        {
            info[*count].start_virt = va;
            info[*count].start_phys = *pte & GENMASK_ULL(47, 12);
            info[*count].size = 1ULL << LEVEL_TO_VA_SIZE_SHIFT(level);
            info[*count].attrs = *pte & ~GENMASK_ULL(47, 12); /* Simplified attributes extraction */
            info[*count].end_virt = va + info[*count].size - 1;
            info[*count].end_phys = info[*count].start_phys + info[*count].size - 1;
            info[*count].count = 1;
            (*count)++;
        }
        else if ((*pte & PTE_DESC_TYPE_MASK) == PTE_TABLE_DESC)
        {
            u64 *next_table = (u64 *)(*pte & GENMASK_ULL(47, 12));
            FMmuCollectMappingInfo(next_table, level + 1, va, info, count);
        }
    }
}


static void FMmuReverseSetMapping(struct MappingInfo *info, fsize_t *count)
{
    struct ArmMmuPtables *pt = FMmuGetKernelPtables();
    *count = 0;
    FMmuCollectMappingInfo(pt->ctx.xlat_tables, 0, 0, info, count);
}


static const char *FMmuFormatAttributes(u64 attrs)
{
    const char *mem_type, *shareability, *priv_rw, *user_rw, *priv_exec, *user_exec;

    switch ((attrs >> 2) & MT_TYPE_MASK)
    {
        case 0:
            mem_type = "device nGnRnE";
            break;
        case 1:
            mem_type = "device nGnRE";
            break;
        case 2:
            mem_type = "device GRE";
            break;
        case 3:
            mem_type = "normal NC";
            break;
        case 4:
            if ((attrs & PTE_BLOCK_DESC_INNER_SHARE) && (attrs & PTE_BLOCK_DESC_OUTER_SHARE))
            {
                mem_type = "write-back/read-write-allocate";
            }
            else if (attrs & PTE_BLOCK_DESC_INNER_SHARE)
            {
                mem_type = "write-back/no-write-allocate";
            }
            else if (attrs & PTE_BLOCK_DESC_OUTER_SHARE)
            {
                mem_type = "write-through/read-write-allocate";
            }
            else
            {
                mem_type = "write-through/no-write-allocate";
            }
            break;
        case 5:
            mem_type = "normal WT";
            break;
        default:
            mem_type = "Unknown";
    }

    /* Determine shareability*/
    if ((attrs & (3ULL << 8)) == (2ULL << 8))
    {
        shareability = "out";
    }
    else if ((attrs & (3ULL << 8)) == (3ULL << 8))
    {
        shareability = "inner";
    }
    else
    {
        shareability = "none";
    }

    /* Determine permissions */
    priv_exec = (attrs & PTE_BLOCK_DESC_PXN) ? "xn" : "exec";
    user_exec = (attrs & PTE_BLOCK_DESC_UXN) ? "xn" : "exec";
    if ((PTE_BLOCK_DESC_AP_RO & attrs) == PTE_BLOCK_DESC_AP_RO)
    {
        priv_rw = "readonly";
        user_rw = "readonly";
    }
    else
    {
        priv_rw = "readwrite";
        user_rw = "readwrite";
    }
    if ((PTE_BLOCK_DESC_AP_ELx & attrs) == 0)
    {
        user_rw = "noaccess";
    }

    snprintf(formatted_attr_str, sizeof(formatted_attr_str), "P:%s U:%s P:%s U:%s no %s  %s",
             priv_rw, user_rw, priv_exec, user_exec, shareability, mem_type);

    return formatted_attr_str;
}


/**
 * @name: FMmuPrintReconstructedTables
 * @msg: 打印重构的内存映射表信息。根据 print_detailed 参数的值，此函数可以打印简略或详细的映射信息。
 * @return {void}: 此函数不返回任何值。
 * @note: 打印的信息包括虚拟地址范围、物理地址范围、安全性、尺寸、权限等。
 * @param {int} print_detailed: 控制打印的详细程度。如果为0，打印压缩后的映射信息；如果非0，打印所有映射的详细信息。
 *
 * 该函数首先调用 FMmuReverseSetMapping 以填充映射信息到全局变量 all_mappings 中。
 * 根据 print_detailed 参数的值，函数决定打印的详细级别：
 * - 如果 print_detailed == 0，调用 FMmuCompressMappingInfo 压缩映射信息，并打印压缩后的结果。
 * - 如果 print_detailed != 0，直接遍历 all_mappings 并打印每个映射的详细信息。
 * 打印的格式包括地址范围、尺寸和权限等，可用于调试或监控内存映射状态。
 */
int FMmuPrintReconstructedTables(int print_detailed)
{
    fsize_t count = 0;
    FMmuReverseSetMapping(all_mappings, &count);
    printf("address                              physical                              "
           " sec  d   size        permissions                              glb  shr  "
           "pageflags (remapped)\n\n");
    if (print_detailed == 0)
    {
        fsize_t compressed_count = FMmuCompressMappingInfo(all_mappings, count, compressed_mappings);

        for (fsize_t i = 0; i < compressed_count; i++)
        {

            printf("N:%016lx--%016lx  AN:%016lx--%016lx  ns   yes  %08lx   %s (%zu "
                   "entries)\n",
                   compressed_mappings[i].start_virt, compressed_mappings[i].end_virt,
                   compressed_mappings[i].start_phys, compressed_mappings[i].end_phys,
                   compressed_mappings[i].size,
                   FMmuFormatAttributes(compressed_mappings[i].attrs),
                   compressed_mappings[i].count);
        }
    }
    else
    {
        for (fsize_t i = 0; i < count; i++)
        {
            printf("N:%016lx--%016lx  AN:%016lx--%016lx  ns   yes  %08lx   %s (%zu "
                   "entries)\n",
                   all_mappings[i].start_virt, all_mappings[i].end_virt,
                   all_mappings[i].start_phys, all_mappings[i].end_phys, all_mappings[i].size,
                   FMmuFormatAttributes(all_mappings[i].attrs), all_mappings[i].count);
        }
    }
    return 0;
}


static void FMmuMapping2Region(struct MappingInfo *mappings, struct ArmMmuRegion *mmu_region_out)
{
    mmu_region_out->attrs = 0;

    if (mappings->attrs & PTE_BLOCK_DESC_PXN)
    {
        mmu_region_out->attrs |= MT_P_EXECUTE_NEVER;
    }

    if (mappings->attrs & PTE_BLOCK_DESC_NS)
    {
        mmu_region_out->attrs |= MT_NS;
    }


    if (mappings->attrs & PTE_BLOCK_DESC_UXN)
    {
        mmu_region_out->attrs |= MT_U_EXECUTE_NEVER;
    }

    if ((PTE_BLOCK_DESC_AP_RO & mappings->attrs) == PTE_BLOCK_DESC_AP_RO)
    {
        mmu_region_out->attrs |= (MT_RO);
    }
    else
    {
        mmu_region_out->attrs |= (MT_RW);
    }


    if ((PTE_BLOCK_DESC_AP_ELx & mappings->attrs) == 0)
    {
        mmu_region_out->attrs |= MT_RW_AP_EL_HIGHER;
    }
    else
    {
        mmu_region_out->attrs |= MT_RW_AP_ELX;
    }

    /* Region Base Physical Address */
    mmu_region_out->base_pa = mappings->start_phys;
    /* Region Base Virtual Address */
    mmu_region_out->base_va = mappings->start_virt;
    /* Region size */
    mmu_region_out->size = mappings->size;


    switch ((mappings->attrs >> 2) & MT_TYPE_MASK)
    {
        case 0:
            {
                mmu_region_out->attrs |= MT_DEVICE_NGNRNE;
                break;
            }
        case 1:
            {
                mmu_region_out->attrs |= MT_DEVICE_NGNRE;
                break;
            }
        case 2:
            {
                mmu_region_out->attrs |= MT_DEVICE_GRE;
                break;
            }
        case 3:
            {
                mmu_region_out->attrs |= MT_NORMAL_NC;
                break;
            }
        case 4:
            mmu_region_out->attrs |= MT_NORMAL;
            break;
        case 5:
            {
                mmu_region_out->attrs |= MT_NORMAL_WT;
            }
            break;
        default:;
    }
}


/**
 * @name: FMmuGetMappingsByPA
 * @msg: 根据物理地址查找所有对应的内存映射，并将它们的信息输出到一个数组中。
 * @return {fsize_t}: 返回找到的映射数量。
 * @note: 此函数检索的映射数量可能受到 max_mappings 参数的限制。当找到的映射数量超过 max_mappings 时，
 *        不会继续保存更多的映射信息，但函数会继续计算总映射数量并返回。
 * @param {uintptr_t} pa: 要查询的物理地址。
 * @param {struct ArmMmuRegion*} mmu_region_out: 输出参数，一个数组，用于存储找到的映射的区域信息。
 * @param {fsize_t} max_mappings: mmu_region_out 数组可以容纳的最大映射数。
 *
 * 此函数遍历预先设定的所有映射(all_mappings)，查找所有包含指定物理地址的映射区域。
 * 对于每一个符合条件的映射，如果已经找到的映射数量少于 max_mappings，它会调用 FMmuMapping2Region
 * 函数将映射信息转换为区域信息并保存到 mmu_region_out 数组中。最终，函数返回所有符合条件的映射数量。
 */

fsize_t FMmuGetMappingsByPA(uintptr_t pa, struct ArmMmuRegion *mmu_region_out, fsize_t max_mappings)
{
    fsize_t mapping_count = 0;

    for (fsize_t i = 0; i < map_num; i++)
    {
        if (pa >= all_mappings[i].start_phys && pa <= all_mappings[i].end_phys)
        {
            if (mapping_count < max_mappings)
            {
                FMmuMapping2Region(&all_mappings[i], &mmu_region_out[mapping_count]);
            }
            mapping_count++;
        }
    }

    return mapping_count;
}

void FMmuMapRetopology(void)
{
    FMmuReverseSetMapping(all_mappings, &map_num);
}


static int CompareMappings(const void *a, const void *b)
{
    struct MappingInfo *infoa = (struct MappingInfo *)a;
    struct MappingInfo *infob = (struct MappingInfo *)b;
    if (infoa->start_virt < infob->start_virt)
    {
        return -1;
    }
    else if (infoa->start_virt > infob->start_virt)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @name: FMmuGetMappingByVA
 * @msg: 查找给定虚拟地址对应的内存映射区域。
 * @return {fsize_t}: 如果找到对应的映射区域，返回1；如果没有找到，返回0。
 * @note: 此函数依赖于`all_mappings`数组和`FMmuReverseSetMapping`函数来预先填充所有可能的映射。
 * @param {uintptr_t} va: 要查询的虚拟地址。
 * @param {struct ArmMmuRegion*} mmu_region_out: 指针，用于输出找到的映射区域的详细信息。
 *
 * 此函数遍历所有的映射，查找包含指定虚拟地址的映射。如果找到，使用`FMmuMappingfo2Region`函数将映射信息转换为更详细的区域信息，然后存储在`mmu_region_out`中。
 */


fsize_t FMmuGetMappingByVA(uintptr_t va, struct ArmMmuRegion *mmu_region_out)
{
    /* Ensure all_mappings is sorted by start_virt */
    static int is_sorted = 0;
    if (!is_sorted)
    {
        qsort(all_mappings, map_num, sizeof(struct MappingInfo), CompareMappings);
        is_sorted = 1;
    }

    /* Perform binary search to find the mapping */
    int left = 0, right = (int)(map_num - 1);
    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        struct MappingInfo *midMapping = &all_mappings[mid];

        if (va >= midMapping->start_virt && va <= midMapping->end_virt)
        {
            FMmuMapping2Region(midMapping, mmu_region_out);
            return 1;
        }
        else if (va < midMapping->start_virt)
        {
            right = mid - 1;
        }
        else
        {
            left = mid + 1;
        }
    }

    return 0;
}


static void FMmuCalculateRemainingMemorySpace(u64 *remaining_tables, u64 *remaining_space_bytes)
{
    u64 free_entries = 0;
    struct ArmMmuPtables *pt = FMmuGetKernelPtables();
    u16 *xlat_use_count = pt->ctx.xlat_use_count;
    for (u32 i = 0; i < sizeof(pt->ctx.xlat_use_count) / sizeof(pt->ctx.xlat_use_count[0]); i++)
    {
        free_entries += (LN_XLAT_NUM_ENTRIES + 2 - xlat_use_count[i]); /* 此处+2 的原因为：NewTable 调用时xlat_use_count 的默认值为1 ，分配NewTable （ExpandToTable）的时候还会再次+1 */
    }

    *remaining_space_bytes = free_entries * (1ULL << PAGE_SIZE_SHIFT);
    *remaining_tables = free_entries;
}


/* mmu table usage */
/**
 * @name: DisplayMmuUsage
 * @msg: 打印当前系统内存管理单元(MMU)剩余的表和剩余的内存空间。
 * @return {void}: 此函数不返回任何值。
 * @note: 此函数依赖于`FMmuCalculateRemainingMemorySpace`来获取剩余的表和内存空间信息。
 *        输出是以控制台的形式显示，其中内存空间还被转换为兆字节(MB)单位以便阅读。
 *
 * 该函数首先调用`FMmuCalculateRemainingMemorySpace`以获取剩余的表格数量和剩余的内存空间大小，
 * 然后将这些信息打印到控制台。这可以帮助管理员或开发者快速了解系统的内存使用情况。
 */
void DisplayMmuUsage(void)
{
    u64 remaining_tables = 0;
    u64 remaining_memory_space = 0;
    FMmuCalculateRemainingMemorySpace(&remaining_tables, &remaining_memory_space);
    printf("Remaining tables: %lu\n", remaining_tables);
    printf("Remaining memory space: %lu bytes (%lu MB)\n", remaining_memory_space,
           remaining_memory_space / 1024 / 1024);
}


/**
 * @name: FMmuGetAllVaByPA
 * @msg: 获取给定物理地址的所有对应虚拟地址。
 * @return {fsize_t}: 返回找到的虚拟地址数量。
 * @note: 
 * 此函数会遍历所有映射表，查找包含指定物理地址的所有映射区域，
 * 并计算每个映射区域对应的虚拟地址，返回找到的虚拟地址数量。
 * 
 * @param {uintptr_t} pa: 要查询的物理地址。
 * @param {uintptr_t} *va_out: 输出参数，存储找到的虚拟地址的数组。
 * @param {fsize_t} max_va: va_out 数组的最大容量。
 */
fsize_t FMmuGetAllVaByPA(uintptr_t pa, uintptr_t *va_out, fsize_t max_va)
{

    fsize_t mapping_count = 0;
    for (fsize_t i = 0; i < map_num; i++)
    {
        if (pa >= all_mappings[i].start_phys && pa <= all_mappings[i].end_phys)
        {
            if (mapping_count < max_va)
            {
                va_out[mapping_count] = all_mappings[i].start_virt +
                                        (pa - all_mappings[i].start_phys);
            }

            mapping_count++;
        }
    }

    return mapping_count;
}


/**
 * @name: FMmuGetPAByVA
 * @msg: 获取给定虚拟地址对应的物理地址。
 * @return {fsize_t}: 成功返回 1；失败返回 0。
 * @note: 
 * 此函数会遍历所有映射表，查找包含指定虚拟地址的映射区域，
 * 并计算对应的物理地址，返回成功或失败状态。
 * 
 * @param {uintptr_t} va: 要查询的虚拟地址。
 * @param {uintptr_t} *pa_out: 输出参数，存储找到的物理地址。
 */
fsize_t FMmuGetPAByVA(uintptr_t va, uintptr_t *pa_out)
{
    struct ArmMmuRegion mmu_region;

    if (FMmuGetMappingByVA(va, &mmu_region) == 1)
    {
        *pa_out = mmu_region.base_pa + (va - mmu_region.base_va);
        return 1; /* 成功 */
    }

    return 0; /* 没有找到对应的物理地址 */
}
