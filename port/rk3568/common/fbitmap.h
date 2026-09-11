/*
 * Copyright (C) 2023, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fbitmap.h
 * Created Date: 2023-10-31 18:09:18
 * Last Modified: 2023-11-15 09:42:05
 * Description:  This file is for bitmap
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0      huanghe     2023/11/10      first release
 */

#ifndef FBITMAP_H
#define FBITMAP_H

#include "ftypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned long FBitPerWordType;

#define FBIT_INVALID_BIT_INDEX (sizeof(FBitPerWordType))


void FBitMapSet(FBitPerWordType *bitmap, u16 pos);

void FBitMapClear(FBitPerWordType *bitmap, u16 pos);

u16 FBitMapHighGet(FBitPerWordType bitmap);

u16 FBitMapLowGet(FBitPerWordType bitmap);


void FBitMapSetNBits(FBitPerWordType *bitmap, u32 start, u32 nums_set);


void FBitMapClrNBits(FBitPerWordType *bitmap, u32 start, u32 nums_clear);

s32 FBitMapFfz(FBitPerWordType *bitmap, u32 num_bits);

void FBitMapCopyClearTail(FBitPerWordType *dst, const FBitPerWordType *src, u32 nbits);


#ifdef __cplusplus
}
#endif


#endif
