/*
 * @file
 * @brief atomic(9) shell over compiler builtins.
 */

#ifndef _COMPAT_SYS_ATOMIC_H_
#define _COMPAT_SYS_ATOMIC_H_

#include <sys/cdefs.h>
#include <sys/types.h>

static inline void atomic_inc_uint(volatile unsigned int *p) {
	__atomic_add_fetch(p, 1, __ATOMIC_RELAXED);
}

static inline void atomic_dec_uint(volatile unsigned int *p) {
	__atomic_sub_fetch(p, 1, __ATOMIC_RELAXED);
}

static inline unsigned int atomic_inc_uint_nv(volatile unsigned int *p) {
	return __atomic_add_fetch(p, 1, __ATOMIC_RELAXED);
}

static inline unsigned int atomic_dec_uint_nv(volatile unsigned int *p) {
	return __atomic_sub_fetch(p, 1, __ATOMIC_RELAXED);
}

static inline void *atomic_swap_ptr(volatile void *p, void *v) {
	return __atomic_exchange_n((void *volatile *) p, v, __ATOMIC_RELAXED);
}

#endif /* _COMPAT_SYS_ATOMIC_H_ */
