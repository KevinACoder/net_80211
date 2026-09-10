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
 * FilePath: fgeneric_timer.h
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:33:07
 * Description:  This file provides the common helper routines for the generic timer API's 
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 *  1.0  huanghe	2021/11/13		initialization
 *  1.1  zhugengyu 2022/06/05     add tick api
 *  1.2  wangxiaodong 2023/05/29  modify api
 */


#ifndef FGENERIC_TIMER_H
#define FGENERIC_TIMER_H

#include "ftypes.h"
#include "fparameters.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef CONFIG_USE_EL2
#define GENERIC_PTIMER_IRQ_NUM GENERIC_PTIMER_EL1_IRQ_NUM
#else
#define GENERIC_PTIMER_IRQ_NUM GENERIC_PTIMER_EL2_NS_IRQ_NUM
#endif

#ifndef CONFIG_USE_EL2
#define GENERIC_VTIMER_IRQ_NUM GENERIC_VTIMER_EL1_IRQ_NUM
#else
#define GENERIC_VTIMER_IRQ_NUM GENERIC_VTIMER_EL2_NS_IRQ_NUM
#endif

/* Set generic timer CompareValue */
void GenericTimerSetTimerCompareValue(u32 id, u64 timeout);

/* Set generic timer TimerValue */
void GenericTimerSetTimerValue(u32 id, u32 timeout);

/* Unmask generic timer interrupt */
void GenericTimerInterruptEnable(u32 id);

/* Mask generic timer interrupt */
void GenericTimerInterruptDisable(u32 id);

/* Enable generic timer */
void GenericTimerStart(u32 id);

/* Get generic timer count value */
u64 GenericTimerRead(u32 id);

/* Get generic timer frequency of the system counter */
u64 GenericTimerFrequecy(void);

/* Disable generic timer */
void GenericTimerStop(u32 id);


#ifdef __cplusplus
}
#endif

#endif // !