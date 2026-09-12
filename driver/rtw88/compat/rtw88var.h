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
 * Private state shared by the USB transport (rtw88_usb.c) and the chip glue
 * (rtw88_chip.c).  This header lives in the "dist world": only files that
 * may include the imported headers should include it.
 */

#ifndef _RTW88VAR_H_
#define _RTW88VAR_H_

#include "rtw88_compat.h"
#include "rtw88_mac80211.h"
#include "main.h"
#include "debug.h"
#include "reg.h"
#include "rx.h"
#include "tx.h"
#include "fw.h"
#include "rtw8821c.h"

#include "rtw88_glue.h"
#include "rtw88_chipvar.h"

/* from the Linux transport header, which is not part of the import */
#define	TX_DESC_QSEL_MAX	20

#define	RTW88_TX_EP_MAX		4
#define	RTW88_TX_XFER_NUM	8
#define	RTW88_TX_BUFSZ		(16 * 1024)
#define	RTW88_RX_XFER_NUM	2
#define	RTW88_RX_BUFSZ		32768
/* shortest bulk-IN transfer Linux hands to the demux (one rx descriptor) */
#define	RTW88_RX_MIN_LEN	24
#define	RTW88_TX_TIMEOUT	5000	/* ms */

struct rtw88_rx_xfer {
	struct usbd_xfer	*xfer;
	struct rtw88_usb	*usb;
	uint8_t			*buf;
};

struct rtw88_tx_xfer {
	struct usbd_xfer	*xfer;
	struct usbd_pipe	*pipe;
	uint8_t			*buf;
	struct sk_buff		*skb;
	struct rtw88_usb	*usb;
	int			ep;
	volatile int		done;	/* completion seen, waiting for tx_mtx */
	TAILQ_ENTRY(rtw88_tx_xfer) next;
};

TAILQ_HEAD(rtw88_txfree_head, rtw88_tx_xfer);

/*
 * The USB transport, plus the state the HCI ops need.  A single instance per
 * device; rtwdev->priv points back here.
 */
struct rtw88_usb {
	struct rtw_dev		*rtwdev;
	struct usbd_device	*udev;
	struct usbd_interface	*iface;

	kmutex_t		reg_mtx;
	kmutex_t		tx_mtx;	/* serialises TX submission */
	uint32_t		usb_data[128];
	unsigned int		usb_data_index;

	struct usbd_pipe	*rx_pipe;
	struct usbd_pipe	*int_pipe;
	struct usbd_pipe	*tx_pipe[RTW88_TX_EP_MAX];
	int			n_tx_pipe;
	uint8_t			qsel_to_ep[TX_DESC_QSEL_MAX];

	struct rtw88_rx_xfer	rx[RTW88_RX_XFER_NUM];
	struct sk_buff_head	rx_queue;
	struct work_struct	rx_work;

	struct rtw88_tx_xfer	tx[RTW88_TX_XFER_NUM];
	struct rtw88_txfree_head tx_free[RTW88_TX_EP_MAX];
	struct sk_buff_head	tx_queue[RTW88_TX_EP_MAX];
	struct work_struct	tx_work;
	bool			tx_stopped;
};

struct rtw88_chip {
	struct rtw_dev		rtwdev;		/* first member */
	struct rtw88_usb	usb;
	struct device		hostdev;	/* fake struct device */
	struct ieee80211_hw	*hw;
	struct rtw88_hw_info	info;

	void			*rx_ctx;
	rtw88_rx_cb_t		rx_cb;
	rtw88_scan_cb_t		scan_cb;

	bool			started;
	bool			assoc;
};

static __inline struct rtw88_chip *
rtw88_chip_from_dev(struct rtw_dev *rtwdev)
{

	return (struct rtw88_chip *)rtwdev;
}

static __inline struct rtw88_usb *
rtw88_usb_from_dev(struct rtw_dev *rtwdev)
{

	return &rtw88_chip_from_dev(rtwdev)->usb;
}

/* rtw88_usb.c */
int	rtw88_usb_attach(struct rtw88_chip *, struct usbd_interface *);
void	rtw88_usb_detach(struct rtw88_chip *);
const struct rtw_hci_ops *rtw88_usb_get_ops(void);

/* rtw88_chip.c: runs on the rtw88 workqueue, feeding the net80211 driver */
void	rtw88_chip_rx_work(struct work_struct *);

#endif /* _RTW88VAR_H_ */
