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
 * FilePath: flinkage.h
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:35:12
 * Description:  This files is for assembler code format macro definition
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 *  1.0  huanghe	2021/11/06		first release
 */


#ifndef FLINKAGE_H
#define FLINKAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Some toolchains use other characters (e.g. '`') to mark new line in macro */
#ifndef ASM_NL
#define ASM_NL ;
#endif

#ifdef __cplusplus
#define CPP_ASMLINKAGE extern "C"
#else
#define CPP_ASMLINKAGE
#endif

#ifndef asmlinkage
#define asmlinkage CPP_ASMLINKAGE
#endif

#define SYMBOL_NAME_STR(X) #X
#define SYMBOL_NAME(X)     X
#ifdef __STDC__
#define SYMBOL_NAME_LABEL(X) X##:
#else
#define SYMBOL_NAME_LABEL(X) \
    X:
#endif

#ifndef __ALIGN
#define __ALIGN .align 4
#endif

#ifndef __ALIGN_STR
#define __ALIGN_STR ".align 4"
#endif

#define ALIGN        __ALIGN
#define ALIGN_STR    __ALIGN_STR

#define LENTRY(name) ALIGN ASM_NL SYMBOL_NAME_LABEL(name)

#define ENTRY(name)          \
    .globl SYMBOL_NAME(name) \
    ASM_NL                   \
    LENTRY(name)

#define WEAK(name)          \
    .weak SYMBOL_NAME(name) \
    ASM_NL                  \
    LENTRY(name)

#ifndef END
#define END(name) .size name, .- name
#endif

#ifndef ENDPROC
#define ENDPROC(name) .type name STT_FUNC ASM_NL END(name)
#endif

#ifdef __cplusplus
}
#endif

#endif
