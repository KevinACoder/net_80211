/* $NetBSD$ */
#ifndef _RTW88_LINUX_BCD_H_
#define _RTW88_LINUX_BCD_H_

#include "rtw88_compat.h"

static __always_inline __unused unsigned int
bcd2bin(unsigned char val)
{
	return (val & 0x0f) + (val >> 4) * 10;
}

static __always_inline __unused unsigned char
bin2bcd(unsigned int val)
{
	return ((val / 10) << 4) | (val % 10);
}

#endif
