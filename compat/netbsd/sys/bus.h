/*
 * @file
 * @brief bus_space shell: the imported drivers do MMIO through the
 * usbdi request API only, this header just has to exist.
 */

#ifndef _COMPAT_SYS_BUS_H_
#define _COMPAT_SYS_BUS_H_

#include <sys/cdefs.h>

typedef struct { int unused; } bus_dma_tag_t;
typedef struct { int unused; } bus_space_tag_t;
typedef uintptr_t bus_space_handle_t;
typedef uintptr_t bus_addr_t;
typedef uintptr_t bus_size_t;

#endif /* _COMPAT_SYS_BUS_H_ */
