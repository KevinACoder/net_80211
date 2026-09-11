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
 * FilePath: funwind.c
 * Created Date: 2023-11-24 13:20:28
 * Last Modified: 2023-12-07 11:15:10
 * Description:  This file is for aarc64 unwind function
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 * 1.0      huanghe   2023-11-24        first release
 */


#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


#include "funwind.h"
#include "ftypes.h"
#include "ferror_code.h"
#include "funwind.h"
#include "fdebug.h"

#include "fdebug.h"

#define FUNWIND_DEBUG_TAG "UNWIND"
#define FUNWIND_DEBUG(format, ...) \
    FT_DEBUG_PRINT_D(FUNWIND_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUNWIND_INFO(format, ...) \
    FT_DEBUG_PRINT_I(FUNWIND_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUNWIND_WARN(format, ...) \
    FT_DEBUG_PRINT_W(FUNWIND_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUNWIND_ERROR(format, ...) \
    FT_DEBUG_PRINT_E(FUNWIND_DEBUG_TAG, format, ##__VA_ARGS__)

/* Maximum number of entries in the backtrace to display */
#define UNWIND_LIMIT 0xff

/*
 * Returns true if all the bytes in a given object are in mapped memory and an
 * LDR using this pointer would succeed, false otherwise.
 */
static bool is_valid_object(uintptr_t addr, size_t size)
{
    assert(size > 0U);

    if (addr == 0U)
    {
        return false;
    }

    /* Detect overflows */
    if ((addr + size) < addr)
    {
        return false;
    }

    /* A pointer not aligned properly could trigger an alignment fault. */
    if ((addr & (sizeof(uintptr_t) - 1U)) != 0U)
    {
        return false;
    }

    return true;
}

/*
 * Returns true if the specified address is correctly aligned and points to a
 * valid memory region.
 */
static bool is_valid_jump_address(uintptr_t addr)
{
    if (addr == 0U)
    {
        return false;
    }

    /* Check alignment. Both A64 and A32 use 32-bit opcodes */
    if ((addr & (sizeof(uint32_t) - 1U)) != 0U)
    {
        return false;
    }

    return true;
}

/*
 * Returns true if the pointer points at a valid frame record, false otherwise.
 */
static bool is_valid_frame_record(struct FUnwindStackFrame *fr)
{
    return is_valid_object((uintptr_t)fr, sizeof(struct FUnwindStackFrame));
}

/*
 * Adjust the frame-pointer-register value by 4 bytes on AArch32 to have the
 * same layout as AArch64.
 */
static struct FUnwindStackFrame *adjust_frame_record(struct FUnwindStackFrame *fr)
{
    return fr;
}

void FUnwindFrame(struct FUnwindStackFrame *fr, uintptr_t current_pc,
                  uintptr_t link_register, void *args)
{
    uintptr_t call_site;
    static const char *backtrace_str = "%08x ";

    if (!is_valid_frame_record(fr))
    {
        FUNWIND_ERROR("ERROR: Corrupted frame pointer (frame record address = %p)\n", fr);
        return;
    }

    if (fr->return_addr != link_register)
    {
        FUNWIND_ERROR("ERROR: Corrupted stack (frame record address = %p)\n", fr);
        return;
    }

    /* The level 0 of the backtrace is the current backtrace function */
    printf(backtrace_str, current_pc);

    /*
	 * The last frame record pointer in the linked list at the beginning of
	 * the stack should be NULL unless stack is corrupted.
	 */
    for (unsigned int i = 1U; i < UNWIND_LIMIT; i++)
    {
        /* If an invalid frame record is found, exit. */
        if (!is_valid_frame_record(fr))
        {
            return;
        }
        /*
		 * A32 and A64 are fixed length so the address from where the
		 * call was made is the instruction before the return address,
		 * which is always 4 bytes before it.
		 */
        call_site = fr->return_addr - 4U;

        /*
		 * If the address is invalid it means that the frame record is
		 * probably corrupted.
		 */
        if (!is_valid_jump_address(call_site))
        {
            return;
        }

        printf(backtrace_str, call_site);

        fr = adjust_frame_record(fr->parent);
    }

    FUNWIND_ERROR("ERROR: Max backtrace depth reached\n");
}


/**
 * @name: FUnwindBacktrace
 * @msg: 执行堆栈回溯，用于调试和异常处理中确定调用栈的状态。
 * @return: 无返回值（void）
 * @note: 此函数被标记为_WEAK，它可以被同名函数覆盖。
 * @param {void *} regs: 预留开发使用。
 */
_WEAK void FUnwindBacktrace(void *regs)
{
    uintptr_t return_address = (uintptr_t)__builtin_return_address(0U);
    struct FUnwindStackFrame *fr = __builtin_frame_address(0U);

    fr = adjust_frame_record(fr);

    printf("please use: addr2line -e <project name>.elf -a -f ");
    FUnwindFrame(fr, (uintptr_t)&FUnwindBacktrace, return_address, NULL);
    printf("\n");
}
