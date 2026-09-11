/*
 * Copyright (C) 2026, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fcpu_affinity_mask.c
 * Date: 2026-08-06
 * Description: RK3568 quad-core a55, single cluster
 */
#include "fparameters.h"
#include "ftypes.h"

/**
 * @name: GetCpuMaskToAffval
 * @msg:  Convert information in cpu_mask to cluster_ID and target_list
 * @param {u32} *cpu_mask is each bit of cpu_mask represents a selected CPU, for example, 0x3 represents core0 and CORE1 .
 * @param {u32} *cluster_id is information about the cluster in which core resides ,format is
 * |--------[bit31-24]-------[bit23-16]-------------[bit15-8]-----------[bit7-0]
 * |--------Affinity level3-----Affinity level2-----Affinity level1-----Affinity level0
 * @param {u32} *target_list  is core mask in cluster
 * @return {u32} 0 indicates that the conversion was not successful , 1 indicates that the conversion was successful
 */
u32 GetCpuMaskToAffval(u32 *cpu_mask, u32 *cluster_id, u32 *target_list)
{
    if (*cpu_mask == 0)
    {
        return 0;
    }

    *target_list = 0;
    *cluster_id = 0;

    *target_list = *cpu_mask;
    *cpu_mask = 0;
    return 1;
}
