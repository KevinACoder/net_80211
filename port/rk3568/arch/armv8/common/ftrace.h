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
 * FilePath: ftrace.h
 * Date: 2022-06-09 16:35:50
 * LastEditTime: 2022-06-09 16:35:51
 * Description:  This file is for trace macro definition
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/6/13    first release
 */
#ifndef FTRACE_H
#define FTRACE_H

/***************************** Include Files *********************************/
#if !defined(__ASSEMBLER__)
#include "ftypes.h"
#endif

#include "fparameters.h"

#ifdef __cplusplus
extern "C"
{
#endif

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
#define FTRACE_UART_BASE   FUART1_BASE_ADDR           /* UART-1 as trace Uart */
#define FTRACE_UART_UARTDR (FTRACE_UART_BASE + 0x0U)  /* UART data register offset */
#define FTRACE_UART_UARTFR (FTRACE_UART_BASE + 0x18U) /* UART status register offset */

/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif