/*
 * @file
 * @brief sysctl(7) stub: the whole tree compiles out.
 *
 * The imported glue wraps its sysctl nodes in SYSCTL_SETUP blocks;
 * with the macros below they expand to nothing, no registration
 * happens, and the runtime knobs keep their defaults.
 */

#ifndef _COMPAT_SYS_SYSCTL_H_
#define _COMPAT_SYS_SYSCTL_H_

#include <sys/cdefs.h>

struct sysctllog { int unused; };
typedef struct sysctllog sysctllog;
#define SYSCTL_VERS 0

#define CTL_KERN 1
#define CTL_HW 6
#define KERN_SYSVIPC 39

#define SYSCTL_SETUP(name, desc) static void name(void)
#define sysctl_createv(log, flags, rnode, node, kind, qtype, name, \
    descr, ...)

enum {
	SYSCTL_INT = 1, SYSCTL_QUAD, SYSCTL_STRING, SYSCTL_NODE,
};

#define CTLFLAG_READWRITE 0
#define CTLFLAG_READONLY 0
#define CTLFLAG_HIDDEN 0
#define CTLTYPE_INT 0
#define CTLFLAG_PERMANENT 0

#endif /* _COMPAT_SYS_SYSCTL_H_ */
