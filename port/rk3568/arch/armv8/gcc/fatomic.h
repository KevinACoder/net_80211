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
 * FilePath: fatomic.h
 * Date: 2022-03-08 21:56:42
 * LastEditTime: 2022-03-15 11:14:45
 * Description:  This file is for l3 cache-related operations
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   wangxiaodong    2023/6/6       first release
 */

#ifndef FATOMIC_H
#define FATOMIC_H

#ifdef __cplusplus
extern "C"
{
#endif

/***************************** Basic Types *************************************/

/* 原子类型定义 */
typedef struct
{
    volatile int counter;
} FAtomicT;

typedef struct
{
    volatile long long counter;
} FAtomic64T;

typedef FAtomic64T FAtomicLongT;

/************************** Function Prototypes ******************************/

/* data atomic add val, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform the operation suggested by
 * the name, and returns the value that had previously been in memory." */
#define FATOMIC_ADD(data, val)  __sync_fetch_and_add(&(data), (val))
/* data atomic add 1, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform the operation suggested by
 * the name, and returns the value that had previously been in memory." */
#define FATOMIC_INC(data)       FATOMIC_ADD(data, 1)

/* data atomic subtract val, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform the operation suggested by
 * the name, and returns the value that had previously been in memory." */
#define FATOMIC_SUB(data, val)  __sync_fetch_and_sub(&(data), (val))
/* data atomic subtract 1, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform the operation suggested by
 * the name, and returns the value that had previously been in memory." */
#define FATOMIC_DEC(data)       FATOMIC_SUB(data, 1)

/* data atomic or val, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform the operation suggested by
 * the name, and returns the value that had previously been in memory." */
#define FATOMIC_OR(data, val)   __sync_fetch_and_or(&(data), (val))
/* data atomic xor val, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform the operation suggested by
 * the name, and returns the value that had previously been in memory." */
#define FATOMIC_XOR(data, val)  __sync_fetch_and_xor(&(data), (val))

/* data atomic and val, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform the operation suggested by
 * the name, and returns the value that had previously been in memory." */
#define FATOMIC_AND(data, val)  __sync_fetch_and_and(&(data), (val))
/* data atomic nand val, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform the operation suggested by
 * the name, and returns the value that had previously been in memory."
 * GCC 16 Manual, page 824: "Note: GCC 4.4 and later implement __sync_fetch_and_nand as
 * *ptr = ~(tmp & value) instead of *ptr = ~tmp & value." */
#define FATOMIC_NAND(data, val) __sync_fetch_and_nand(&(data), (val))

/* atomic compare data and cmpval
 * if not equal, return false
 * if equal, data = newval, return true
 * GCC 16 Manual, page 824: "These built-in functions perform an atomic compare and swap.
 * That is, if the current value of *ptr is oldval, then write newval into *ptr.
 * The 'bool' version returns true if the comparison is successful and newval is written." */
#define FATOMIC_CAS_BOOL(data, cmpval, newval) \
    __sync_bool_compare_and_swap(&(data), (cmpval), (newval))

/* atomic compare data and cmpval
 * if not equal, return data
 * if equal, data = newval, return initial data
 * GCC 16 Manual, page 824: "These built-in functions perform an atomic compare and swap.
 * That is, if the current value of *ptr is oldval, then write newval into *ptr.
 * The 'val' version returns the contents of *ptr before the operation." */
#define FATOMIC_CAS_VAL(data, cmpval, newval) \
    __sync_val_compare_and_swap(&(data), (cmpval), (newval))

/* full memory barrier
 * GCC 16 Manual, page 824: "This built-in function issues a full memory barrier."
 * GCC 16 Manual, page 823: "In most cases, these built-in functions are considered a full barrier.
 * That is, no memory operand is moved across the operation, either forward or backward.
 * Further, instructions are issued as necessary to prevent the processor from speculating loads
 * across the operation and from queuing stores after the operation." */
#define FATOMIC_MEM_BARRIER(data) __sync_synchronize()

/* set data = val, and lock data
 * GCC 16 Manual, page 825: "This built-in function, as described by Intel, is not a traditional
 * test-and-set operation, but rather an atomic exchange operation. It writes value into *ptr,
 * and returns the previous contents of *ptr."
 * GCC 16 Manual, page 825: "This built-in function is not a full barrier, but rather an acquire
 * barrier. This means that references after the operation cannot move to (or be speculated to)
 * before the operation, but previous memory stores may not be globally visible yet, and previous
 * memory loads may not yet be satisfied." */
#define FATOMIC_LOCK(data, val)   __sync_lock_test_and_set(&(data), val)

/* release data, set data = 0
 * GCC 16 Manual, page 825: "This built-in function releases the lock acquired by
 * __sync_lock_test_and_set. Normally this means writing the constant 0 to *ptr."
 * GCC 16 Manual, page 825: "This built-in function is not a full barrier, but rather a release
 * barrier. This means that all previous memory stores are globally visible, and all previous
 * memory loads have been satisfied, but following memory reads are not prevented from being
 * speculated to before the barrier." */
#define FATOMIC_UNLOCK(data)      __sync_lock_release(&(data))

/**************************** Layer 1: Low Dependency ****************************/

/* 内存屏障 */
// #define F_BARRIER() __asm__ __volatile__("": : :"memory")

/* 内存排序常量 */
#define F_MEMORY_ORDER_RELAXED    0
#define F_MEMORY_ORDER_ACQUIRE    1
#define F_MEMORY_ORDER_RELEASE    2
#define F_MEMORY_ORDER_ACQ_REL    3
#define F_MEMORY_ORDER_SEQ_CST    4

/* 1.1 基础读写 */
static inline int FAtomicRead(FAtomicT *v)
{
    return __atomic_load_n(&v->counter, __ATOMIC_SEQ_CST);
}

static inline void FAtomicSet(FAtomicT *v, int i)
{
    __atomic_store_n(&v->counter, i, __ATOMIC_SEQ_CST);
}

static inline long long FAtomic64Read(FAtomic64T *v)
{
    return __atomic_load_n(&v->counter, __ATOMIC_SEQ_CST);
}

static inline void FAtomic64Set(FAtomic64T *v, long long i)
{
    __atomic_store_n(&v->counter, i, __ATOMIC_SEQ_CST);
}

/* 1.2 CAS 核心实现
 * GCC 16 Manual, page 837: __atomic_add_fetch returns the result of the operation (new value) */
static inline int FAtomicAddReturn(int i, FAtomicT *v)
{
    return __atomic_add_fetch(&v->counter, i, __ATOMIC_SEQ_CST);
}

/* GCC 16 Manual, page 835: __atomic_exchange_n writes val into *ptr and returns the previous contents of *ptr (old value) */
static inline int FAtomicXchg(FAtomicT *v, int newVal)
{
    return __atomic_exchange_n(&v->counter, newVal, __ATOMIC_SEQ_CST);
}

/* 1.3 64位 CAS
 * GCC 16 Manual, page 837: __atomic_add_fetch returns the result of the operation (new value) */
static inline long long FAtomic64AddReturn(long long i, FAtomic64T *v)
{
    return __atomic_add_fetch(&v->counter, i, __ATOMIC_SEQ_CST);
}

/* 1.4 比较交换 */
static inline int FAtomicCmpXchg(FAtomicT *v, int old, int newVal)
{
    return __sync_val_compare_and_swap(&v->counter, old, newVal);
}

/* 1.5 位操作
 * GCC 16 Manual, page 837: __atomic_fetch_* returns the value that had previously been in *ptr (old value)
 * Note: 这些函数返回 void，仅执行原子操作而不使用返回值 */
static inline void FAtomicAnd(int i, FAtomicT *v)
{
    __atomic_fetch_and(&v->counter, i, __ATOMIC_SEQ_CST);
}

static inline void FAtomicOr(int i, FAtomicT *v)
{
    __atomic_fetch_or(&v->counter, i, __ATOMIC_SEQ_CST);
}

static inline void FAtomicXor(int i, FAtomicT *v)
{
    __atomic_fetch_xor(&v->counter, i, __ATOMIC_SEQ_CST);
}

/* 1.6 内存屏障
 * GCC 16 Manual, page 838: __atomic_thread_fence acts as a synchronization fence between threads based on the specified memory order */
static inline void FAtomicThreadFence(int order)
{
    __atomic_thread_fence(order);
}

/* 1.7 显式加载/存储
 * GCC 16 Manual, page 835: __atomic_load_n returns the contents of *ptr
 * GCC 16 Manual, page 835: __atomic_store_n writes val into *ptr
 * Note: Valid memory orders for __atomic_load_n: RELAXED, SEQ_CST, ACQUIRE, CONSUME
 * Note: Valid memory orders for __atomic_store_n: RELAXED, SEQ_CST, RELEASE
 * For ACQ_REL, we use SEQ_CST as a safe fallback */
static inline int FAtomicLoadExplicit(const volatile int *ptr, int memoryOrder)
{
    switch (memoryOrder)
    {
        case F_MEMORY_ORDER_RELAXED:
            return __atomic_load_n(ptr, __ATOMIC_RELAXED);
        case F_MEMORY_ORDER_ACQUIRE:
            return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
        case F_MEMORY_ORDER_SEQ_CST:
        case F_MEMORY_ORDER_ACQ_REL:
        default:
            return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
    }
}

static inline void FAtomicStoreExplicit(volatile int *ptr, int value, int memoryOrder)
{
    switch (memoryOrder)
    {
        case F_MEMORY_ORDER_RELAXED:
            __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
            break;
        case F_MEMORY_ORDER_RELEASE:
            __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
            break;
        case F_MEMORY_ORDER_SEQ_CST:
        default:
            __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
            break;
    }
}

/* 1.8 获取并操作
 * GCC 16 Manual, page 837: __atomic_fetch_add returns the value that had previously been in *ptr (old value) */
static inline int FAtomicFetchAdd(int i, FAtomicT *v)
{
    return __atomic_fetch_add(&v->counter, i, __ATOMIC_SEQ_CST);
}

static inline int FAtomicFetchAndNotRelaxed(int flags, FAtomicT *v)
{
    return __atomic_fetch_and(&v->counter, ~flags, __ATOMIC_SEQ_CST);
}

/* 1.9 条件读取宏
 * GCC 16 Manual, page 835: __atomic_load performs atomic load with specified memory order
 * Note: 条件应使用宏内部 val，例如 F_ATOMIC_COND_READ_RELAXED(v, 5) 表示 val >= 5
 * 使用 __ATOMIC_SEQ_CST 确保与 FAtomicSet (SEQ_CST) 兼容 */
#define F_ATOMIC_COND_READ_RELAXED(v, min_val)                    \
    ({                                                            \
        __typeof__((v)->counter) val;                             \
        do                                                        \
        {                                                         \
            __atomic_load(&(v)->counter, &val, __ATOMIC_SEQ_CST); \
        } while (val < (min_val));                                \
        val;                                                      \
    })

#define F_ATOMIC_COND_READ_ACQUIRE(v, min_val)                    \
    ({                                                            \
        __typeof__((v)->counter) val;                             \
        do                                                        \
        {                                                         \
            __atomic_load(&(v)->counter, &val, __ATOMIC_ACQUIRE); \
        } while (val < (min_val));                                \
        val;                                                      \
    })

#define F_ATOMIC_LONG_COND_READ_RELAXED(v, min_val)               \
    ({                                                            \
        __typeof__((v)->counter) val;                             \
        do                                                        \
        {                                                         \
            __atomic_load(&(v)->counter, &val, __ATOMIC_SEQ_CST); \
        } while (val < (min_val));                                \
        val;                                                      \
    })

/**************************** Layer 2: 复合依赖代码层 ****************************/

/* 2.1 依赖 FAtomicAddReturn 的函数 */
static inline int FAtomicSubReturn(int i, FAtomicT *v)
{
    return FAtomicAddReturn(-i, v);
}

static inline void FAtomicAdd(int i, FAtomicT *v)
{
    FAtomicAddReturn(i, v);
}

static inline void FAtomicSub(int i, FAtomicT *v)
{
    FAtomicAddReturn(-i, v);
}

static inline int FAtomicIncReturn(FAtomicT *v)
{
    return FAtomicAddReturn(1, v);
}

/* 2.2 依赖 FAtomicSubReturn 的函数 */
static inline int FAtomicDecReturn(FAtomicT *v)
{
    return FAtomicSubReturn(1, v);
}

/* 2.3 依赖 FAtomicIncReturn/FAtomicDecReturn 的函数 */
static inline void FAtomicInc(FAtomicT *v)
{
    FAtomicIncReturn(v);
}

static inline void FAtomicDec(FAtomicT *v)
{
    FAtomicDecReturn(v);
}

/* 2.4 依赖 FAtomicCmpXchg 的函数 */
static inline int FAtomicTestAndSet(FAtomicT *v)
{
    return FAtomicCmpXchg(v, 0, 1);
}

/* 2.5 依赖 FAtomicSet 的函数 */
static inline void FAtomicClear(FAtomicT *v)
{
    FAtomicSet(v, 0);
}

/* 2.6 64位操作 */
static inline long long FAtomic64SubReturn(long long i, FAtomic64T *v)
{
    return FAtomic64AddReturn(-i, v);
}

static inline void FAtomic64Add(long long i, FAtomic64T *v)
{
    FAtomic64AddReturn(i, v);
}

static inline void FAtomic64Sub(long long i, FAtomic64T *v)
{
    FAtomic64AddReturn(-i, v);
}

/* 2.7 释放语义操作
 * GCC 16 Manual, page 837: __atomic_sub_fetch returns the result of the operation (new value)
 * GCC 16 Manual, page 837: __atomic_fetch_sub returns the value that had previously been in *ptr (old value)
 * Using __atomic_sub_fetch with __ATOMIC_RELEASE to ensure release semantics and return new value */
static inline int FAtomicDecReturnRelease(FAtomicT *v)
{
    return __atomic_sub_fetch(&v->counter, 1, __ATOMIC_RELEASE);
}

static inline long long FAtomic64DecReturnRelease(FAtomic64T *v)
{
    return __atomic_sub_fetch(&v->counter, 1, __ATOMIC_RELEASE);
}

/* 2.8 复合宏定义
 * GCC 16 Manual, page 835: __atomic_store_n writes val into *ptr with specified memory order
 * GCC 16 Manual, page 837: __atomic_fetch_xor returns the value that had previously been in *ptr (old value)
 * Note: 使用 __typeof__ 避免强制类型转换导致 strict aliasing 违规 */
#define F_ATOMIC_SET_RELEASE(v, i)                    \
    do                                                \
    {                                                 \
        __typeof__(&(v)->counter) _p = &(v)->counter; \
        __typeof__((v)->counter) _tmp = (i);          \
        __atomic_store(_p, &_tmp, __ATOMIC_RELEASE);  \
    } while (0)

#define F_ATOMIC_LONG_XOR(i, v)                                   \
    do                                                            \
    {                                                             \
        __atomic_fetch_xor(&(v)->counter, (i), __ATOMIC_SEQ_CST); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
