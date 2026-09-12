/* $NetBSD$ */
/*
 * Polling read helpers.  The chip code uses these to wait for a hardware
 * register bit to settle; the NetBSD equivalents of the timekeeping and
 * sleep primitives live in rtw88_compat.h.
 */
#ifndef _RTW88_LINUX_IOPOLL_H_
#define _RTW88_LINUX_IOPOLL_H_

#include "rtw88_compat.h"

#define	read_poll_timeout(op, val, cond, sleep_us, timeout_us,		\
			  sleep_before_read, args...)			\
({									\
	u64 __timeout_us = (timeout_us);				\
	unsigned long __sleep_us = (sleep_us);				\
	ktime_t __timeout = ktime_add_us(ktime_get(), __timeout_us);	\
	int __ret;							\
	if (sleep_before_read && __sleep_us)				\
		usleep_range((__sleep_us >> 2) + 1, __sleep_us);	\
	for (;;) {							\
		(val) = op(args);					\
		if (cond)						\
			break;						\
		if (__timeout_us &&					\
		    ktime_compare(ktime_get(), __timeout) > 0) {	\
			(val) = op(args);				\
			break;						\
		}							\
		if (__sleep_us)						\
			usleep_range((__sleep_us >> 2) + 1, __sleep_us);\
	}								\
	__ret = (cond) ? 0 : -ETIMEDOUT;				\
	__ret;								\
})

#define	read_poll_timeout_atomic(op, val, cond, delay_us, timeout_us,	\
				 delay_before_read, args...)		\
({									\
	u64 __timeout_us = (timeout_us);				\
	unsigned long __delay_us = (delay_us);				\
	ktime_t __timeout = ktime_add_us(ktime_get(), __timeout_us);	\
	int __ret;							\
	if (delay_before_read && __delay_us)				\
		udelay(__delay_us);					\
	for (;;) {							\
		(val) = op(args);					\
		if (cond)						\
			break;						\
		if (__timeout_us &&					\
		    ktime_compare(ktime_get(), __timeout) > 0) {	\
			(val) = op(args);				\
			break;						\
		}							\
		if (__delay_us)						\
			udelay(__delay_us);				\
	}								\
	__ret = (cond) ? 0 : -ETIMEDOUT;				\
	__ret;								\
})

#define	readx_poll_timeout(op, addr, val, cond, sleep_us, timeout_us)	\
	read_poll_timeout(op, val, cond, sleep_us, timeout_us, false, addr)

#endif
