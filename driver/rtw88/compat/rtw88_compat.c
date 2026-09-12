/*	$NetBSD$	*/

/*-
 * Copyright (c) 2026 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the RK3568 lab (NetBSD/RTL8821CU bring-up).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Runtime side of the rtw88 compatibility layer: sk_buff, workqueue,
 * completion and firmware(9) plumbing, plus the mac80211 entry points the
 * chip code calls.
 *
 * Everything the chip code runs here is invoked from thread context (the
 * rtw88 workqueue or the driver's own task), never from an interrupt
 * handler: the register accessors in the transport sleep.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/workqueue.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <dev/firmload.h>

#include "rtw88_compat.h"
#include <linux/firmware.h>

#include "rtw88_mac80211.h"
#include "rtw88_glue.h"

/* Worker threads come from the port (same pair the usbdi shim uses), not
 * from a kernel kthread API: the port creates them suspended and starts
 * them on demand. */
extern void *wlan_port_thread_create(void *(*run)(void *), void *arg);
extern void wlan_port_thread_start(void *thread);

/* ------------------------------------------------------------------ */
/* sk_buff                                                             */
/* ------------------------------------------------------------------ */

void *
rtw88_skb_alloc(unsigned int size, gfp_t gfp)
{
	struct sk_buff *skb;

	/* one allocation for the header and the data area */
	skb = kmem_zalloc(sizeof(*skb) + size, (int)gfp);
	skb->head = (unsigned char *)(skb + 1);
	skb->data = skb->head;
	skb->tail = 0;
	skb->end = size;
	skb->len = 0;
	skb->truesize = sizeof(*skb) + size;
	return skb;
}

void
rtw88_skb_free(struct sk_buff *skb)
{

	if (skb == NULL)
		return;
	kmem_free(skb, sizeof(*skb) + skb->end);
}

void
rtw88_skb_queue_init(struct sk_buff_head *q)
{

	q->next = (struct sk_buff *)q;
	q->prev = (struct sk_buff *)q;
	q->qlen = 0;
}

void
rtw88_skb_queue_tail(struct sk_buff_head *q, struct sk_buff *skb)
{
	struct sk_buff *head = (struct sk_buff *)q;
	struct sk_buff *tail = head->prev;

	skb->next = head;
	skb->prev = tail;
	skb->list = q;
	tail->next = skb;
	head->prev = skb;
	q->qlen++;
}

void
rtw88_skb_queue_head(struct sk_buff_head *q, struct sk_buff *skb)
{
	struct sk_buff *head = (struct sk_buff *)q;
	struct sk_buff *first = head->next;

	skb->next = first;
	skb->prev = head;
	skb->list = q;
	first->prev = skb;
	head->next = skb;
	q->qlen++;
}

struct sk_buff *
rtw88_skb_dequeue(struct sk_buff_head *q)
{
	struct sk_buff *head = (struct sk_buff *)q;
	struct sk_buff *skb = head->next;

	if (skb == head)
		return NULL;
	rtw88_skb_unlink(skb, q);
	return skb;
}

void
rtw88_skb_unlink(struct sk_buff *skb, struct sk_buff_head *q)
{

	skb->prev->next = skb->next;
	skb->next->prev = skb->prev;
	skb->next = NULL;
	skb->prev = NULL;
	skb->list = NULL;
	if (q != NULL && q->qlen > 0)
		q->qlen--;
}

void
rtw88_skb_queue_purge(struct sk_buff_head *q)
{
	struct sk_buff *skb;

	while ((skb = rtw88_skb_dequeue(q)) != NULL)
		rtw88_skb_free(skb);
}

/* ------------------------------------------------------------------ */
/* workqueue                                                           */
/* ------------------------------------------------------------------ */

/*
 * Deferred work runs on a private thread rather than workqueue(9): work items
 * are queued from USB completion callbacks, which run in softint context and
 * must not take a sleeping mutex.  The pending list is guarded by a spin
 * mutex and the worker is poked with wakeup(), both of which are safe from
 * any context.
 *
 * The list is a STAILQ rather than NetBSD's SIMPLEQ: the port resolves
 * <sys/queue.h> to the host libc's copy, which carries the BSD queues but
 * not the SIMPLEQ family.
 */
struct rtw88_work_item {
	STAILQ_ENTRY(rtw88_work_item) wi_entry;
	struct work_struct *wi_work;
};

static STAILQ_HEAD(, rtw88_work_item) rtw88_pending =
    STAILQ_HEAD_INITIALIZER(rtw88_pending);
