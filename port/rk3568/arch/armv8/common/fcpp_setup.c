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
 * FilePath: fcpp_setup.c
 * Date: 2022-03-08 21:56:42
 * LastEditTime: 2022-03-15 11:10:40
 * Description:  This file is for cpp environment setup
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2023/05/19       first release
 */

#include "sdkconfig.h"
#include "fdebug.h"

#ifdef CONFIG_ENABLE_CXX

typedef void (*FCxxFunc)(void);

/* See: C++ Application Binary Interface Standard for the ARM 64-bit
   Architecture chapter 3.2.3. */
void FCxxInitGlobals(void)
{
    /* call constructors of static objects */
    extern FCxxFunc __init_array_start[];
    extern FCxxFunc __init_array_end[];
    FCxxFunc *func;

    for (func = __init_array_start; func < __init_array_end; func++)
    {
        (*func)();
    }
}

void FCxxDeInitGlobals(void)
{
    /* call deconstructors of static objects */
    extern FCxxFunc __fini_array_start[];
    extern FCxxFunc __fini_array_end[];
    FCxxFunc *func;

    for (func = __fini_array_start; func < __fini_array_end; func++)
    {
        (*func)();
    }
}

#endif