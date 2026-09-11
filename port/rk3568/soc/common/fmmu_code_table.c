/*
 * Copyright (C) 2024, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fmmu_code_table.c
 * Created Date: 2024-02-21 08:55:44
 * Last Modified: 2024-11-27 15:38:35
 * Description:  This file is for
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 */

#include "sdkconfig.h"
#include "fmmu.h"

extern char _image_ram_start[];
extern char _image_ram_end[];
extern char _image_ram_size[];

extern char __text_region_start[];
extern char __text_region_end[];
extern char __text_region_size[];


extern char __rodata_region_start[];
extern char __rodata_region_end[];
extern char __rodata_region_size[];


#ifndef CONFIG_USE_EL2


const struct ArmMmuRegion code_mmu_regions[] = {
    MMU_REGION_FLAT_ENTRY("CODE_TEXT", (uintptr_t)__text_region_start,
                          (uintptr_t)__text_region_size, MT_NORMAL | MT_P_RX_U_NA | MT_NS),

    MMU_REGION_FLAT_ENTRY("CODE_RODE", (uintptr_t)__rodata_region_start,
                          (uintptr_t)__rodata_region_size, MT_NORMAL | MT_P_RO_U_NA | MT_NS),


    MMU_REGION_FLAT_ENTRY("CODE_RAM", (uintptr_t)_image_ram_start,
                          (uintptr_t)_image_ram_size, MT_NORMAL | MT_P_RW_U_NA | MT_NS),

};

#else
const struct ArmMmuRegion code_mmu_regions[] = {
    MMU_REGION_FLAT_ENTRY("CODE_TEXT", (uintptr_t)__text_region_start,
                          (uintptr_t)__text_region_size, MT_NORMAL | MT_ELX_RX | MT_NS),

    MMU_REGION_FLAT_ENTRY("CODE_RODE", (uintptr_t)__rodata_region_start,
                          (uintptr_t)__rodata_region_size, MT_NORMAL | MT_ELX_RO | MT_NS),


    MMU_REGION_FLAT_ENTRY("CODE_RAM", (uintptr_t)_image_ram_start,
                          (uintptr_t)_image_ram_size, MT_NORMAL | MT_ELX_RW | MT_NS),

};

#endif

const uint32_t code_mmu_regions_size = ARRAY_SIZE(code_mmu_regions);

const struct ArmMmuConfig code_mmu_config = {
    .num_regions = code_mmu_regions_size,
    .mmu_regions = code_mmu_regions,
};
