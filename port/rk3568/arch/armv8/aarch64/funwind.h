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
 * FilePath: funwind.h
 * Created Date: 2023-11-24 13:20:38
 * Last Modified: 2023-12-07 11:15:05
 * Description:  This file is for aarc64 unwind function 
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 * 1.0      huanghe   2023-11-24        first release
 */
#ifndef FUNWIND_H
#define FUNWIND_H

#include "ftypes.h"
#include "ferror_code.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * If -fno-omit-frame-pointer is used:
 *
 * - AArch64: The AAPCS defines the format of the frame records and mandates the
 *   usage of r29 as frame pointer.
 *
 * - AArch32: The format of the frame records is not defined in the AAPCS.
 *   However, at least GCC and Clang use the same format. When they are forced
 *   to only generate A32 code (with -marm), they use r11 as frame pointer and a
 *   similar format as in AArch64. If interworking with T32 is enabled, the
 *   frame pointer is r7 and the format is  different. This is not supported by
 *   this implementation of backtrace, so it is needed to use -marm.
 */

/* Frame records form a linked list in the stack */
struct FUnwindStackFrame
{
    /* Previous frame record in the list */
    struct FUnwindStackFrame *parent;
    /* Return address of the function at this level */
    uintptr_t return_addr;
};

void FUnwindFrame(struct FUnwindStackFrame *fr, uintptr_t current_pc,
                  uintptr_t link_register, void *args);

void FUnwindBacktrace(void *regs);


#ifdef __cplusplus
}
#endif

#endif
