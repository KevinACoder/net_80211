/*
 * @file
 * @brief Basic types for the NetBSD-imported sources.
 */

#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/cdefs.h>

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

typedef uintptr_t vaddr_t;
typedef uintptr_t paddr_t;
typedef uintptr_t vsize_t;

#endif /* _SYS_TYPES_H_ */
