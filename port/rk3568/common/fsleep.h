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
 * FilePath: fsleep.h
 * Date: 2021-05-28 08:48:40
 * LastEditTime: 2022-02-17 18:02:51
 * Description:  This file is for creating custom sleep interface for standlone sdk.
 *
 * Modify History:
 *  Ver     Who           Date                  Changes
 * -----   ------       --------     --------------------------------------
 *  1.0    huanghe      2022/7/23            first release
 */


#ifndef FSLEEP_H
#define FSLEEP_H

#include "ftypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

u32 fsleep_seconds(u32 seconds);   /* 按秒延迟 */
u32 fsleep_millisec(u32 mseconds); /* 按毫秒延迟 */
u32 fsleep_microsec(u32 useconds); /* 按微秒延迟 */

#ifdef __cplusplus
}
#endif


#endif // !