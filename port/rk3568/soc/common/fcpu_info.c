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
 * FilePath: cpu_info.c
 * Date: 2022-03-08 19:37:19
 * LastEditTime: 2022-03-15 11:18:14
 * Description:  This file is for
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 */
#include "faarch.h"
#include "fcpu_info.h"
#include "ferror_code.h"
#include "fparameters.h"

FError GetCpuId(u32 *cpu_id_p)
{
    fsize_t affinity = GetAffinity();
    FError ret = ERR_SUCCESS;

    switch (affinity & 0xfffff)
    {
#ifdef CORE0_AFF
        case CORE0_AFF:
            *cpu_id_p = 0;
            break;
#endif
#ifdef CORE1_AFF
        case CORE1_AFF:
            *cpu_id_p = 1;
            break;
#endif
#ifdef CORE2_AFF
        case CORE2_AFF:
            *cpu_id_p = 2;
            break;
#endif
#ifdef CORE3_AFF
        case CORE3_AFF:
            *cpu_id_p = 3;
            break;
#endif
#ifdef CORE4_AFF
        case CORE4_AFF:
            *cpu_id_p = 4;
            break;
#endif
#ifdef CORE5_AFF
        case CORE5_AFF:
            *cpu_id_p = 5;
            break;
#endif
#ifdef CORE6_AFF
        case CORE6_AFF:
            *cpu_id_p = 6;
            break;
#endif
#ifdef CORE7_AFF
        case CORE7_AFF:
            *cpu_id_p = 7;
            break;
#endif
#ifdef CORE8_AFF
        case CORE8_AFF:
            *cpu_id_p = 8;
            break;
#endif
#ifdef CORE9_AFF
        case CORE9_AFF:
            *cpu_id_p = 9;
            break;
#endif
#ifdef CORE10_AFF
        case CORE10_AFF:
            *cpu_id_p = 10;
            break;
#endif
#ifdef CORE11_AFF
        case CORE11_AFF:
            *cpu_id_p = 11;
            break;
#endif
#ifdef CORE12_AFF
        case CORE12_AFF:
            *cpu_id_p = 12;
            break;
#endif
#ifdef CORE13_AFF
        case CORE13_AFF:
            *cpu_id_p = 13;
            break;
#endif
#ifdef CORE14_AFF
        case CORE14_AFF:
            *cpu_id_p = 14;
            break;
#endif
#ifdef CORE15_AFF
        case CORE15_AFF:
            *cpu_id_p = 15;
            break;
#endif
        default:
            ret = ERR_GENERAL;
            break;
    }
    return ret;
}

/**
 * @name: GetCpuAffinityByMask
 * @msg:  Determine the cluster information using the CPU ID
 * @param {u32} cpu_id cpu id mask .for example : 1 is core0 ,2 is core1 .....
 * @param {u64} *affinity_level_p cluster information , format is:
 * |--------[bit31-24]-------[bit23-16]-------------[bit15-8]--------[bit7-0]
 * |--------Affinity level3-----Affinity level2-----Affinity level1---Affinity level0
 * @return {*} ERR_SUCCESS is ok
 */
FError GetCpuAffinityByMask(u32 cpu_id_mask, u64 *affinity_level_p)
{
    FError ret = ERR_SUCCESS;
    switch (cpu_id_mask)
    {
#ifdef CORE0_AFF
        case (1 << 0):
            *affinity_level_p = CORE0_AFF;
            break;
#endif
#ifdef CORE1_AFF
        case (1 << 1):
            *affinity_level_p = CORE1_AFF;
            break;
#endif
#ifdef CORE2_AFF
        case (1 << 2):
            *affinity_level_p = CORE2_AFF;
            break;
#endif
#ifdef CORE3_AFF
        case (1 << 3):
            *affinity_level_p = CORE3_AFF;
            break;
#endif
#ifdef CORE4_AFF
        case (1 << 4):
            *affinity_level_p = CORE4_AFF;
            break;
#endif
#ifdef CORE5_AFF
        case (1 << 5):
            *affinity_level_p = CORE5_AFF;
            break;
#endif
#ifdef CORE6_AFF
        case (1 << 6):
            *affinity_level_p = CORE6_AFF;
            break;
#endif
#ifdef CORE7_AFF
        case (1 << 7):
            *affinity_level_p = CORE7_AFF;
            break;
#endif
#ifdef CORE8_AFF
        case (1 << 8):
            *affinity_level_p = CORE8_AFF;
            break;
#endif
#ifdef CORE9_AFF
        case (1 << 9):
            *affinity_level_p = CORE9_AFF;
            break;
#endif
#ifdef CORE10_AFF
        case (1 << 10):
            *affinity_level_p = CORE10_AFF;
            break;
#endif
#ifdef CORE11_AFF
        case (1 << 11):
            *affinity_level_p = CORE11_AFF;
            break;
#endif
#ifdef CORE12_AFF
        case (1 << 12):
            *affinity_level_p = CORE12_AFF;
            break;
#endif
#ifdef CORE13_AFF
        case (1 << 13):
            *affinity_level_p = CORE13_AFF;
            break;
#endif
#ifdef CORE14_AFF
        case (1 << 14):
            *affinity_level_p = CORE14_AFF;
            break;
#endif
#ifdef CORE15_AFF
        case (1 << 15):
            *affinity_level_p = CORE15_AFF;
            break;
#endif
        default:
            ret = ERR_GENERAL;
            break;
    }
    return ret;
}


