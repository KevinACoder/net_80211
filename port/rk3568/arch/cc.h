/*
 * @file cc.h
 * @brief lwIP architecture hook for the FreeRTOS test system.
 *
 * Intentionally minimal: lwIP 2.2's arch.h provides the defaults
 * (little-endian aarch64, GCC packed attributes, stdio diagnostics)
 * and the contrib FreeRTOS sys_arch implements SYS_ARCH protection.
 *
 * @author zhugengyu
 * @date 10.09.2026
 */

#ifndef LWIP_ARCH_CC_H_
#define LWIP_ARCH_CC_H_

/* newlib provides ssize_t/SSIZE_MAX; defining SSIZE_MAX here keeps
 * lwIP's arch.h from typedef-ing its own conflicting ssize_t */
#include <sys/types.h>
#include <limits.h>
#ifndef SSIZE_MAX
#define SSIZE_MAX __INT_MAX__
#endif

#endif /* LWIP_ARCH_CC_H_ */
