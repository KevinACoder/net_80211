/*
 * @file
 * @brief CherryUSB OS services (usb_osal.h) over embox kernel primitives.
 *
 * The USB host controller completes transfers from its interrupt handler,
 * so sem_give and mq_send must be legal in IRQ context.  On embox both go
 * through the scheduler wait queues, which are IPL-protected and safe to
 * touch from an interrupt handler (same pattern as the in-tree usb_dwc
 * and GMAC drivers); the actual context switch happens when the interrupt
 * critical section unwinds.
 *
 * embox `struct sem` counts consumed tokens (enter succeeds while
 * value < max_value and increments; leave decrements and wakes), so the
 * cherryusb "available count" maps to (max_value - value) and a fresh
 * semaphore needs its value seeded to (max - initial).
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <hal/ipl.h>
#include <kernel/thread.h>
#include <kernel/thread/sync/semaphore.h>
#include <kernel/thread/sync/mutex.h>
#include <kernel/sched/schedee_priority.h>
#include <kernel/time/sys_timer.h>
#include <kernel/time/ktime.h>
#include <kernel/time/time.h>

#include <usb_osal.h>
#include <usb_errno.h>

/* cherryusb priorities: 0 = highest.  Map onto the upper embox range so
 * hub/worker threads outrank the default- priorities shell threads. */
#define USB_EMBOX_PRIO_BASE 200
#define USB_EMBOX_PRIO_STEP 4

/* ------------------------------------------------------------------ */
/* threads */

struct usb_embox_thread_ctx {
	usb_thread_entry_t entry;
	void *arg;
};

static void *usb_embox_thread_trampoline(void *p) {
	struct usb_embox_thread_ctx ctx;

	ctx = *(struct usb_embox_thread_ctx *) p;
	free(p);
	ctx.entry(ctx.arg);
	return NULL;
}

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size,
	uint32_t prio, usb_thread_entry_t entry, void *args) {
	struct usb_embox_thread_ctx *ctx;
	struct thread *t;
	int embox_prio;

	(void) name;
	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}
	ctx->entry = entry;
	ctx->arg = args;

	t = thread_create_with_stack(THREAD_FLAG_DETACHED | THREAD_FLAG_SUSPENDED,
	    stack_size, usb_embox_thread_trampoline, ctx);
	if (t == NULL) {
		free(ctx);
		return NULL;
	}

	embox_prio = USB_EMBOX_PRIO_BASE - (int) prio * USB_EMBOX_PRIO_STEP;
	if (embox_prio < 16) {
		embox_prio = 16;
	}
	schedee_priority_set(&t->schedee, embox_prio);

	thread_launch(t);
	return (usb_osal_thread_t) t;
}

void usb_osal_thread_delete(usb_osal_thread_t thread) {
	if (thread == NULL || thread == (usb_osal_thread_t) thread_self()) {
		thread_exit(NULL);
	} else {
		thread_terminate((struct thread *) thread);
	}
}

void usb_osal_thread_schedule_other(void) {
	thread_yield();
}

/* ------------------------------------------------------------------ */
/* semaphores (give is IRQ-safe) */

struct usb_embox_sem {
	struct sem s;
};

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count) {
	struct usb_embox_sem *s;

	s = malloc(sizeof(*s));
	if (s == NULL) {
		return NULL;
	}
	/* binary: capacity 1 */
	semaphore_init(&s->s, 1);
	s->s.value = 1 - (int) initial_count;
	return (usb_osal_sem_t) s;
}

usb_osal_sem_t usb_osal_sem_create_counting(uint32_t max_count) {
	struct usb_embox_sem *s;

	s = malloc(sizeof(*s));
	if (s == NULL) {
		return NULL;
	}
	semaphore_init(&s->s, (int) max_count);
	s->s.value = (int) max_count; /* counting sems start empty */
	return (usb_osal_sem_t) s;
}