static kmutex_t rtw88_pending_mtx;
static bool rtw88_worker_started;
static bool rtw88_ready;

static void
rtw88_workqueue_worker(void *arg)
{
	struct rtw88_work_item *item;
	struct work_struct *w;

	for (;;) {
		mutex_enter(&rtw88_pending_mtx);
		item = STAILQ_FIRST(&rtw88_pending);
		if (item != NULL)
			STAILQ_REMOVE_HEAD(&rtw88_pending, wi_entry);
		mutex_exit(&rtw88_pending_mtx);

		if (item == NULL) {
			/* wakeup() from the producer cuts this short */
			tsleep(&rtw88_pending, 0, "rtw88wq", 1);
			continue;
		}

		w = item->wi_work;
		kmem_free(item, sizeof(*item));

		/* the item may requeue itself from inside wk_func() */
		w->wk_queued = 0;
		atomic_store_relaxed(&w->wk_running, 1);
		w->wk_func(w);
		atomic_store_relaxed(&w->wk_running, 0);
	}
}

/* the port's thread entry returns a value, the worker never returns */
static void *
rtw88_workqueue_trampoline(void *arg)
{

	rtw88_workqueue_worker(arg);
	return NULL;
}

void
rtw88_workqueue_ready(void)
{
	void *thread;

	if (rtw88_ready)
		return;
	rtw88_ready = true;
	netbsd_spin_mutex_init(&rtw88_pending_mtx);

	thread = wlan_port_thread_create(rtw88_workqueue_trampoline, NULL);
	if (thread == NULL) {
		printf("rtw88: cannot start the work thread\n");
		return;
	}
	wlan_port_thread_start(thread);
	rtw88_worker_started = true;
}

struct workqueue *
rtw88_workqueue_alloc(const char *name)
{

	rtw88_workqueue_ready();
	/* the queue is a singleton; the handle is only a token */
	return rtw88_worker_started ? (struct workqueue *)&rtw88_pending : NULL;
}

/*
 * The chip code calls destroy_workqueue() from failure paths that may run on
 * the rtw88 worker itself (rtw_core_init() bailing out, for example), so the
 * thread is kept for the lifetime of the kernel.
 */
void
rtw88_workqueue_free(struct workqueue *wq)
{
	(void)wq;
}

/*
 * Wait for a work item that is queued or currently running.  There is no way
 * to cancel an item that has not run yet, so this is a flush, which is what
 * the callers (rtw_core_stop and friends) actually need before tearing state
 * down.
 */
void
rtw88_work_flush(struct work_struct *w)
{

	while (w->wk_queued != 0 || w->wk_running)
		tsleep(w, 0, "rtw88fl", 1);
}

/*
 * Queue an item for the worker: safe from softint and callout context.
 */
void
rtw88_work_enqueue_safe(struct work_struct *w)
{
	struct rtw88_work_item *item;

	if (!rtw88_worker_started)
		return;
	if (atomic_cas_uint(&w->wk_queued, 0, 1) != 0)
		return;

	item = kmem_alloc(sizeof(*item), KM_NOSLEEP);
	if (item == NULL) {
		w->wk_queued = 0;
		return;
	}
	item->wi_work = w;
	mutex_enter(&rtw88_pending_mtx);
	STAILQ_INSERT_TAIL(&rtw88_pending, item, wi_entry);
	mutex_exit(&rtw88_pending_mtx);
	wakeup(&rtw88_pending);
}

void
rtw88_delayed_work_callout(void *arg)
{
	struct delayed_work *dw = arg;

	dw->dw_scheduled = false;
	if (dw->dw_cancel)
		return;
	rtw88_work_enqueue(&dw->work);
}

void
rtw88_timer_callout(void *arg)
{
	struct timer_list *tl = arg;

	tl->tl_pending = false;
	tl->tl_func(tl);
}

/* ------------------------------------------------------------------ */
/* deferred calls                                                      */
/* ------------------------------------------------------------------ */

/*
 * net80211 calls into the driver at splnet, but the chip code below sleeps,
 * so those paths are deferred here and run on the rtw88 workqueue.
 */
/*
 * The work item cannot be freed from inside its own callback (the workqueue
 * still walks it afterwards), so deferred calls are taken from a small pool
 * whose entries are recycled.
 */
#define	RTW88_ASYNC_CALLS	16

struct rtw88_async_call {
	struct work_struct	work;
	void			(*fn)(void *);
	void			*arg;
	volatile unsigned int	in_use;
	bool			initialised;
};

