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
 * API of the compat layer (rtw88_compat.c) used by the chip and USB layers.
 */

#ifndef _RTW88_GLUE_H_
#define _RTW88_GLUE_H_

#include <sys/types.h>

struct ieee80211_hw;
struct ieee80211_vif;
struct ieee80211_sta;
struct rtw_dev;

/* must run before any work item can be queued */
void	rtw88_workqueue_ready(void);

/*
 * Run fn(arg) on the rtw88 workqueue.  net80211 calls into the driver at
 * splnet (state changes, ioctls) while the chip code below it sleeps, so
 * those paths are handed to this thread instead.
 */
int	rtw88_call_async(void (*)(void *), void *);

/* mac80211 stand-in: one hw/wiphy, one STA vif, one peer sta */
struct ieee80211_hw *rtw88_mac80211_alloc(void *);
void	rtw88_mac80211_free(struct ieee80211_hw *);
struct ieee80211_vif *rtw88_mac80211_vif(struct ieee80211_hw *);
struct ieee80211_sta *rtw88_mac80211_sta(struct ieee80211_hw *);
void	rtw88_mac80211_set_sta(struct ieee80211_hw *, const uint8_t *, bool);
void	rtw88_mac80211_set_assoc(struct ieee80211_hw *, const uint8_t *, bool);
/*
 * Bind the chip's per-station view (struct rtw_sta_info, which lives in the
 * sta/vif drv_priv[] areas) before any station iterator runs.  Linux does
 * this in rtw_sta_add(); the shadow mac80211 here has no such callback, so
 * the station iterators call it themselves.
 */
void	rtw88_sta_init(struct ieee80211_sta *, struct ieee80211_vif *,
	    struct rtw_dev *);

#endif /* _RTW88_GLUE_H_ */
