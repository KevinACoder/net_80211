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
 * Date: 2025-08-20 18:40:52
 * LastEditTime: 2025-08-20 19:02:45
 * Description:  This file is for trace f_printf function.
 *
 * Modify History:
 *  Ver     Who           Date                  Changes
 * -----   ------       --------     --------------------------------------
 *  1.0    LiuSM      2025/8/20            first release
 */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "fkernel.h"
#include "faarch.h"
#include "ftypes.h"
#include "fprintf.h"
#include "fprintk.h"
#include "console.h"
#include "sdkconfig.h"
#include "fmmu.h"
#include "ftrace_printk.h"
#include "fgeneric_timer.h"

#define SHARED_MEM_BASE   0xC8000000
#define TRACE_BUFFER_SIZE (1024 * 1024) /* 1MB共享内存 */

typedef struct
{
    volatile u32 write_index;
    volatile u32 read_index;
    volatile u32 overflow_count;
    char buffer[TRACE_BUFFER_SIZE - 12]; /* 减去索引和计数的大小 */
} FTraceBuffer;

/* 共享内存指针 */
static FTraceBuffer *trace_buffer = (FTraceBuffer *)SHARED_MEM_BASE;
static boolean trace_initialized = FALSE;

/**
 * @brief 初始化共享内存Trace Buffer
 * 
 * 此函数初始化共享内存区域，重置所有索引和计数器。
 * 应在系统启动时调用一次。
 */
void trace_buffer_init(void)
{
    /* 初始化共享内存中的索引 */
    FMmuMap(SHARED_MEM_BASE, SHARED_MEM_BASE, TRACE_BUFFER_SIZE, MT_NORMAL | MT_P_RW_U_RW | MT_NS);
    trace_buffer->write_index = 0;
    trace_buffer->read_index = 0;
    trace_buffer->overflow_count = 0;
    trace_initialized = TRUE;
    /* 确保内存同步 */
    DSB();
}

/**
 * @brief 将字符串写入共享内存
 * 
 * 此函数将字符串数据写入共享内存的环形缓冲区，
 * 处理缓冲区环绕和溢出情况。
 * 
 * @param str 要写入的字符串
 * @param level 消息级别
 * @param function_name 产生消息的函数名称
 */
static void trace_putstring(const char *str, FTraceLevel level, const char *function_name)
{
    if (!trace_initialized)
    {
        return;
    }

    u32 timestamp = GenericTimerRead(GENERIC_TIMER_ID0); /* 获取时间戳 */
    u32 buffer_size = TRACE_BUFFER_SIZE - 12;            /* 实际缓冲区大小 */

    /* 格式化消息前缀 [时间戳] [级别] [函数]: */
    char prefix[128];
    const char *level_str;

    switch (level)
    {
        case TRACE_LEVEL_DEBUG:
            level_str = "DEBUG";
            break;
        case TRACE_LEVEL_INFO:
            level_str = "INFO";
            break;
        case TRACE_LEVEL_WARN:
            level_str = "WARN";
            break;
        case TRACE_LEVEL_ERROR:
            level_str = "ERROR";
            break;
        case TRACE_LEVEL_CRITICAL:
            level_str = "CRITICAL";
            break;
        default:
            level_str = "UNKNOWN";
            break;
    }

    int prefix_len = snprintf(prefix, sizeof(prefix), "[%u] [%s] [%s]: ", timestamp,
                              level_str, function_name);

    /* 计算总消息长度 (前缀 + 内容 + 换行符) */
    u32 total_len = prefix_len + strlen(str) + 1;

    /* 检查是否有足够空间 */
    u32 write_idx = trace_buffer->write_index;
    u32 read_idx = trace_buffer->read_index;
    u32 space_available;

    if (write_idx >= read_idx)
    {
        space_available = buffer_size - (write_idx - read_idx);
    }
    else
    {
        space_available = read_idx - write_idx;
    }

    if (total_len > space_available)
    {
        /* 空间不足，增加溢出计数 */
        trace_buffer->overflow_count++;
        return;
    }

    /* 写入消息前缀 */
    for (u32 i = 0; i < prefix_len; i++)
    {
        trace_buffer->buffer[write_idx] = prefix[i];
        write_idx = (write_idx + 1) % buffer_size;
    }

    /* 写入消息内容 */
    const char *p = str;
    while (*p)
    {
        trace_buffer->buffer[write_idx] = *p;
        write_idx = (write_idx + 1) % buffer_size;
        p++;
    }

    /* 添加换行符 */
    trace_buffer->buffer[write_idx] = '\n';
    write_idx = (write_idx + 1) % buffer_size;

    /* 更新写索引 */
    trace_buffer->write_index = write_idx;

    /* 确保内存写入完成 */
    DSB();
}

