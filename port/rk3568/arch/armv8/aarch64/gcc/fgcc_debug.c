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
 * FilePath: fgcc_debug.c
 * Date: 2023-01-03 15:29:17
 * LastEditTime: 2023-01-03 15:29:17
 * Description:  This file is for c debug print in assembly language
 * 
 * Modify History: 
 *  Ver   Who  Date   Changes
 * ----- ------  -------- --------------------------------------
 *  1.0  huanghe	2021/11/13		initialization
 *  1.1  zhugengyu	2022/06/05		add debugging information		
 */


#include <stdio.h>
#include "fprintk.h"
#include "sdkconfig.h"
#include "fparameters.h"
#include "fearly_uart.h"


void SyncDoubleIn(void)
{
    f_printk("SyncDoubleIn \r\n");

    while (1)
    {
        /* code */
    }
}

void FloatSave(void)
{
}


void HangPrint(void)
{
    printf_call('h');
    printf_call('a');
    printf_call('n');
    printf_call('g');
    while (1)
    {
        ;
    }
}

void TestCode(void)
{
    printf_call('h');
}