void usb_osal_sem_delete(usb_osal_sem_t sem) {
	free(sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout) {
	struct usb_embox_sem *s = sem;
	struct timespec now;
	int64_t ns;
	struct timespec deadline;
	int ret;

	if (timeout == USB_OSAL_WAITING_FOREVER) {
		semaphore_enter(&s->s);
		return 0;
	}
	if (timeout == 0) {
		return semaphore_tryenter(&s->s) == 0 ? 0 : -USB_ERR_TIMEOUT;
	}

	clock_gettime(CLOCK_REALTIME, &now);
	ns = timespec_to_ns(&now) + (int64_t) timeout * NSEC_PER_MSEC;
	deadline = ns_to_timespec(ns);
	ret = semaphore_timedwait(&s->s, &deadline);
	return ret == 0 ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_sem_give(usb_osal_sem_t sem) {
	struct usb_embox_sem *s = sem;

	semaphore_leave(&s->s);
	return 0;
}

void usb_osal_sem_reset(usb_osal_sem_t sem) {
	struct usb_embox_sem *s = sem;

	s->s.value = s->s.max_value; /* empty */
}

/* ------------------------------------------------------------------ */
/* mutexes */

usb_osal_mutex_t usb_osal_mutex_create(void) {
	struct mutex *m;

	m = malloc(sizeof(*m));
	if (m == NULL) {
		return NULL;
	}
	mutex_init_default(m, NULL);
	return (usb_osal_mutex_t) m;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex) {
	free(mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex) {
	return mutex_lock((struct mutex *) mutex);
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex) {
	mutex_unlock((struct mutex *) mutex);
	return 0;
}

/* ------------------------------------------------------------------ */
/* message queue (send is IRQ-safe: ring guarded by IPL, items counted
 * by a counting semaphore).  One consumer is assumed - the hub thread. */

struct usb_embox_mq {
	uintptr_t *buf;
	uint32_t cap;
	uint32_t head;
	uint32_t count;
	struct usb_embox_sem items;
};

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs) {
	struct usb_embox_mq *mq;

	if (max_msgs == 0) {
		return NULL;
	}
	mq = malloc(sizeof(*mq));
	if (mq == NULL) {
		return NULL;
	}
	mq->buf = malloc(max_msgs * sizeof(uintptr_t));
	if (mq->buf == NULL) {
		free(mq);
		return NULL;
	}
	mq->cap = max_msgs;
	mq->head = 0;
	mq->count = 0;
	semaphore_init(&mq->items.s, (int) max_msgs);
	mq->items.s.value = (int) max_msgs; /* starts empty */
	return (usb_osal_mq_t) mq;
}

void usb_osal_mq_delete(usb_osal_mq_t mq) {
	struct usb_embox_mq *m = mq;

	free(m->buf);
	free(m);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr) {
	struct usb_embox_mq *m = mq;
	ipl_t ipl;

	ipl = ipl_save();
	if (m->count == m->cap) {
		ipl_restore(ipl);
		return -USB_ERR_NOMEM;
	}
	m->buf[(m->head + m->count) % m->cap] = addr;
	m->count++;
	ipl_restore(ipl);

	semaphore_leave(&m->items.s);
	return 0;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout) {
	struct usb_embox_mq *m = mq;
	ipl_t ipl;
	int ret;

	ret = usb_osal_sem_take((usb_osal_sem_t) &m->items, timeout);
	if (ret != 0) {
		return ret;
	}

	ipl = ipl_save();
	*addr = m->buf[m->head];
	m->head = (m->head + 1) % m->cap;
	m->count--;
	ipl_restore(ipl);
	return 0;
}

/* ------------------------------------------------------------------ */
/* timers */

static void usb_embox_timer_fire(struct sys_timer *tmr, void *param) {
	struct usb_osal_timer *t = param;

	(void) tmr;
	t->handler(t->argument);
}

struct usb_osal_timer *usb_osal_timer_create(const char *name,
	uint32_t timeout_ms, usb_timer_handler_t handler, void *argument,
	bool is_period) {
	struct usb_osal_timer *t;

	(void) name;
	t = malloc(sizeof(*t));
	if (t == NULL) {
		return NULL;
	}
	t->handler = handler;
	t->argument = argument;
	t->is_period = is_period;
	t->timeout_ms = timeout_ms;
	t->timer = sys_timer_alloc();
	if (t->timer == NULL) {
		free(t);
		return NULL;
	}
	return t;
}

void usb_osal_timer_delete(struct usb_osal_timer *timer) {
	if (timer == NULL) {
		return;
	}
	sys_timer_stop(timer->timer);
	sys_timer_free(timer->timer);
	free(timer);
}

void usb_osal_timer_start(struct usb_osal_timer *timer) {
	sys_timer_init_start_msec(timer->timer,
	    timer->is_period ? SYS_TIMER_PERIODIC : SYS_TIMER_ONESHOT,
	    timer->timeout_ms, usb_embox_timer_fire, timer);
}

void usb_osal_timer_stop(struct usb_osal_timer *timer) {
	sys_timer_stop(timer->timer);
}

/* ------------------------------------------------------------------ */
/* misc */

size_t usb_osal_enter_critical_section(void) {
	return (size_t) ipl_save();
}

void usb_osal_leave_critical_section(size_t flag) {
	ipl_restore((ipl_t) flag);
}

void usb_osal_msleep(uint32_t delay) {
	ksleep(delay);
}

void *usb_osal_malloc(size_t size) {
	return malloc(size);
}

void usb_osal_free(void *ptr) {
	free(ptr);
}
