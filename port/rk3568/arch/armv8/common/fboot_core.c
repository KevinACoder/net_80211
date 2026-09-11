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
 * FilePath: fboot_core.c
 * Created Date: 2024-08-27 17:04:54
 * Last Modified: 2024-12-02 11:54:10
 * Description:  This file is for
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0    LiuSM    2024-08-27 17:04:54  First version
 */

#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#ifndef SDK_CONFIG_H__
#warning "Please include sdkconfig.h"
#endif
#include "felf.h"
#include "ferror_code.h"
#include "fparameters.h"
#include "fcpu_info.h"
#include "fimage_info.h"
#include "fpsci.h"
#include "fl3cache.h"
#include "fsleep.h"
#include "fdebug.h"
#include "fmmu.h"
#include "fcache.h"

#define FBOOT_DEBUG_TAG "FBOOT_CORE"
#define FBOOT_ERROR(format, ...) \
    FT_DEBUG_PRINT_E(FBOOT_DEBUG_TAG, format, ##__VA_ARGS__)
#define FBOOT_DEBUG_D(format, ...) \
    FT_DEBUG_PRINT_D(FBOOT_DEBUG_TAG, format, ##__VA_ARGS__)

#define ERR_ELF_LOAD_SUCCESS FT_MAKE_ERRCODE(ErrorModGeneral, ErrElf, 0)
#define ERR_ELF_LOAD_FAIL    FT_MAKE_ERRCODE(ErrorModGeneral, ErrElf, 1)
#define ERR_ELF_LOAD_OVER    FT_MAKE_ERRCODE(ErrorModGeneral, ErrElf, 2)

extern void *amp_img_start;
extern void *amp_img_end;

typedef struct
{
    void *image_start_address;
    FImageInfo image_info;
} amp_pack_img;

static amp_pack_img multcore_img[FCORE_NUM] = {0};
static u32 image_nums = 0; /* 获取当前组合镜像的子镜像个数 */

static void *CheckElfStartAddress(void *address)
{
    if (ElfIsImageValid((unsigned long)address))
    {
        return address;
    }
    return NULL;
}

u32 FGetImageInfo(void)
{
    FImageInfo image_info;
    u32 cpu_core_mask = 0;

    void *temp_address = &amp_img_start;
    FBOOT_DEBUG_D("amp_img_start is 0x%x.", &amp_img_start);
    FBOOT_DEBUG_D("amp_img_end is 0x%x.", &amp_img_end);
    FError err;

    /* 暂时开启所有的DDR 映射 */
    FMmuMap((uintptr)CONFIG_F32BIT_MEMORY_ADDRESS, (uintptr)CONFIG_F32BIT_MEMORY_ADDRESS,
            CONFIG_F32BIT_MEMORY_LENGTH, MT_NORMAL | MT_P_RWX_U_NA | MT_NS);

#ifdef __aarch64__
    FMmuMap((uintptr)CONFIG_F64BIT_MEMORY_ADDRESS, (uintptr)CONFIG_F64BIT_MEMORY_ADDRESS,
            CONFIG_F64BIT_MEMORY_LENGTH, MT_NORMAL | MT_P_RWX_U_NA | MT_NS);

#endif
    while (temp_address < (void *)&amp_img_end)
    {
        void *boot_elf_address = CheckElfStartAddress((void *)temp_address);
        if (boot_elf_address != NULL)
        {
            FBOOT_DEBUG_D("Found ELF image at address 0x%x.", boot_elf_address);
            u32 length = sizeof(FImageInfo);
            memset(&image_info, 0, sizeof(image_info));
            err = ElfGetSection((unsigned long)boot_elf_address, ".image_info",
                                (u8 *)&image_info, &length);
            if (!err)
            {
                FBOOT_DEBUG_D("image_info.process_core is %d, cpu_core_mask is %d.",
                              image_info.process_core, cpu_core_mask);
                if (image_info.process_core == 10000)
                {
                    FBOOT_DEBUG_D("AMP image: process_core is 10000, skip this image.");
                }
                else if (image_info.process_core >= FCORE_NUM)
                {
                    FBOOT_ERROR("Error: process_core is over max core number %d.",
                                image_info.process_core);
                    return 0;
                }
                else if (cpu_core_mask & (1 << image_info.process_core))
                {
                    FBOOT_ERROR("Error: Multiple programs for a single core.");
                    return 0;
                }
                else
                {
                    cpu_core_mask |= (1 << image_info.process_core);
                    multcore_img[image_info.process_core].image_start_address = (void *)
                        ElfLoadElfImagePhdr((unsigned long)boot_elf_address);
                }
                image_nums++;
            }
        }
        temp_address++;
    }
    FBOOT_DEBUG_D("cpu_core_mask is %d.", cpu_core_mask);
    FBOOT_DEBUG_D("image_nums: %d.", image_nums);

    return cpu_core_mask;
}

/* 引导核心进行具体的操作 */
int FBootInit(u32 cpu_id)
{
    u32 cpu_core_mask;
    image_nums = 0;
    cpu_core_mask = FGetImageInfo();

    if (cpu_core_mask == 0 || __builtin_popcount(cpu_core_mask) > FCORE_NUM)
    {
        FBOOT_ERROR("Error: No valid ELF image found or the number of found images "
                    "exceeds the limit of FCORE_NUM.");
        cpu_core_mask = 0;
    }

    if (image_nums == 0)
    {
        return ERR_ELF_LOAD_FAIL;
    }
    else if (image_nums > FCORE_NUM)
    {
        FBOOT_ERROR("Warning: The number of found images exceeds the limit of "
                    "FCORE_NUM.");
        return ERR_ELF_LOAD_OVER;
    }
    else
    {
        FBOOT_DEBUG_D("Found %d images.", image_nums);
    }
    /* 启动核心 */
    for (int i = 0; i < FCORE_NUM; i++)
    {
        if (((1 << i) & cpu_core_mask) && (i != cpu_id))
        {
            FPsciCpuMaskOn(1 << i, (uintptr)multcore_img[i].image_start_address);
            FBOOT_DEBUG_D("Start core %d, address 0x%x.", i, multcore_img[i].image_start_address);
        }
    }

    return 0;
}

int FBootCore(void)
{
    int ret = 0;
#if defined(CONFIG_USE_MSDF)

#if defined(CONFIG_USE_AARCH64_L1_TO_AARCH32)
    /* 当前启动核心是否与 CONFIG_MSDF_CORE_ID 一致，不一致，则跳转到启动核心 */
    /* AARCH32因为已经启动，重新运行镜像（AARCH64）会导致异常，需要跳过机器码 */
#define CONFIG_AARCH64_TO_AARCH32_JUMP_OFFSET 0x0 /*预留跳转偏移*/

    u32 cpu_id;
    ret = GetCpuId(&cpu_id);
    if (cpu_id != CONFIG_MSDF_CORE_ID)
    {
        FBOOT_DEBUG_D("***********************************");
        FBOOT_DEBUG_D("current core %d, jump to core %d. ", cpu_id, CONFIG_MSDF_CORE_ID);
        FBOOT_DEBUG_D("jump to address 0x%x\n.",
                      CONFIG_IMAGE_LOAD_ADDRESS + CONFIG_AARCH64_TO_AARCH32_JUMP_OFFSET);
        FPsciCpuMaskOn(1 << CONFIG_MSDF_CORE_ID,
                       CONFIG_IMAGE_LOAD_ADDRESS + CONFIG_AARCH64_TO_AARCH32_JUMP_OFFSET); /* 跳转到启动核心 */
        FPsciCpuOff(); /* 关闭当前核心 */
    }
#else
    /* 当前启动核心是否与 CONFIG_MSDF_CORE_ID 一致，不一致，则跳转到启动核心 */
    u32 cpu_id;
    ret = GetCpuId(&cpu_id);
    if (cpu_id != CONFIG_MSDF_CORE_ID)
    {
        FBOOT_DEBUG_D("***********************************");
        FBOOT_DEBUG_D("current core %d, jump to core %d. ", cpu_id, CONFIG_MSDF_CORE_ID);
        FPsciCpuMaskOn(1 << CONFIG_MSDF_CORE_ID, CONFIG_IMAGE_LOAD_ADDRESS); /* 跳转到启动核心 */
        FPsciCpuOff(); /* 关闭当前核心 */
    }
#endif

#if defined(CONFIG_MSDF1) /* 启动方式CONFIG_MSDF0已经完成 */
    ret = FBootInit(cpu_id);
#endif

#endif
    return ret;
}
