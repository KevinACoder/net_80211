/*
 * Copyright (C) 2021, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fimage_info.c
 * Date: 2021-08-31 11:16:59
 * LastEditTime: 2022-02-17 18:05:16
 * Description:  This file is for image information of boot
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0  huanghe  2023/05/26       init
 */

#ifndef FIMAGE_INFO_H
#define FIMAGE_INFO_H

#include "sdkconfig.h"
#include "ftypes.h"

#ifdef __cplusplus
extern "C"
{
#endif


#define FIMAGE_BAREMETAL_TYPE_ID   1
#define FIMAGE_MAGIC_CODE          0xCAFEBABEDEADBEEF
#define FIMAGE_USE_IMAGE_PARAMETER 1

typedef struct
{
    u64 magic_code;      /* 魔数，用于标识此结构体是否有效 */
    u64 image_type;      /* image 类型，如 kernel、ramdisk 等 */
    u64 phy_address;     /* image 的物理地址 */
    u64 phy_endaddress;  /* image 的物理结束地址 */
    u64 virt_address;    /* image 的虚拟地址 */
    u64 virt_endaddress; /* image 的虚拟结束地址 */
    /* boot parameters */
    u64 use_boot_parameters; /* 是否使用 boot 参数 */
    s64 process_core;        /* image 运行的核心 */
} FImageInfo;


#ifdef __cplusplus
extern "C"
}
#endif

#endif
