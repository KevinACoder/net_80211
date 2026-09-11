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
 * @FilePath: fcompiler.h
 * @Date: 2023-05-26 11:11:30
 * @LastEditTime: 2023-05-26 11:11:31
 * @Description:  This file is for gcc compiler compilation
 * 
 * @Modify History: 
 *  Ver   Who     Date   Changes
 * ----- ------  -------- --------------------------------------
 *  1.0   huanghe 2023-05-26  init
 */


#ifndef FCOMPILER_H
#define FCOMPILER_H

#ifdef __cplusplus
extern "C"
{
#endif

#define FCOMPILER_SECTION(section_name) __attribute__((section(section_name)))
#if defined(__aarch64__)
#define FAARCH64_USE __aarch64__
#endif

#ifndef ASM
#define ASM __asm
#endif

#ifndef INLINE
#define INLINE __inline
#endif

#ifndef STATIC_INLINE
#define STATIC_INLINE static inline
#endif

#ifndef USED
#define USED __attribute__((used))
#endif

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

#ifndef CLZ
#define CLZ(value) (__builtin_clz(value))
#endif

#ifndef CLZL
#define CLZL(value) (__builtin_clzl(value))
#endif

#ifndef CTZ
#define CTZ(value) (__builtin_ctz(value))
#endif

#ifndef CTZL
#define CTZL(value) (__builtin_ctzl(value))
#endif

#ifndef NORETURN
#define NORETURN __attribute__((__noreturn__))
#endif

#ifndef DEPRECATED
#define DEPRECATED __attribute__((deprecated))
#endif


#ifdef __cplusplus
}
#endif

#endif // !
