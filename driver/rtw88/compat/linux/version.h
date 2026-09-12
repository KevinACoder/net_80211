/* $NetBSD$ */
/* Linux version the imported rtw88 chip code is compiled against. */
#ifndef _RTW88_LINUX_VERSION_H_
#define _RTW88_LINUX_VERSION_H_

#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(6, 12, 0)
#define KERNEL_VERSION_STRING "6.12.0"

#endif
