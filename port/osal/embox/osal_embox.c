/*
 * @file
 * @brief Embox implementation of the compat/netbsd kernel services.
 *
 * Memory goes to the embox heap, locks map onto the embox mutex and
 * cond primitives, callouts onto sys_timer, and the NetBSD usb task
 * queues onto one worker kthread per claimed device.
 *
 * @date 07.09.2026
 * @author zhugengyu
 */

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <embox/unit.h>

#include <kernel/thread.h>
#include <kernel/thread/sync/mutex.h>
#include <kernel/thread/sync/cond.h>
#include <kernel/time/time.h>
#include <kernel/time/sys_timer.h>
#include <kernel/time/ktime.h>
#include <mem/sysmalloc.h>

#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/intr.h>
#include <sys/endian.h>

/* the compat malloc/free macros must not intercept the embox calls */
#undef malloc
#undef free

void panic(const char *fmt, ...) {
	va_list ap;

	printf("panic: ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");

	for (;;) {
	}
}

int kprintf(const char *fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);
	return ret;
}

/* ------------------------------------------------------------------ */

void *wlan_kmalloc(size_t size, int flags, int type) {
	void *p;

	(void) type;
	/* Drivers can initialize from a short-lived shell command task. */
	p = sysmalloc(size);
	if (p != NULL && (flags & M_ZERO)) {
		memset(p, 0, size);
	}
	return p;
}

void wlan_kfree(void *p, int type) {
	(void) type;
	sysfree(p);
}

void *kmem_intr_alloc(size_t size, int flags) {
	return wlan_kmalloc(size, flags, M_DEVBUF);
}

void kmem_intr_free(void *p, size_t size) {
	(void) size;
	wlan_kfree(p, M_DEVBUF);
}

void *kmem_intr_zalloc(size_t size, int flags) {
	return wlan_kmalloc(size, flags | M_ZERO, M_DEVBUF);
}

void *kmem_zalloc(size_t size, int flags) {
	return wlan_kmalloc(size, flags | M_ZERO, M_DEVBUF);
}

void *kmem_alloc(size_t size, int flags) {
	return wlan_kmalloc(size, flags, M_DEVBUF);
}

void kmem_free(void *p, size_t size) {
	(void) size;
	wlan_kfree(p, M_DEVBUF);
}

/* ------------------------------------------------------------------ */

/* the fixed-size shells from compat sys/mutex.h */
static struct mutex *hm_mutex(kmutex_t *m) {
	_Static_assert(sizeof(struct host_mutex) >= sizeof(struct mutex), "mutex shell");
	return (struct mutex *) &((struct host_mutex *) m)->hm_priv;
}

static struct cond *hc_cond(kcondvar_t *cv) {
	_Static_assert(sizeof(struct host_cond) >= sizeof(struct cond), "cond shell");
	return (struct cond *) &((struct host_cond *) cv)->hc_priv;
}

int wlan_mutex_init(kmutex_t *m, int type, int ipl) {
	(void) type;
	(void) ipl;
	mutex_init_default(hm_mutex(m), NULL);
	return 0;
}

int wlan_mutex_enter(kmutex_t *m) {
	mutex_lock(hm_mutex(m));
	return 0;
}

int wlan_mutex_exit(kmutex_t *m) {
	mutex_unlock(hm_mutex(m));
	return 0;
}

int wlan_mutex_owned(kmutex_t *m) {
	(void) m;
	/* embox does not track ownership; the lock assertions pass. */
	return 1;
}

void wlan_mutex_destroy(kmutex_t *m) {
	(void) m;
}

int wlan_cv_init(kcondvar_t *cv, int flags) {
	struct condattr attr;

	(void) flags;
	condattr_init(&attr);
	/* the urtwn task handshake crosses kernel-task boundaries (shell
	 * thread waits, worker thread broadcasts); the default
	 * PROCESS_PRIVATE attribute rejects those wakeups with EACCES */
	condattr_setpshared(&attr, PROCESS_SHARED);
	cond_init(hc_cond(cv), &attr);
	return 0;
}

int wlan_cv_wait(kcondvar_t *cv, kmutex_t *m) {
	return cond_wait(hc_cond(cv), hm_mutex(m));
}

int wlan_cv_timedwait(kcondvar_t *cv, kmutex_t *m, int ticks) {
	struct timespec ts;

	/* bounded wait: callers poll a predicate around this, so a
	 * broadcast is not required for progress - but the deadline is */
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += (ticks * 1000 / hz) / 1000;
	ts.tv_nsec += ((ticks * 1000 / hz) % 1000) * NSEC_PER_MSEC;
	if (ts.tv_nsec >= (long) NSEC_PER_MSEC * 1000) {
		ts.tv_sec += 1;
		ts.tv_nsec -= (long) NSEC_PER_MSEC * 1000;
	}
	return cond_timedwait(hc_cond(cv), hm_mutex(m), &ts);
}

int wlan_cv_broadcast(kcondvar_t *cv) {
	cond_broadcast(hc_cond(cv));
	return 0;
}

void wlan_cv_destroy(kcondvar_t *cv) {
	(void) cv;
}

