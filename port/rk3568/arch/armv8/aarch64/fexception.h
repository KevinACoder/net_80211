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
 * FilePath: fexception.h
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:32:53
 * Description:  This file contains low-level driver functions for the processor exception
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe    2021/7/3     first release
 * 1.1  zhugengyu	2022/6/3		add debugging information	
 * 1.2   wangxiaodong 2023/2/23	  add nested interrupt enable and disable	

 */
#ifndef FEXCEPTION_H
#define FEXCEPTION_H

/***************************** Include Files *********************************/
#include "ftypes.h"
#include "fassert.h"
#include "faarch.h"

#ifdef __cplusplus
extern "C"
{
#endif

/************************** Constant Definitions *****************************/
#define FEXC_FRAME_SIZE 720U


/****************************************************************************/
/**
* @brief	Enable nested interrupts by clearing the I bit in DAIF.
* @note     Caution: 
            This macro must be used in interrupt handler.
            The value parameter must be a global or static variable, not a local variable.
******************************************************************************/
static inline void FInterruptNestedEnable(fsize_t *value)
{
    u64 daif_value;
#ifndef CONFIG_USE_EL2
    value[0] = AARCH64_READ_SYSREG(ELR_EL1);
    value[1] = AARCH64_READ_SYSREG(SPSR_EL1);
#else
    value[0] = AARCH64_READ_SYSREG(ELR_EL2);
    value[1] = AARCH64_READ_SYSREG(SPSR_EL2);
#endif
    daif_value = MFCPSR();
    daif_value &= ~(0x1ULL << 7);
    MTCPSR(daif_value);
    DSB();
}

/****************************************************************************/
/**
* @brief	Disable the nested interrupts by setting the I bit in DAIF.
* @note     Caution: 
            This macro must be used in interrupt handler.
            The value parameter must be a global or static variable, not a local variable.
******************************************************************************/
static inline void FInterruptNestedDisable(fsize_t *value)
{
    u64 daif_value;
#ifndef CONFIG_USE_EL2
    AARCH64_WRITE_SYSREG(ELR_EL1, value[0]);
    AARCH64_WRITE_SYSREG(SPSR_EL1, value[1]);
#else
    AARCH64_WRITE_SYSREG(ELR_EL2, value[0]);
    AARCH64_WRITE_SYSREG(SPSR_EL2, value[1]);
#endif
    daif_value = MFCPSR();
    daif_value |= (0x1 << 7);
    MTCPSR(daif_value);
    DSB();
}


/**************************** Type Definitions *******************************/
typedef struct
{
    u64 spsr;
    u64 sp;
    u64 cpacr;
    u64 elr;
    u64 q[64];
    u64 x29;
    u64 x30;
    u64 x18;
    u64 x19;
    u64 x16;
    u64 x17;
    u64 x14;
    u64 x15;
    u64 x12;
    u64 x13;
    u64 x10;
    u64 x11;
    u64 x8;
    u64 x9;
    u64 x6;
    u64 x7;
    u64 x4;
    u64 x5;
    u64 x2;
    u64 x3;
    u64 x0;
    u64 x1;
} FExcFrame;

FASSERT_STATIC(sizeof(FExcFrame) == FEXC_FRAME_SIZE);

typedef void (*FExcInterruptEndHandler)(void);
/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/
void FExcRegisterSyncEndHandler(FExcInterruptEndHandler handler);
void FExcRegisterSerrEndHandler(FExcInterruptEndHandler handler);

/************************** Variable Definitions *****************************/
void FExceptionInterruptHandler(void *temp);
/*****************************************************************************/

#ifdef __cplusplus
}
#endif


#endif