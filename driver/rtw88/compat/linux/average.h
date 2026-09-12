/* $NetBSD$ */
/*
 * Fixed point exponentially weighted moving average, API compatible with the
 * Linux DECLARE_EWMA() macro the chip code uses for RSSI/EVM/SNR smoothing.
 * internal holds the average scaled by 2^precision; each sample decays the
 * previous value by 1/weight_rcp and adds the new one.
 */
#ifndef _RTW88_LINUX_AVERAGE_H_
#define _RTW88_LINUX_AVERAGE_H_

#include "rtw88_compat.h"

#define	DECLARE_EWMA(name, _precision, _weight_rcp)			\
	struct ewma_##name {						\
		unsigned long internal;					\
	};								\
	static __always_inline __unused void					\
	ewma_##name##_init(struct ewma_##name *e)			\
	{								\
		e->internal = 0;					\
	}								\
	static __always_inline __unused void					\
	ewma_##name##_add(struct ewma_##name *e, unsigned long val)	\
	{								\
		unsigned long internal = e->internal;			\
									\
		if (internal == 0)					\
			internal = val << (_precision);			\
		else							\
			internal = internal -				\
			    (internal >> __builtin_ctzl(_weight_rcp)) +	\
			    (val << (_precision)) -			\
			    ((val << (_precision)) >>			\
			     __builtin_ctzl(_weight_rcp));		\
		e->internal = internal;					\
	}								\
	static __always_inline __unused unsigned long				\
	ewma_##name##_read(struct ewma_##name *e)			\
	{								\
		return e->internal >> (_precision);			\
	}								\
	struct __ewma_##name##_semicolon

#endif
