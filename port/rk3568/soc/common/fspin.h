/*
 * Copyright (C) 2024, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fspin.h
 * Date: 2024-08-08 14:53:42
 * LastEditTime: 2024-08-08 17:58:18
 * Description:  This file is for spinlock function
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 *  1.0   carl       2023-02-28   Use GCC built-in functions to implement spinlock
 *  1.1   wangxiaodong 2024-08-08   Use mcs lock to implement spinlock
 */


#ifndef COMMON_FSPIN_H
#define COMMON_FSPIN_H

#include "ftypes.h"
#include "fatomic.h"
#include "faarch.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Layer 0: 数据结构层
 * 依赖: fatomic.h (FAtomicT), faarch.h (内存屏障)
 */

/* ARMv8 内存屏障 - Inner Shareable Domain */
/*
ARM DDI 0487 L.a - 14391 

  In AArch32 state, where the barrier function in the litmus test can be achieved by a DMB ST,            
  that is a barrier to stores only, this is shown by the use of DMB [ST]. This indicates that
  the ST qualifier can be omitted without affecting the result of the test. In some implementations
  DMB ST is faster than DMB.

  For AArch64 code, the shareability domain of the DMB or DSB must be included. This is shown in
  this manual using the notation DMB <domain> and DSB <domain> respectively.
*/

#if defined(__aarch64__)
#define FSMP_MB()                     \
    __asm__ __volatile__("dmb ish" :: \
                             : "memory") /*  Inner Shareable, reads and writes   */
#define FSMP_RMB()                      \
    __asm__ __volatile__("dmb ishld" :: \
                             : "memory") /* Inner Shareable, reads before, reads+writes after   */
#define FSMP_WMB() \
    __asm__ __volatile__("dmb ishst" ::: "memory") /* Inner Shareable, writes only   */
#else


#define FSMP_MB()  __asm__ __volatile__("dmb" ::: "memory")
#define FSMP_RMB() __asm__ __volatile__("dmb" ::: "memory")
#define FSMP_WMB() __asm__ __volatile__("dmb" ::: "memory")
#endif


/* 自旋锁类型 - 基于 fatomic.h 中的 FAtomicT */
typedef FAtomicT FSpinlockT;


/* mcs node struct */
typedef struct mcs_node
{
    struct mcs_node *next;
    volatile boolean locked;
} mcs_node_t;

/* mcs lock struct*/
typedef struct
{
    volatile mcs_node_t *tail;
    int is_ready;
} mcs_lock_t;

extern mcs_lock_t *mcs_lock_instance;

void FMcsLockInit(mcs_lock_t *lock);
void FMcsLock(mcs_lock_t *lock, mcs_node_t *node);
void FMcsUnlock(mcs_lock_t *lock, mcs_node_t *node);

/*
 * Layer 1: 低依赖代码层
 * 依赖: fatomic.h (FAtomicT), fatomic.h 原子操作函数
 */

/**
 * FSpinLockInit - 初始化自旋锁
 * @lock: 指向自旋锁的指针
 *
 * 将自旋锁初始化为未锁定状态 (值为0)
 */
void FSpinLockInit(FSpinlockT *lock);

/**
 * FSpinLockTrylock - 尝试获取自旋锁
 * @lock: 指向自旋锁的指针
 * @return: 获取成功返回非零值，失败返回0
 *
 * 尝试以原子操作方式获取锁，如果锁已被占用则立即返回
 */
int FSpinLockTrylock(FSpinlockT *lock);

/*
 * Layer 2: 复合依赖代码层
 * 依赖: FSpinlockT, SMP_MB, faarch.h 临界保护接口
 */

/**
 * FSpinLock - 获取自旋锁
 * @lock: 指向自旋锁的指针
 *
 * 自旋等待获取锁，成功获取后返回
 */
void FSpinLock(FSpinlockT *lock);

/**
 * FSpinUnlock - 释放自旋锁
 * @lock: 指向自旋锁的指针
 *
 * 释放锁并唤醒等待该锁的CPU
 */
void FSpinUnlock(FSpinlockT *lock);

/**
 * FSpinLockIrqSave - 获取自旋锁并保存中断状态
 * @lock: 指向自旋锁的指针
 * @flags: 用于保存中断状态的变量
 *
 * 先保存当前中断状态并禁用中断，然后获取自旋锁
 */
void FSpinLockIrqSave(FSpinlockT *lock, u32 *flags);

/**
 * FSpinUnlockIrqRestore - 释放自旋锁并恢复中断状态
 * @lock: 指向自旋锁的指针
 * @flags: 之前保存的中断状态
 *
 * 先释放自旋锁，然后恢复之前保存的中断状态
 */
void FSpinUnlockIrqRestore(FSpinlockT *lock, u32 *flags);

/**
 * FSpinLockIrq - 获取自旋锁并禁用中断
 * @lock: 指向自旋锁的指针
 *
 * 禁用IRQ和FIQ中断，然后获取自旋锁
 */
void FSpinLockIrq(FSpinlockT *lock);

/**
 * FSpinUnlockIrq - 释放自旋锁并启用中断
 * @lock: 指向自旋锁的指针
 *
 * 释放自旋锁，然后启用IRQ和FIQ中断
 */
void FSpinUnlockIrq(FSpinlockT *lock);

/**
 * FSpinLockBh - 获取自旋锁并禁用软中断
 * @lock: 指向自旋锁的指针
 *
 * 获取自旋锁 (软中断在ARMv8中通过抢占控制)
 */
void FSpinLockBh(FSpinlockT *lock);

/**
 * FSpinUnlockBh - 释放自旋锁并启用软中断
 * @lock: 指向自旋锁的指针
 *
 * 释放自旋锁 (软中断在ARMv8中通过抢占控制)
 */
void FSpinUnlockBh(FSpinlockT *lock);

#ifdef __cplusplus
}
#endif


#endif // COMMON_FSPIN_H