static struct rtw88_async_call rtw88_async_calls[RTW88_ASYNC_CALLS];
static unsigned int rtw88_async_call_next;

static void
rtw88_async_call_cb(struct work_struct *work)
{
	struct rtw88_async_call *call =
	    container_of(work, struct rtw88_async_call, work);

	call->fn(call->arg);
	call->in_use = 0;
}

int
rtw88_call_async(void (*fn)(void *), void *arg)
{
	struct rtw88_async_call *call;
	unsigned int i, slot;

	for (i = 0; i < RTW88_ASYNC_CALLS; i++) {
		slot = (rtw88_async_call_next + i) % RTW88_ASYNC_CALLS;
		call = &rtw88_async_calls[slot];
		if (atomic_cas_uint(&call->in_use, 0, 1) != 0)
			continue;
		call->fn = fn;
		call->arg = arg;
		if (!call->initialised) {
			call->initialised = true;
			INIT_WORK(&call->work, rtw88_async_call_cb);
		}
		rtw88_async_call_next = slot + 1;
		rtw88_work_enqueue(&call->work);
		return 0;
	}

	return EAGAIN;
}

/* ------------------------------------------------------------------ */
/* firmware(9) bridge                                                  */
/* ------------------------------------------------------------------ */

/*
 * The chip code passes Linux paths like "rtw88/rtw8821c_fw.bin"; firmware(9)
 * looks them up as <prefix>/<driver>/<image>, so only the base name is used.
 */
int
rtw88_request_firmware_nowait(const char *name, struct device *dev,
    struct firmware **fw_out, void (*cont)(const struct firmware *, void *),
    void *context)
{
	firmware_handle_t fh;
	struct firmware *fw;
	const char *base;
	u8 *buf;
	off_t size;
	int error;

	base = strrchr(name, '/');
	base = base != NULL ? base + 1 : name;

	error = firmware_open("if_rtw88", base, &fh);
	if (error != 0) {
		printf("%s: failed to open firmware %s (error %d)\n",
		    dev != NULL ? device_xname(dev) : "rtw88", base, error);
		if (cont != NULL)
			cont(NULL, context);
		return error;
	}

	size = firmware_get_size(fh);
	buf = firmware_malloc(size);
	if (buf == NULL) {
		firmware_close(fh);
		if (cont != NULL)
			cont(NULL, context);
		return ENOMEM;
	}
	error = firmware_read(fh, 0, buf, size);
	firmware_close(fh);
	if (error != 0) {
		firmware_free(buf, size);
		if (cont != NULL)
			cont(NULL, context);
		return error;
	}

	fw = kmem_zalloc(sizeof(*fw), KM_SLEEP);
	fw->size = size;
	fw->data = buf;
	fw->priv = buf;		/* the allocation to free */
	if (fw_out != NULL)
		*fw_out = fw;
	if (cont != NULL)
		cont(fw, context);
	return 0;
}

void
rtw88_release_firmware(const struct firmware *fw)
{

	if (fw == NULL)
		return;
	if (fw->priv != NULL)
		firmware_free(fw->priv, fw->size);
	kmem_free(__UNCONST(fw), sizeof(*fw));
}

/* ------------------------------------------------------------------ */
/* mac80211: the little of it the chip code really reaches             */
/* ------------------------------------------------------------------ */

/*
 * A single-instance mac80211 stand-in: one hw (with an embedded wiphy), one
 * STA vif and one peer sta.  if_rtw88.c owns these through the helpers below
 * and keeps them in sync with net80211 state; the chip code only reads them
 * (rates, AID, association) and iterates over them in its watchdog.
 */
struct rtw88_mac80211_ctx {
	struct ieee80211_hw	hw;
	struct wiphy		wiphy;
	struct ieee80211_vif	vif;
	struct ieee80211_sta	sta;
	bool			vif_valid;
	bool			sta_valid;
};

struct ieee80211_hw *
rtw88_mac80211_alloc(void *priv)
{
	struct rtw88_mac80211_ctx *ctx;
	struct ieee80211_hw *hw;

	ctx = kmem_zalloc(sizeof(*ctx), KM_SLEEP);
	hw = &ctx->hw;
	hw->priv = priv;
	hw->wiphy = &ctx->wiphy;
	ctx->wiphy.rts_threshold = 2347;
	ctx->wiphy.max_scan_ssids = 1;
	ctx->wiphy.max_scan_ie_len = 512;
	hw->conf.chandef.chan = NULL;
	return hw;
}

