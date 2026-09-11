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
 * FilePath: fcpu_info.h
 * Date: 2022-03-08 19:37:19
 * LastEditTime: 2022-03-15 11:18:07
 * Description:  This file is for
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 */

#ifndef COMMON_FCPU_INFO_H
#define COMMON_FCPU_INFO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ftypes.h"
#include "ferror_code.h"
#include "faarch.h"

FError GetCpuId(u32 *cpu_id_p);
FError GetCpuAffinity(u32 cpu_id, u64 *cluster_value_p);
FError GetCpuAffinityByMask(u32 cpu_id, u64 *affinity_level_p);
FError UseAffinityGetCpuId(u64 affinity_level, u32 *cpu_id_p);
u32 GetCpuMaskToAffval(u32 *cpu_mask, u32 *cluster_id, u32 *target_list);

#ifdef __cplusplus
}
#endif

#endif // !