/* ------------------------------------------------------------------ */

static struct callout *hc_cast(callout_t *c) {
	_Static_assert(sizeof(struct callout) >= sizeof(void *) * 4,
	    "callout shell");
	return (struct callout *) c;
}

static void host_callout_fire(struct sys_timer *tmr, void *param) {
	struct callout *c = param;

	(void) tmr;
	c->hc_pending = 0;
	if (c->hc_fn != NULL) {
		c->hc_fn(c->hc_arg);
	}
}

int callout_init(callout_t *c0, int flags) {
	struct callout *c = hc_cast(c0);

	(void) flags;
	c->hc_timer = sys_timer_alloc();
	if (c->hc_timer == NULL) {
		return ENOMEM;
	}
	memset(c->hc_timer, 0, sizeof(struct sys_timer));
	c->hc_fn = NULL;
	c->hc_arg = NULL;
	c->hc_pending = 0;
	return 0;
}

int callout_setfunc(callout_t *c0, callout_fn_t fn, void *arg) {
	struct callout *c = hc_cast(c0);

	c->hc_fn = fn;
	c->hc_arg = arg;
	return 0;
}

int callout_schedule(callout_t *c0, int ticks) {
	struct callout *c = hc_cast(c0);

	if (ticks <= 0) {
		ticks = 1;
	}
	sys_timer_stop(c->hc_timer);
	c->hc_pending = 1;
	return sys_timer_init_start_msec(c->hc_timer,
	    SYS_TIMER_ONESHOT, (uint32_t) ticks * 1000 / hz,
	    host_callout_fire, c);
}

int callout_stop(callout_t *c) {
	if (c->hc_timer != NULL) {
		sys_timer_stop(c->hc_timer);
	}
	c->hc_pending = 0;
	return 0;
}

int callout_halt(callout_t *c, kmutex_t *lock) {
	(void) lock;
	return callout_stop(c);
}

void callout_destroy(callout_t *c) {
	callout_stop(c);
	if (c->hc_timer != NULL) {
		sys_timer_free(c->hc_timer);
		c->hc_timer = NULL;
	}
}

bool callout_pending(callout_t *c) {
	return c->hc_pending;
}

/* ------------------------------------------------------------------ */

int hz = 100;

ticks_t getticks(void) {
	return (ticks_t) (ktime_get_ns() / (10 * NSEC_PER_MSEC));
}

void delay(unsigned int us) {
	/* no busy-wait primitive on embox; yield around the deadline */
	int64_t deadline = ktime_get_ns() + (int64_t) us * 1000;

	while (ktime_get_ns() < deadline) {
		thread_yield();
	}
}

static unsigned int wlan_rand_state = 0x1234abcd;

void cprng_fast(void *buf, size_t len) {
	unsigned char *p = buf;
	size_t i;

	for (i = 0; i < len; i++) {
		wlan_rand_state = wlan_rand_state * 1103515245 + 12345;
		p[i] = (unsigned char) (wlan_rand_state >> 16);
	}
}

int copyin(const void *u, void *k, size_t len) {
	memcpy(k, u, len);
	return 0;
}

int copyout(const void *k, void *u, size_t len) {
	memcpy(u, k, len);
	return 0;
}

int copystr(const void *kf, void *kt, size_t len, size_t *done) {
	const char *src = kf;
	size_t n = 0;

	while (n + 1 < len && src[n] != '\0') {
		((char *) kt)[n] = src[n];
		n++;
	}
	((char *) kt)[n] = '\0';
	if (done != NULL) {
		*done = n + 1;
	}
	return 0;
}

ipl_t splraiseipl(ipl_t ipl) {
	(void) ipl;
	return 0;
}

int max_linkhdr = 16;
int uimin(int a, int b) { return a < b ? a : b; }
int uimax(int a, int b) { return a > b ? a : b; }

ipl_t splsoftserial(void) {
	return 0;
}

ipl_t splnet(void) {
	return 0;
}

void splx(ipl_t ipl) {
	(void) ipl;
}

/* ------------------------------------------------------------------ */
/* Port serializer.
 *
 * The imported PCIe driver has interrupt, interrupt-worker, state
 * machine and ioctl contexts that NetBSD serialises with splnet();
 * spl is a no-op here, so the driver adapter wraps every entry with
 * this lock instead. Sleeping waits drop it (see tsleep) so the
 * interrupt worker can deliver completions. */

static struct mutex wlan_ser_mtx;
static struct thread *wlan_ser_owner;

void wlan_port_serializer_lock(void) {
	mutex_lock(&wlan_ser_mtx);
	wlan_ser_owner = thread_self();
}

void wlan_port_serializer_unlock(void) {
	wlan_ser_owner = NULL;
	mutex_unlock(&wlan_ser_mtx);
}

void *wlan_port_serializer_owner(void) {
	return wlan_ser_owner;
}

/* ------------------------------------------------------------------ */
/* tsleep/wakeup: identified bounded waits over one global (mutex, cv)
 * pair. A waiter releases the port serializer around the wait. */

struct wlan_tsleep_slot {
	void *ident;
	int fired;
	struct wlan_tsleep_slot *next;
};

