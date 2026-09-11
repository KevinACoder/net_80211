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
 * FilePath: ftypes.h
 * Date: 2021-05-27 13:30:03
 * LastEditTime: 2026-07-08 16:35:00
 * Description:  This file is for variable type definition
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0  zhugengyu   2021/05/27  init
 * 1.1  zhugengyu   2022/02/18  add some typedef
 * 1.2  zhugengyu   2026/07/08  add PRI*64 fallback macros for freestanding builds
 */


#ifndef FTYPES_H
#define FTYPES_H

#include <stdint.h>
#include <stddef.h>

#if !defined(__int64_t_defined) && defined(__INT64_TYPE__)
#define __int64_t_defined 1
#endif

#include <inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FT_COMPONENT_IS_READY   0x11111111U
#define FT_COMPONENT_IS_STARTED 0x22222222U

typedef uint8_t u8;            /* unsigned 8-bit */
typedef signed char s8;        /* signed 8-bit (aarch64 char is unsigned) */
typedef uint16_t u16;          /* unsigned 16-bit */
typedef short s16;             /* signed 16-bit */
typedef uint32_t u32;          /* unsigned 32-bit */
typedef int32_t s32;           /* signed 32-bit */
typedef uint64_t u64;          /* unsigned 64-bit */
typedef int64_t s64;           /* unsigned */
typedef float f32;             /* 32-bit floating point */
typedef double f64;            /* 64-bit double precision FP */
typedef unsigned long boolean; /* boolean */
typedef uint64_t _time_t;
typedef size_t fsize_t;

typedef intptr_t intptr; /* intptr_t是为了跨平台，其长度总是所在平台的位数，所以用来存放地址。 */
typedef uintptr_t uintptr;
typedef ptrdiff_t ptrdiff;

#ifdef __aarch64__
typedef u64 tick_t;
#else
typedef u32 tick_t;
#endif

#define ULONG64_HI_MASK 0xFFFFFFFF00000000U
#define ULONG64_LO_MASK ~ULONG64_HI_MASK

#ifndef TRUE
#define TRUE 1U
#endif

#ifndef FALSE
#define FALSE 0U
#endif

#ifndef NULL
#define NULL 0U
#endif

#define _INLINE        inline
#define _ALWAYS_INLINE inline __attribute__((always_inline))
#define _WEAK          __attribute__((weak))
#define _UNUSED        __attribute__((unused))

#define FUNUSED(x)     ((void)x)

typedef void (*FIrqHandler)(void *InstancePtr);

typedef void (*FExceptionHandler)(void *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif // !
