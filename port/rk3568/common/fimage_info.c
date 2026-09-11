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

#include "fimage_info.h"
#include "fcompiler.h"
#include "sdkconfig.h"
#include "fparameters.h"

#define FIMAGE_BOOTCODE_CORE_SKIP \
    10000 /* 当amp_config.json设置启动核心的编号为10000时，表示不使用bootstrap启动,只编译打包到组合镜像中 */

#if (CONFIG_IMAGE_CORE > FCORE_NUM) && (CONFIG_IMAGE_CORE != FIMAGE_BOOTCODE_CORE_SKIP)
#error "Check the configuration of IMAGE_CORE, which needs to be less than the maximum number of cores allowed by SOC"
#endif


FImageInfo fimage_info FCOMPILER_SECTION(".my_image_info") = {
    .magic_code = FIMAGE_MAGIC_CODE,
    .image_type = FIMAGE_BAREMETAL_TYPE_ID,
    .phy_address = 0,
    .phy_endaddress = 0,
    .virt_address = 0,
    .virt_endaddress = 0,
    /* boot parameters */
    .use_boot_parameters = 1,
    .process_core = CONFIG_IMAGE_CORE,
};
