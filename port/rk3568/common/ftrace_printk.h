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
 * FilePath: ftrace_printk.c
 * Date: 2025-08-20 18:42:52
 * LastEditTime: 2025-08-20 19:09:45
 * Description:  This file is for trace printf function.
 *
 * Modify History:
 *  Ver     Who           Date                  Changes
 * -----   ------       --------     --------------------------------------
 *  1.0    LiuSM      2025/8/20            first release
 */

#ifndef FTRACE_PRINTK_H
#define FTRACE_PRINTK_H

#ifdef __cplusplus
extern "C"
{
#endif

/* 消息级别定义 */
typedef enum
{
    TRACE_LEVEL_DEBUG = 0,
    TRACE_LEVEL_INFO,
    TRACE_LEVEL_WARN,
    TRACE_LEVEL_ERROR,
    TRACE_LEVEL_CRITICAL
} FTraceLevel;

void ftrace_printk(const char *format, ...);
void ftrace_printf(FTraceLevel level, const char *function_name, const char *fmt, ...);
void ftrace_print_recent_messages(uint32_t count);
void ftrace_print_shared_memory(void);
uint32_t fget_unread_message_count(void);
void fmark_all_messages_read(void);

#ifdef __cplusplus
}
#endif

#endif
