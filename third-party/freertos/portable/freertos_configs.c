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
 * FilePath: freertos_configs.c
 * Date: 2022-02-24 13:42:19
 * LastEditTime: 2022-03-21 17:03:31
 * Description:  This file is for the freertos config functions
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0  wangxiaodong  2021/9/26  first release
 * 1.1  wangxiaodong  2021/12/24 adapt new standalone
 * 1.2  wangxiaodong  2022/6/20  v0.1.0
 * 2.0  wangxiaodong  2022/8/9   adapt E2000D
 * 2.1  liuzhihong    2023/1/12  improve lwip functions
 */
#include <stdio.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"
#include "ftypes.h"
#include "fparameters.h"
#include "fgeneric_timer.h"
#include "intr.h"
#include "fcpu_info.h"
#include "fassert.h"
#include "fexception.h"
#include "sdkconfig.h"

#ifdef CONFIG_NON_SECURE_PHYSICAL_TIMER
    #define USING_GENERIC_TIMER_ID GENERIC_TIMER_ID0
    #define USING_GENERIC_TIMER_IRQ_ID GENERIC_PTIMER_IRQ_NUM
#elif defined CONFIG_NON_SECURE_VIRTUAL_TIMER
    #define USING_GENERIC_TIMER_ID GENERIC_TIMER_ID1
    #ifdef CONFIG_TARGET_RK3568
        /* rk3568 (OP-TEE): CNTV (EL1 virtual timer) 的中断线实测接在
         * GIC PPI11 (INTID 27),不是标准 ARM 的 PPI14 (INTID 30)。
         * 实测:CNTV tval=1 触发后 GICR_ISPENDR0 bit27 置位,中断 27
         * 到达 vApplicationInterruptHandler。且 PPI27 是 OP-TEE 开放
         * 给 NS 世界的(GICR_ISENABLER0 bit27 写生效)。 */
        #define USING_GENERIC_TIMER_IRQ_ID 27
    #else
        #define USING_GENERIC_TIMER_IRQ_ID GENERIC_VTIMER_IRQ_NUM
    #endif
#endif

static volatile u32 is_in_irq = 0 ;

void vMainAssertCalled(const char *pcFileName, uint32_t ulLineNumber)
{
    printf("Assert Error is %s : %d .\r\n", pcFileName, ulLineNumber);
    for (;;)
        ;
}

void vApplicationMallocFailedHook(void)
{
    u32 cpu_id;
    GetCpuId(&cpu_id);
    printf("CPU %d Malloc Failed.\r\n", cpu_id);
    while (1)
        ;
}


_WEAK void vApplicationTickHook(void)
{
}

_WEAK void vApplicationIdleHook(void)
{
}

u32 PlatformGetGicDistBase(void)
{
    return GICV3_BASE_ADDR;
}

static u32 cntfrq; /* System frequency */

/* rk3568 (OP-TEE): 普通 FreeRTOS tick 用 EL1 physical timer (CNTP, PPI29),
 * 但本板 OP-TEE 把 PPI29 划为 Group0/安全组, NS 世界无法使能
 * (GICR_ISENABLER0 bit29 写无效)。EL2 physical timer (CNTHP, PPI26)
 * 虽开放给 NS (linux arch_timer 同款), 但 go 环境 CNTHCTL_EL2.EL1PCTEN=0,
 * EL1 无法编程 CNTHP_EL2。实测 CNTV (EL1 virtual timer) 可用:
 * EL1 可直接编程 CNTV_CTL_EL0/CNTV_TVAL_EL0, 且其中断线接 PPI27
 * (GICR_ISENABLER0 bit27 写生效, 见 USING_GENERIC_TIMER_IRQ_ID 注释)。
 * 因此 rk3568 的 FreeRTOS tick 用 CNTV + PPI27。 */
void vConfigureTickInterrupt(void)
{
    /* Disable the timer */
    GenericTimerStop(USING_GENERIC_TIMER_ID);
    /* Get system frequency */
    cntfrq = GenericTimerFrequecy();

    /* Set tick rate */
    GenericTimerSetTimerValue(USING_GENERIC_TIMER_ID, cntfrq / configTICK_RATE_HZ);
    GenericTimerInterruptEnable(USING_GENERIC_TIMER_ID);

    /* Set as the lowest priority */
    InterruptSetPriority(USING_GENERIC_TIMER_IRQ_ID, configKERNEL_INTERRUPT_PRIORITY);
    InterruptUmask(USING_GENERIC_TIMER_IRQ_ID);

    GenericTimerStart(USING_GENERIC_TIMER_ID);
}

void vClearTickInterrupt(void)
{
    GenericTimerSetTimerValue(USING_GENERIC_TIMER_ID, cntfrq / configTICK_RATE_HZ);
}

volatile unsigned int gCpuRuntime;

void vApplicationInterruptHandler(uint32_t ulICCIAR)
{
    is_in_irq ++;

    if (ulICCIAR < 8192)
    {
        /* Interrupts cannot be re-enabled until the source of the interrupt is
        cleared. The ID of the interrupt is obtained by bitwise ANDing the ICCIAR
        value with 0x3FF. */
        ulICCIAR = ulICCIAR & 0x3FFUL;
    }

    /* call handler function */
    if (ulICCIAR == USING_GENERIC_TIMER_IRQ_ID)
    {
        /* Generic Timer */
        gCpuRuntime++;
        FreeRTOS_Tick_Handler();
    }
    else
    {
        FExceptionInterruptHandler((void *)(uintptr)ulICCIAR);
    }
    is_in_irq --;
}


/**
 * @name: vApplicationInIrq
 * @msg:  Used to indicate whether you are currently in an interrupt
 * @return {int} 1:is in an irq ,0 is not in
 * @note:
 */

int vApplicationInIrq(void)
{
    return is_in_irq;
}

static InterruptDrvType intr_instance;

void vApplicationInitIrq(void)
{
    InterruptInit(&intr_instance, INTERRUPT_DRV_INTS_ID, INTERRUPT_ROLE_MASTER);
}


void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName)
{
    (void) pxTask;
    (void) pcTaskName;

    taskDISABLE_INTERRUPTS();
    FASSERT(FALSE);

}

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    /* If the buffers to be provided to the Idle task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
      state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
      Note that, as the array is necessarily of type StackType_t,
      configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
 * application must provide an implementation of `vApplicationGetTimerTaskMemory()
 * to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
      task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
      Note that, as the array is necessarily of type StackType_t,
      configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vPrintString(const char *pcString)
{
    /* Print the string, using a critical section as a crude method of mutual
    exclusion. */
    taskENTER_CRITICAL();
    {
        printf("%s\r\n", pcString);
    }
    taskEXIT_CRITICAL();
}

void vPrintStringAndNumber(const char *pcString, uint32_t ulValue)
{
    /* Print the string, using a critical section as a crude method of mutual
    exclusion. */
    taskENTER_CRITICAL();
    {
        printf("%s %lu\r\n", pcString, ulValue);
    }
    taskEXIT_CRITICAL();
}

void vPrintf(const char *format, ...)
{
    /* Print the string, using a critical section as a crude method of mutual exclusion. */
    taskENTER_CRITICAL();
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    taskEXIT_CRITICAL();
}