struct ieee80211_hw *
wiphy_to_ieee80211_hw(struct wiphy *wiphy)
{
	struct rtw88_mac80211_ctx *ctx =
	    container_of(wiphy, struct rtw88_mac80211_ctx, wiphy);

	return &ctx->hw;
}

void
rtw88_mac80211_free(struct ieee80211_hw *hw)
{
	struct rtw88_mac80211_ctx *ctx;

	if (hw == NULL)
		return;
	ctx = container_of(hw, struct rtw88_mac80211_ctx, hw);
	kmem_free(ctx, sizeof(*ctx));
}

static struct rtw88_mac80211_ctx *
rtw88_mac80211_ctx(struct ieee80211_hw *hw)
{
	return container_of(hw, struct rtw88_mac80211_ctx, hw);
}

struct ieee80211_vif *
rtw88_mac80211_vif(struct ieee80211_hw *hw)
{
	struct rtw88_mac80211_ctx *ctx = rtw88_mac80211_ctx(hw);

	ctx->vif_valid = true;
	ctx->vif.type = NL80211_IFTYPE_STATION;
	return &ctx->vif;
}

struct ieee80211_sta *
rtw88_mac80211_sta(struct ieee80211_hw *hw)
{
	struct rtw88_mac80211_ctx *ctx = rtw88_mac80211_ctx(hw);

	ctx->sta_valid = true;
	return &ctx->sta;
}

void
rtw88_mac80211_set_sta(struct ieee80211_hw *hw, const u8 *addr, bool valid)
{
	struct rtw88_mac80211_ctx *ctx = rtw88_mac80211_ctx(hw);

	ctx->sta_valid = valid;
	ether_addr_copy(ctx->sta.addr, addr);
}

void
rtw88_mac80211_set_assoc(struct ieee80211_hw *hw, const u8 *bssid, bool assoc)
{
	struct rtw88_mac80211_ctx *ctx = rtw88_mac80211_ctx(hw);

	ctx->vif_valid = true;
	ctx->vif.cfg.assoc = assoc;
	ctx->vif.bss_conf.assoc = assoc;
	ether_addr_copy(ctx->vif.addr, bssid);
	ether_addr_copy(ctx->vif.bss_conf.bssid, bssid);
	ctx->vif.bss_conf.basic_rates = 0x01;	/* 1 Mb/s, legacy only */
}

int
ieee80211_register_hw(struct ieee80211_hw *hw)
{

	/* the net80211 registration is done by if_rtw88.c */
	return 0;
}

void
ieee80211_unregister_hw(struct ieee80211_hw *hw)
{
}

void
ieee80211_restart_hw(struct ieee80211_hw *hw)
{
}

void
ieee80211_queue_work(struct ieee80211_hw *hw, struct work_struct *work)
{

	rtw88_work_enqueue(work);
}

void
ieee80211_queue_delayed_work(struct ieee80211_hw *hw,
    struct delayed_work *dwork, unsigned long delay)
{

	schedule_delayed_work(dwork, delay);
}

void
ieee80211_wake_queues(struct ieee80211_hw *hw)
{
}

void
ieee80211_stop_queues(struct ieee80211_hw *hw)
{
}

void
ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb)
{

	rtw88_skb_free(skb);
}

void
ieee80211_free_txskb(struct ieee80211_hw *hw, struct sk_buff *skb)
{

	rtw88_skb_free(skb);
}

void
ieee80211_tx_info_clear_status(struct ieee80211_tx_info *info)
{

	memset(&info->status, 0, sizeof(info->status));
}

