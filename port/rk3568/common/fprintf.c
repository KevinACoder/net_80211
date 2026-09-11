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
 * FilePath: fprintf.c
 * Date: 2021-08-23 16:24:02
 * LastEditTime: 2022-02-17 18:01:19
 * Description:  This file is for creating custom print interface for standlone sdk.
 *
 * Modify History:
 *  Ver     Who           Date                  Changes
 * -----   ------       --------     --------------------------------------
 *  1.0    huanghe      2022/7/23            first release
 */


#include <stdarg.h>
#include "console.h"

#define PAD_RIGHT 1
#define PAD_ZERO  2
static void printchar(char **str, int c)
{
    if (str)
    {
        **str = c;
        ++(*str);
    }
    else
    {
        printf_call((const char)c);
    }
}

static int prints(char **out, const char *string, int width, int pad)
{
    register int pc = 0, padchar = (int)' ';

    if (width > 0)
    {
        register int len = 0;
        register const char *ptr;
        for (ptr = string; *ptr; ++ptr)
        {
            ++len;
        }
        if (len >= width)
        {
            width = 0;
        }
        else
        {
            width -= len;
        }
        if (pad & PAD_ZERO)
        {
            padchar = (int)'0';
        }
    }
    if (!(pad & PAD_RIGHT))
    {
        for (; width > 0; --width)
        {
            printchar(out, padchar);
            ++pc;
        }
    }
    for (; *string; ++string)
    {
        printchar(out, *string);
        ++pc;
    }
    for (; width > 0; --width)
    {
        printchar(out, padchar);
        ++pc;
    }

    return pc;
}

/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 12

static int printi(char **out, int i, int b, int sg, int width, int pad, int letbase)
{
    char print_buf[PRINT_BUF_LEN];
    register char *s;
    register int t, neg = 0, pc = 0;
    register unsigned int u = i;

    if (i == 0)
    {
        print_buf[0] = '0';
        print_buf[1] = '\0';
        return prints(out, print_buf, width, pad);
    }

    if (sg && b == 10 && i < 0)
    {
        neg = 1;
        u = -i;
    }

    s = print_buf + PRINT_BUF_LEN - 1;
    *s = '\0';

    while (u)
    {
        t = u % b;
        if (t >= 10)
        {
            t += letbase - (int)'0' - 10;
        }
        *--s = t + '0';
        u /= b;
    }

    if (neg)
    {
        if (width && (pad & PAD_ZERO))
        {
            printchar(out, '-');
            ++pc;
            --width;
        }
        else
        {
            *--s = '-';
        }
    }

    return pc + prints(out, s, width, pad);
}

static int print(char **out, const char *format, va_list args)
{
    register int width, pad;
    register int pc = 0;
    char scr[2];

    for (; *format != 0; ++format)
    {
        if (*format == '%')
        {
            ++format;
            width = pad = 0;
            if (*format == '\0')
            {
                break;
            }
            if (*format == '%')
            {
                goto out;
            }
            if (*format == '-')
            {
                ++format;
                pad = PAD_RIGHT;
            }
            while (*format == '0')
            {
                ++format;
                pad |= PAD_ZERO;
            }
            for (; *format >= '0' && *format <= '9'; ++format)
            {
                width *= 10;
                width += *format - '0';
            }
            if (*format == 's')
            {
                //register char *s = (char *)va_arg( args, int );
                register char *s = (char *)va_arg(args, char *);
                pc += prints(out, s ? s : "(null)", width, pad);
                continue;
            }
            if (*format == 'd')
            {
                pc += printi(out, va_arg(args, int), 10, 1, width, pad, 'a');
                continue;
            }
            if (*format == 'x')
            {
                pc += printi(out, va_arg(args, int), 16, 0, width, pad, 'a');
                continue;
            }
            if (*format == 'X')
            {
                pc += printi(out, va_arg(args, int), 16, 0, width, pad, 'A');
                continue;
            }
            if (*format == 'u')
            {
                pc += printi(out, va_arg(args, int), 10, 0, width, pad, 'a');
                continue;
            }
            if (*format == 'c')
            {
                /* char are converted to int then pushed on the stack */
                scr[0] = (char)va_arg(args, int);
                scr[1] = '\0';
                pc += prints(out, scr, width, pad);
                continue;
            }
        }
        else
        {
        out:
            printchar(out, *format);
            ++pc;
        }
    }
    if (out)
    {
        **out = '\0';
    }
    va_end(args);
    return pc;
}

int f_printf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    return print(0, format, args);
}

#ifdef TEST_PRINTF
int test_printf(void)
{
    char *ptr = "Hello world!";
    char *np = 0;
    int i = 5;
    unsigned int bs = sizeof(int) * 8;
    int mi;
    char buf[80];

    mi = (1 << (bs - 1)) + 1;
    f_printf("%s\n", ptr);
    f_printf("printf test\n");
    f_printf("%s is null pointer\n", np);
    f_printf("%d = 5\n", i);
    f_printf("%d = - max int\n", mi);
    f_printf("char %c = 'a'\n", 'a');
    f_printf("hex %x = ff\n", 0xff);
    f_printf("hex %02x = 00\n", 0);
    f_printf("signed %d = unsigned %u = hex %x\n", -3, -3, -3);
    f_printf("%d %s(s)%", 0, "message");
    f_printf("\n");
    f_printf("%d %s(s) with %%\n", 0, "message");
    sprintf(buf, "justif: \"%-10s\"\n", "left");
    f_printf("%s", buf);
    sprintf(buf, "justif: \"%10s\"\n", "right");
    f_printf("%s", buf);
    sprintf(buf, " 3: %04d zero padded\n", 3);
    f_printf("%s", buf);
    sprintf(buf, " 3: %-4d left justif.\n", 3);
    f_printf("%s", buf);
    sprintf(buf, " 3: %4d right justif.\n", 3);
    f_printf("%s", buf);
    sprintf(buf, "-3: %04d zero padded\n", -3);
    f_printf("%s", buf);
    sprintf(buf, "-3: %-4d left justif.\n", -3);
    f_printf("%s", buf);
    sprintf(buf, "-3: %4d right justif.\n", -3);
    f_printf("%s", buf);

    return 0;
}

#endif
