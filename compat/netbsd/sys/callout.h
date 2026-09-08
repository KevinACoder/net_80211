/*
 * @file
 * @brief callout(9) shell over the host timer primitive.
 */

#ifndef _COMPAT_SYS_CALLOUT_H_
#define _COMPAT_SYS_CALLOUT_H_

#include <sys/cdefs.h>
#include "types.h"
#include <sys/mutex.h>

typedef void (*callout_fn_t)(void *);

/* embedded by driver softc; storage owned by the port */
struct host_callout {
	void *hc_timer;
	callout_fn_t hc_fn;
	void *hc_arg;
	int hc_pending;
};
typedef struct host_callout callout_t;

#define CALLOUT_MPSAFE 0
#define CALLOUT_STOPPING 1

int callout_init(callout_t *c, int flags);
int callout_setfunc(callout_t *c, callout_fn_t fn, void *arg);
int callout_schedule(callout_t *c, int ticks);
int callout_stop(callout_t *c);
int callout_halt(callout_t *c, kmutex_t *lock);
void callout_destroy(callout_t *c);
bool callout_pending(callout_t *c);

#endif /* _COMPAT_SYS_CALLOUT_H_ */