/**
 * @brief 自定义的vprintf函数，将输出重定向到共享内存
 * 
 * 此函数替代标准的vprintf，将格式化后的字符串写入共享内存。
 * 
 * @param fmt 格式化字符串
 * @param arg 可变参数列表
 * @param level 消息级别
 * @param function_name 调用函数名称
 * @return int 写入的字符数
 */
static int trace_vprintf(const char *fmt, va_list arg, FTraceLevel level, const char *function_name)
{
    char buffer[256]; /* 本地缓冲区 */
    int len;

    /* 第一次使用时初始化共享内存 */
    if (!trace_initialized)
    {
        trace_buffer_init();
    }

    /* 格式化字符串到本地缓冲区 */
    len = vsnprintf(buffer, sizeof(buffer), fmt, arg);

    /* 如果格式化后的字符串超过本地缓冲区大小，处理截断 */
    if (len >= (int)sizeof(buffer))
    {
        len = sizeof(buffer) - 1;
        buffer[len] = '\0';
    }

    /* 将格式化后的字符串写入共享内存 */
    trace_putstring(buffer, level, function_name);

    return len;
}

/**
 * @brief 重定向的ftrace_printk函数
 * 
 * 此函数替代原有的ftrace_printk，将输出重定向到共享内存。
 * 默认使用INFO级别。
 * 
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void ftrace_printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    /* 使用我们的trace_vprintf替代原来的f_vprintf */
    trace_vprintf(fmt, ap, TRACE_LEVEL_INFO, "ftrace_printk");
    va_end(ap);
}

#if defined(CONFIG_OPENAMP_TRACE_DEBUG)
// 创建别名
void f_printk(const char *fmt, ...) __attribute__((alias("ftrace_printk")));
#endif
/**
 * @brief 带级别的调试输出函数
 * 
 * 此函数提供带级别的调试输出，可以指定消息的重要性。
 * 
 * @param level 消息级别
 * @param function_name 调用函数名称
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void ftrace_printf(FTraceLevel level, const char *function_name, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    trace_vprintf(fmt, ap, level, function_name);

    va_end(ap);
}

/**
 * @brief 调试函数：打印共享内存内容
 * 
 * 此函数通过printf输出共享内存中的全部内容，
 * 用于调试和验证Trace Buffer功能。
 */
void ftrace_print_shared_memory(void)
{
    if (!trace_initialized)
    {
        f_printf("Trace buffer not initialized\n");
        return;
    }

    u32 read_idx = trace_buffer->read_index;
    u32 write_idx = trace_buffer->write_index;
    u32 buffer_size = TRACE_BUFFER_SIZE - 12;

    f_printf("=== Shared Memory Trace Buffer Contents ===\r\n");
    f_printf("Write Index: %u, Read Index: %u, Overflow Count: %u\r\n", write_idx,
             read_idx, trace_buffer->overflow_count);

    if (read_idx == write_idx)
    {
        f_printf("No messages in buffer.\n");
        return;
    }

    f_printf("Messages:\r\n");

    /* 临时变量用于遍历缓冲区 */
    u32 current_idx = read_idx;
    int message_count = 0;

    while (current_idx != write_idx)
    {
        /* 查找消息结束位置（换行符） */
        u32 message_start = current_idx;
        u32 message_length = 0;
        boolean found_end = FALSE;

        /* 查找消息结束 */
        while (current_idx != write_idx)
        {
            char c = trace_buffer->buffer[current_idx];
            current_idx = (current_idx + 1) % buffer_size;
            message_length++;

            if (c == '\n')
            {
                found_end = TRUE;
                break;
            }
        }

        /* 如果找到完整的消息，打印它 */
        if (found_end)
        {
            f_printf("[%d] ", ++message_count);
            for (u32 i = 0; i < message_length; i++)
            {
                u32 idx = (message_start + i) % buffer_size;
                putchar(trace_buffer->buffer[idx]);
            }
            f_printf("\n");
        }
        else
        {
            /* 不完整的消息（可能由于缓冲区环绕） */
            f_printf("[Partial] ");
            for (u32 i = 0; i < message_length; i++)
            {
                u32 idx = (message_start + i) % buffer_size;
                putchar(trace_buffer->buffer[idx]);
            }
            f_printf("\n");
            break;
        }
    }

    f_printf("=== End of Shared Memory Contents ===\n");
}

/**
 * @brief 调试函数：打印指定数量的最新消息
 * 
 * 此函数通过printf输出共享内存中指定数量的最新消息，
 * 可以灵活控制显示的消息数量。
 * 
 * @param count 要显示的消息数量
 */
