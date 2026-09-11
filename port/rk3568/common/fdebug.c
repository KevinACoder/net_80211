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
 * FilePath: fdebug.c
 * Date: 2021-04-25 16:44:23
 * LastEditTime: 2022-02-17 18:04:50
 * Description:  This file is for providing debug functions.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 *  1.0  zhugengyu  2022/10/27   rename file name
 */
#include <string.h>
#include <stdarg.h>
#include "fdebug.h"
#include "fprintk.h"
#include "stdio.h"
#include "fspin.h"
#include "sdkconfig.h"

static mcs_node_t *debug_mcs_node = NULL;

void FDebugMcsLockNodeInit(uintptr addr)
{
    debug_mcs_node = (mcs_node_t *)addr;
}

void FDebugMcsLock(void)
{
    FMcsLock(mcs_lock_instance, debug_mcs_node);
}

void FDebugMcsUnlock(void)
{
    FMcsUnlock(mcs_lock_instance, debug_mcs_node);
}

static char log_buf[CONFIG_ELOG_LINE_BUF_SIZE] = {0};
#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
void FtDumpHexByte(const u8 *ptr, u32 buflen)
{
    u8 *buf = (u8 *)ptr;
    fsize_t i, j;

    for (i = 0; i < buflen; i += 16)
    {
        f_printk("%p: ", ptr + i);

        for (j = 0; j < 16; j++)
        {
            if (i + j < buflen)
            {
                f_printk("%02X ", buf[i + j]);
            }
            else
            {
                f_printk("   ");
            }
        }
        f_printk(" ");

        for (j = 0; j < 16; j++)
        {
            if (i + j < buflen)
            {
                f_printk("%c", (char)(__is_print(buf[i + j]) ? buf[i + j] : '.'));
            }
        }
        f_printk("\r\n");
    }
}

void FtDumpHexByteDebug(const u8 *ptr, u32 buflen)
{
    u8 *buf = (u8 *)ptr;
    fsize_t i, j;

    for (i = 0; i < buflen; i += 16)
    {
        f_printk("%x: ", ptr + i);

        for (j = 0; j < 16; j++)
        {
            if (i + j < buflen)
            {
                f_printk("%x ", buf[i + j]);
            }
            else
            {
                f_printk("   ");
            }
        }
        f_printk(" ");

        for (j = 0; j < 16; j++)
        {
            if (i + j < buflen)
            {
                f_printk("%c", (char)(__is_print(buf[i + j]) ? buf[i + j] : '.'));
            }
        }
        f_printk("\r\n");
    }
}

_WEAK void FtDumpExtraLogInfo(const char *tag, u32 log_level, const char *log_tag_letter,
                              const char *file_name, u32 line, const char *fmt, ...)
{
    LOG_SPIN_LOCK();
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_buf, sizeof(log_buf), fmt, ap);
    va_end(ap);
    do
    {
        DISPALY_CORE_NUM();
        if (strcmp(log_tag_letter, "31") == 0)
        {
            PORT_KPRINTF("\033[31m%s:%s @%s:%d\033[0m \r\n", tag, log_buf, file_name, line);
        }
        else if (strcmp(log_tag_letter, "32") == 0)
        {
            PORT_KPRINTF("\033[32m%s:%s @%s:%d\033[0m \r\n", tag, log_buf, file_name, line);
        }
        else if (strcmp(log_tag_letter, "33") == 0)
        {
            PORT_KPRINTF("\033[33m%s:%s @%s:%d\033[0m \r\n", tag, log_buf, file_name, line);
        }
        else if (strcmp(log_tag_letter, "36") == 0)
        {
            PORT_KPRINTF("\033[36m%s:%s @%s:%d\033[0m \r\n", tag, log_buf, file_name, line);
        }
        else
        {
            PORT_KPRINTF("\033[35m%s:%s @%s:%d\033[0m \r\n", tag, log_buf, file_name, line);
        }


    } while (0);

    memset(log_buf, 0, sizeof(log_buf));
    LOG_SPIN_UNLOCK();
}

_WEAK void FtDumpLogInfo(const char *tag, u32 log_level, const char *log_tag_letter,
                         const char *fmt, ...)
{
    LOG_SPIN_LOCK();
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_buf, sizeof(log_buf), fmt, ap);
    va_end(ap);

    do
    {
        DISPALY_CORE_NUM();
        if (strcmp(log_tag_letter, "31") == 0)
        {
            PORT_KPRINTF("\033[31m%s:%s\033[0m \r\n", tag, log_buf);
        }
        else if (strcmp(log_tag_letter, "32") == 0)
        {
            PORT_KPRINTF("\033[32m%s:%s\033[0m \r\n", tag, log_buf);
        }
        else if (strcmp(log_tag_letter, "33") == 0)
        {
            PORT_KPRINTF("\033[33m%s:%s\033[0m \r\n", tag, log_buf);
        }
        else if (strcmp(log_tag_letter, "36") == 0)
        {
            PORT_KPRINTF("\033[36m%s:%s\033[0m \r\n", tag, log_buf);
        }
        else
        {
            PORT_KPRINTF("\033[35m%s:%s\033[0m \r\n", tag, log_buf);
        }

    } while (0);

    memset(log_buf, 0, sizeof(log_buf));
    LOG_SPIN_UNLOCK();
}

void FtDumpHexWord(const u32 *ptr, u32 buflen)
{
    u32 *buf = (u32 *)ptr;
    u8 *char_data = (u8 *)ptr;
    fsize_t i, j;
    buflen = buflen / 4;
    for (i = 0; i < buflen; i += 4)
    {
        f_printk("%p: ", ptr + i);

        for (j = 0; j < 4; j++)
        {
            if (i + j < buflen)
            {
                f_printk("%lx ", buf[i + j]);
            }
            else
            {
                f_printk("   ");
            }
        }

        f_printk(" ");

        for (j = 0; j < 16; j++)
        {
            if (i + j < buflen)
            {
                f_printk("%c", (char)(__is_print(char_data[i + j]) ? char_data[i + j] : '.'));
            }
        }

        f_printk("\r\n");
    }
}