struct sk_buff *
ieee80211_tx_dequeue(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{

	return NULL;
}

int
ieee80211_txq_get_depth(struct ieee80211_txq *txq, unsigned long *frame_cnt,
    unsigned long *byte_cnt)
{

	if (frame_cnt != NULL)
		*frame_cnt = 0;
	if (byte_cnt != NULL)
		*byte_cnt = 0;
	return 0;
}

struct ieee80211_sta *
ieee80211_find_sta(struct ieee80211_vif *vif, const u8 *addr)
{
	struct rtw88_mac80211_ctx *ctx =
	    container_of(vif, struct rtw88_mac80211_ctx, vif);

	if (!ctx->sta_valid)
		return NULL;
	return &ctx->sta;
}

struct ieee80211_sta *
ieee80211_find_sta_by_ifaddr(struct ieee80211_hw *hw, const u8 *addr,
    const u8 *localaddr)
{
	struct rtw88_mac80211_ctx *ctx = rtw88_mac80211_ctx(hw);

	if (!ctx->sta_valid)
		return NULL;
	return &ctx->sta;
}

void
ieee80211_iterate_active_interfaces_atomic(struct ieee80211_hw *hw,
    u32 iter_flags, void (*iterator)(void *data, u8 *mac,
    struct ieee80211_vif *vif), void *data)
{
	struct rtw88_mac80211_ctx *ctx = rtw88_mac80211_ctx(hw);

	if (!ctx->vif_valid)
		return;
	iterator(data, ctx->vif.addr, &ctx->vif);
}

void
ieee80211_iterate_active_interfaces(struct ieee80211_hw *hw, u32 iter_flags,
    void (*iterator)(void *data, u8 *mac, struct ieee80211_vif *vif),
    void *data)
{

	ieee80211_iterate_active_interfaces_atomic(hw, iter_flags, iterator,
	    data);
}

void
ieee80211_iterate_stations_atomic(struct ieee80211_hw *hw,
    void (*iterator)(void *data, struct ieee80211_sta *sta), void *data)
{
	struct rtw88_mac80211_ctx *ctx = rtw88_mac80211_ctx(hw);

	if (!ctx->sta_valid)
		return;
	/*
	 * The shadow mac80211 never issues a station-add callback, so the chip
	 * side's per-station state (rtw_sta_info in drv_priv[]) is unbound
	 * until here.  Without it the iterator dereferences si->sta == NULL.
	 */
	rtw88_sta_init(&ctx->sta, &ctx->vif, hw->priv);
	iterator(data, &ctx->sta);
}

void
ieee80211_iterate_stations(struct ieee80211_hw *hw,
    void (*iterator)(void *data, struct ieee80211_sta *sta), void *data)
{

	ieee80211_iterate_stations_atomic(hw, iterator, data);
}

void
ieee80211_iter_keys(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    void (*iter)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta, struct ieee80211_key_conf *key, void *data),
    void *data)
{
}

void
ieee80211_iter_keys_rcu(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    void (*iter)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta, struct ieee80211_key_conf *key, void *data),
    void *data)
{
}

void
ieee80211_request_smps(struct ieee80211_vif *vif, int link_id,
    enum ieee80211_smps_mode smps_mode)
{
}

int
ieee80211_start_tx_ba_session(struct ieee80211_sta *sta, u16 tid,
    u16 timeout)
{

	return -EOPNOTSUPP;
}

/*
 * Randomised MAC address with the given mask (mac80211's
 * get_random_mask_addr): only the bits the caller wants to randomise move.
 */
void
get_random_mask_addr(u8 *buf, const u8 *addr, const u8 *mask)
{
	u8 rnd[ETH_ALEN];
	unsigned int r;
	int i;

	r = (unsigned int)random();
	memcpy(rnd, &r, sizeof(r));
	r = (unsigned int)random();
	memcpy(rnd + sizeof(r), &r, ETH_ALEN - sizeof(r));

	for (i = 0; i < ETH_ALEN; i++)
		buf[i] = (rnd[i] & ~mask[i]) | (addr[i] & mask[i]);
	buf[0] &= ~0x01;	/* never a group address */
}

/*
 * cfg80211 regulatory hint: the chip code reads its own efuse regulatory
 * data and only uses wiphy->reg_notifier for notifications, so the hint
 * itself has nothing to do here.
 */
int
regulatory_hint(struct wiphy *wiphy, const char *alpha2)
{

	return 0;
}

void
ieee80211_scan_completed(struct ieee80211_hw *hw,
    struct cfg80211_scan_info *info)
{
}

void
ieee80211_connection_loss(struct ieee80211_vif *vif)
{
}

void
ieee80211_cqm_rssi_notify(struct ieee80211_vif *vif,
    enum nl80211_cqm_rssi_threshold_event rssi_event, s32 rssi_level,
    gfp_t gfp)
{
}

struct sk_buff *
ieee80211_nullfunc_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    int link_id, bool qos)
{

	return NULL;
}

struct sk_buff *
ieee80211_probereq_get(struct ieee80211_hw *hw, const u8 *src_addr,
    const u8 *ssid, size_t ssid_len, size_t tailroom)
{

	return NULL;
}

struct sk_buff *
ieee80211_proberesp_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{

	return NULL;
}

struct sk_buff *
ieee80211_pspoll_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{

	return NULL;
}

struct sk_buff *
ieee80211_beacon_get_tim(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    u16 *tim_offset, u16 *tim_length, int link_id)
{

	return NULL;
}