void ftrace_print_recent_messages(u32 count)
{
    if (!trace_initialized)
    {
        f_printf("Trace buffer not initialized\n");
        return;
    }

    if (count == 0)
    {
        return;
    }
    u32 read_idx = trace_buffer->read_index;
    u32 write_idx = trace_buffer->write_index;
    u32 buffer_size = TRACE_BUFFER_SIZE - 12;

    f_printf("=== Reading %u Messages from Shared Memory ===\n", count);
    f_printf("Initial - Write Index: %u, Read Index: %u, Overflow Count: %u\n",
             write_idx, read_idx, trace_buffer->overflow_count);

    if (read_idx == write_idx)
    {
        f_printf("No messages in buffer.\n");
        return;
    }

    /* 计算消息总数 */
    u32 total_messages = 0;
    u32 temp_idx = read_idx;

    while (temp_idx != write_idx)
    {
        if (trace_buffer->buffer[temp_idx] == '\n')
        {
            total_messages++;
        }
        temp_idx = (temp_idx + 1) % buffer_size;
    }

    /* 如果请求的消息数量超过总数，调整count */
    if (count > total_messages)
    {
        count = total_messages;
        f_printf("Only %u messages available, adjusting count\n", count);
    }

    /* 找到第(total_messages - count)个消息的起始位置 */
    u32 target_message = total_messages - count;
    u32 message_count = 0;
    u32 message_start = read_idx;

    /* 先找到起始消息的位置 */
    temp_idx = read_idx;
    while (temp_idx != write_idx && message_count < target_message)
    {
        if (trace_buffer->buffer[temp_idx] == '\n')
        {
            message_count++;
            message_start = (temp_idx + 1) % buffer_size; /* 下一个消息的开始 */
        }
        temp_idx = (temp_idx + 1) % buffer_size;
    }

    /* 记录新的读索引位置（将在处理完所有消息后更新） */
    u32 new_read_index = message_start;

    /* 打印指定数量的消息 */
    u32 printed = 0;
    u32 current_idx = message_start;

    while (current_idx != write_idx && printed < count)
    {
        /* 查找消息结束 */
        u32 line_start = current_idx;
        u32 line_length = 0;
        boolean found_end = FALSE;

        while (current_idx != write_idx)
        {
            char c = trace_buffer->buffer[current_idx];
            current_idx = (current_idx + 1) % buffer_size;
            line_length++;

            if (c == '\n')
            {
                found_end = TRUE;
                /* 更新新的读索引位置（指向下一个消息的开始） */
                new_read_index = current_idx;
                break;
            }
        }

        /* 打印消息 */
        if (found_end)
        {
            f_printf("[%u/%u] ", ++printed, count);
            for (u32 i = 0; i < line_length; i++)
            {
                u32 idx = (line_start + i) % buffer_size;
                putchar(trace_buffer->buffer[idx]);
            }
            f_printf("\n");
        }
        else
        {
            /* 不完整的消息（不应该发生，因为我们已经计算了完整的消息数量） */
            f_printf("[Partial %u/%u] ", ++printed, count);
            for (u32 i = 0; i < line_length; i++)
            {
                u32 idx = (line_start + i) % buffer_size;
                putchar(trace_buffer->buffer[idx]);
            }
            f_printf("\n");
            /* 更新新的读索引位置 */
            new_read_index = current_idx;
        }
    }
    /* 更新读索引，标记这些消息为已读 */
    trace_buffer->read_index = new_read_index;
    // 确保内存同步
    DSB();
    f_printf("Final - Write Index: %u, Read Index: %u\n", trace_buffer->write_index,
             trace_buffer->read_index);
    f_printf("============================================\n");
}

/**
 * @brief 获取未读消息数量
 * 
 * 此函数计算并返回共享内存中未读消息的数量。
 * 未读消息是指从当前读索引到写索引之间的所有完整消息。
 * 
 * @return u32 未读消息数量，如果出错返回0
 */
u32 fget_unread_message_count(void)
{
    if (!trace_initialized)
    {
        f_printf("Trace buffer not initialized\n");
        return 0;
    }

    u32 read_idx = trace_buffer->read_index;
    u32 write_idx = trace_buffer->write_index;
    u32 buffer_size = TRACE_BUFFER_SIZE - 12;

    if (read_idx == write_idx)
    {
        return 0;
    }
    /* 计算未读消息数量 */
    u32 unread_count = 0;
    u32 current_idx = read_idx;

    while (current_idx != write_idx)
    {
        /* 查找消息结束位置（换行符） */
        while (current_idx != write_idx)
        {
            if (trace_buffer->buffer[current_idx] == '\n')
            {
                unread_count++;
                current_idx = (current_idx + 1) % buffer_size;
                break;
            }
            current_idx = (current_idx + 1) % buffer_size;
        }
    }
    return unread_count;
}

/**
 * @brief 标记所有消息为已读
 * 
 * 此函数将读索引设置为当前写索引的位置，
 * 从而标记所有消息为已读状态。
 */
void fmark_all_messages_read(void)
{
    if (!trace_initialized)
    {
        f_printf("Trace buffer not initialized\n");
        return;
    }

    /* 将读索引设置为写索引，标记所有消息为已读 */
    trace_buffer->read_index = trace_buffer->write_index;
    DSB();
    f_printf("All messages marked as read\n");
}