/**
 * @name: GetCpuAffinity
 * @msg:  Determine the cluster information using the CPU ID
 * @param {u32} cpu_id cpu id .for example : 0 is core0 ,1 is core1 .....
 * @param {u64} *affinity_level_p cluster information , format is:
 * |--------[bit31-24]-------[bit23-16]-------------[bit15-8]--------[bit7-0]
 * |--------Affinity level3-----Affinity level2-----Affinity level1---Affinity level0
 * @return {*} ERR_SUCCESS is ok
 */
FError GetCpuAffinity(u32 cpu_id, u64 *affinity_level_p)
{
    FError ret = ERR_SUCCESS;
    switch (cpu_id)
    {
#ifdef CORE0_AFF
        case (0):
            *affinity_level_p = CORE0_AFF;
            break;
#endif
#ifdef CORE1_AFF
        case (1):
            *affinity_level_p = CORE1_AFF;
            break;
#endif
#ifdef CORE2_AFF
        case (2):
            *affinity_level_p = CORE2_AFF;
            break;
#endif
#ifdef CORE3_AFF
        case (3):
            *affinity_level_p = CORE3_AFF;
            break;
#endif
#ifdef CORE4_AFF
        case (4):
            *affinity_level_p = CORE4_AFF;
            break;
#endif
#ifdef CORE5_AFF
        case (5):
            *affinity_level_p = CORE5_AFF;
            break;
#endif
#ifdef CORE6_AFF
        case (6):
            *affinity_level_p = CORE6_AFF;
            break;
#endif
#ifdef CORE7_AFF
        case (7):
            *affinity_level_p = CORE7_AFF;
            break;
#endif
#ifdef CORE8_AFF
        case (8):
            *affinity_level_p = CORE8_AFF;
            break;
#endif
#ifdef CORE9_AFF
        case (9):
            *affinity_level_p = CORE9_AFF;
            break;
#endif
#ifdef CORE10_AFF
        case (10):
            *affinity_level_p = CORE10_AFF;
            break;
#endif
#ifdef CORE11_AFF
        case (11):
            *affinity_level_p = CORE11_AFF;
            break;
#endif
#ifdef CORE12_AFF
        case (12):
            *affinity_level_p = CORE12_AFF;
            break;
#endif
#ifdef CORE13_AFF
        case (13):
            *affinity_level_p = CORE13_AFF;
            break;
#endif
#ifdef CORE14_AFF
        case (14):
            *affinity_level_p = CORE14_AFF;
            break;
#endif
#ifdef CORE15_AFF
        case (15):
            *affinity_level_p = CORE15_AFF;
            break;
#endif
        default:
            ret = ERR_GENERAL;
            break;
    }
    return ret;
}


/**
 * @name: UseAffinityGetCpuId
 * @msg:  Get the core value from affinity level
 * @param {u64} affinity_level is cpu affinity level value
 * @param {u32*} cpu_id_p is pointer to get cpu id value
 * @return {*} ERR_SUCCESS is ok , ERR_GENERAL is fail
 */
FError UseAffinityGetCpuId(u64 affinity_level, u32 *cpu_id_p)
{
    FError ret = ERR_SUCCESS;
    switch (affinity_level)
    {
#ifdef CORE0_AFF
        case CORE0_AFF:
            *cpu_id_p = 0;
            break;
#endif
#ifdef CORE1_AFF
        case CORE1_AFF:
            *cpu_id_p = 1;
            break;
#endif
#ifdef CORE2_AFF
        case CORE2_AFF:
            *cpu_id_p = 2;
            break;
#endif
#ifdef CORE3_AFF
        case CORE3_AFF:
            *cpu_id_p = 3;
            break;
#endif
#ifdef CORE4_AFF
        case CORE4_AFF:
            *cpu_id_p = 4;
            break;
#endif
#ifdef CORE5_AFF
        case CORE5_AFF:
            *cpu_id_p = 5;
            break;
#endif
#ifdef CORE6_AFF
        case CORE6_AFF:
            *cpu_id_p = 6;
            break;
#endif
#ifdef CORE7_AFF
        case CORE7_AFF:
            *cpu_id_p = 7;
            break;
#endif
#ifdef CORE8_AFF
        case CORE8_AFF:
            *cpu_id_p = 8;
            break;
#endif
#ifdef CORE9_AFF
        case CORE9_AFF:
            *cpu_id_p = 9;
            break;
#endif
#ifdef CORE10_AFF
        case CORE10_AFF:
            *cpu_id_p = 10;
            break;
#endif
#ifdef CORE11_AFF
        case CORE11_AFF:
            *cpu_id_p = 11;
            break;
#endif
#ifdef CORE12_AFF
        case CORE12_AFF:
            *cpu_id_p = 12;
            break;
#endif
#ifdef CORE13_AFF
        case CORE13_AFF:
            *cpu_id_p = 13;
            break;
#endif
#ifdef CORE14_AFF
        case CORE14_AFF:
            *cpu_id_p = 14;
            break;
#endif
#ifdef CORE15_AFF
        case CORE15_AFF:
            *cpu_id_p = 15;
            break;
#endif
        default:
            ret = ERR_GENERAL;
            break;
    }
    return ret;
}
