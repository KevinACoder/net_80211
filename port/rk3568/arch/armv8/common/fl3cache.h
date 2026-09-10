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
 * FilePath: fl3cache.h
 * Date: 2022-03-08 21:56:42
 * LastEditTime: 2022-03-15 11:14:45
 * Description:  This file is for l3 cache-related operations
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe    2021/10/21       first release
 */


#ifndef FL3CACHE_H
#define FL3CACHE_H

#include "fparameters.h"
#include "fio.h"

#ifdef __cplusplus
extern "C"
{
#endif

/************************** Function Prototypes ******************************/


void FCacheL3CacheEnable(void);
void FCacheL3CacheDisable(void);
void FCacheL3CacheInvalidate(void);
void FCacheL3CacheFlush(void);

#ifdef __cplusplus
}
#endif

#endif