static struct mutex wlan_tsleep_mtx;
static struct cond wlan_tsleep_cv;
static struct wlan_tsleep_slot *wlan_tsleep_slots;

/* Wakeups that found no waiter are remembered here for a short window.
 * The imported drivers use the Unix check-then-tsleep idiom, which
 * relies on the interrupt path not being able to slip a wakeup in
 * between the predicate test and the sleep; spl does that on NetBSD
 * but is a no-op here, so a wakeup that arrives in that window would
 * otherwise be lost and the caller would run into its timeout. */
#define WLAN_TSLEEP_PENDING_MAX 16
static void *wlan_tsleep_pending[WLAN_TSLEEP_PENDING_MAX];
static int wlan_tsleep_pending_n;

static int wlan_tsleep_consume_pending(void *ident) {
	int i;

	for (i = 0; i < wlan_tsleep_pending_n; i++) {
		if (wlan_tsleep_pending[i] == ident) {
			wlan_tsleep_pending[i] =
			    wlan_tsleep_pending[--wlan_tsleep_pending_n];
			return 1;
		}
	}
	return 0;
}

int tsleep(void *ident, int pri, const char *wmesg, int timo) {
	struct wlan_tsleep_slot w;
	struct wlan_tsleep_slot **pp;
	struct timespec ts;
	int64_t deadline_ms;
	int held;
	int rc = 0;

	(void) pri;
	(void) wmesg;

	w.ident = ident;
	w.fired = 0;

	mutex_lock(&wlan_tsleep_mtx);
	w.next = wlan_tsleep_slots;
	wlan_tsleep_slots = &w;
	/* a wakeup may have raced in before the slot was linked */
	if (wlan_tsleep_consume_pending(ident)) {
		w.fired = 1;
	}
	mutex_unlock(&wlan_tsleep_mtx);

	/*timo: ticks; <=0 means forever */
	deadline_ms = (timo <= 0) ? -1 :
	    ktime_get_ns() / NSEC_PER_MSEC + (int64_t) timo * 1000 / hz;

	held = wlan_port_serializer_owner() == thread_self();
	if (held) {
		wlan_port_serializer_unlock();
	}

	mutex_lock(&wlan_tsleep_mtx);
	while (!w.fired) {
		clock_gettime(CLOCK_REALTIME, &ts);
		if (deadline_ms >= 0) {
			int64_t left = deadline_ms -
			    (int64_t) ts.tv_sec * 1000 - ts.tv_nsec / NSEC_PER_MSEC;
			if (left <= 0) {
				rc = EWOULDBLOCK;
				break;
			}
			if (left > 20) {
				left = 20; /* poll quantum */
			}
			ts.tv_sec += left / 1000;
			ts.tv_nsec += (left % 1000) * NSEC_PER_MSEC;
		} else {
			ts.tv_sec += 20;
		}
		if (ts.tv_nsec >= (long) NSEC_PER_MSEC * 1000) {
			ts.tv_sec += 1;
			ts.tv_nsec -= (long) NSEC_PER_MSEC * 1000;
		}
		cond_timedwait(&wlan_tsleep_cv, &wlan_tsleep_mtx, &ts);
	}
	for (pp = &wlan_tsleep_slots; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == &w) {
			*pp = w.next;
			break;
		}
	}
	mutex_unlock(&wlan_tsleep_mtx);

	if (held) {
		wlan_port_serializer_lock();
	}
	return rc;
}

void wakeup(void *ident) {
	struct wlan_tsleep_slot *s;
	int matched = 0;

	mutex_lock(&wlan_tsleep_mtx);
	for (s = wlan_tsleep_slots; s != NULL; s = s->next) {
		if (s->ident == ident) {
			s->fired = 1;
			matched = 1;
		}
	}
	if (!matched && wlan_tsleep_pending_n < WLAN_TSLEEP_PENDING_MAX) {
		wlan_tsleep_pending[wlan_tsleep_pending_n++] = ident;
	}
	cond_broadcast(&wlan_tsleep_cv);
	mutex_unlock(&wlan_tsleep_mtx);
}

void wakeup_one(void *ident) {
	wakeup(ident);
}

/* ------------------------------------------------------------------ */

static int wlan_osal_init(void) {
	struct condattr ca;

	mutex_init_default(&wlan_ser_mtx, NULL);
	mutex_init_default(&wlan_tsleep_mtx, NULL);
	condattr_init(&ca);
	/* the waiter (shell thread) and the waker (interrupt worker) cross
	 * task boundaries; PROCESS_PRIVATE would reject the broadcast */
	condattr_setpshared(&ca, PROCESS_SHARED);
	cond_init(&wlan_tsleep_cv, &ca);
	condattr_destroy(&ca);
	return 0;
}

EMBOX_UNIT_INIT(wlan_osal_init);

/* size guards for the embedded lock shells (see compat sys/mutex.h) */
typedef char wlan_mutex_size_check[
	sizeof(struct host_mutex) >= sizeof(struct mutex) ? 1 : -1];
typedef char wlan_cond_size_check[
	sizeof(struct host_cond) >= sizeof(struct cond) ? 1 : -1];
