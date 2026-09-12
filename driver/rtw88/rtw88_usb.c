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
 * usbdi(9) transport for the imported rtw88 chip code: implements
 * struct rtw_hci_ops on top of the RTL8821CU's bulk endpoints.
 *
 * Ported from the Linux usb.c of the same import; the shape is the same
 * (register access through vendor control requests, firmware pages written
 * 196 bytes at a time, an aggregating bulk IN pipe and up to four bulk OUT
 * pipes), with these NetBSD specifics:
 *
 *   - usbdi owns the DMA buffers, so every transfer whose length is only
 *     known at callback time (RX) is copied into an sk_buff, and TX copies
 *     the sk_buff into the xfer buffer;
 *   - the RX completion callback only moves data and resubmits: the chip
 *     code it feeds (rtw_rx_query_rx_desc(), C2H handling) takes mutexes and
 *     sleeps, so the actual demultiplexing runs on the rtw88 workqueue;
 *   - the 8821C needs one extra vendor write after every register read
 *     (rtw88_usb_reg_sec), exactly like Linux.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/device.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#include "rtw88var.h"

static void	rtw88_usb_rxeof(struct usbd_xfer *, void *, usbd_status);
static void	rtw88_usb_rx_submit(struct rtw88_rx_xfer *);

#define	RTW_USB_CMD_READ	0xc0
#define	RTW_USB_CMD_WRITE	0x40
#define	RTW_USB_CMD_REQ		0x05
#define	RTW_USB_VENQT_CMD_IDX	0x00
#define	RTW_USB_VENQT_TIMEOUT	1000	/* ms */
#define	RTW_USB_REG_SEC_TIMEOUT	500

#define	FW_START_ADDR_LEGACY	0x1000
#define	RTW_USB_FW_BLOCK	196

/* ------------------------------------------------------------------ */
/* register access                                                     */
/* ------------------------------------------------------------------ */

/* Diagnostic counter: tx ring full events. */
static unsigned int rtw88_tx_nobuf;

/*
 * Register accesses go through vendor control requests; on the 8821C the
 * always-powered sections additionally need one byte written to 0x4e0 after
 * every access, reads and writes alike (the "register security" workaround
 * of the vendor driver; missing it after writes leaves the power-on sequence
 * and the firmware download with silently dropped write steps).
 */
static void
rtw88_usb_reg_sec(struct rtw_dev *rtwdev, u32 addr, __le32 *data)
{
	struct rtw88_usb *usb = rtw88_usb_from_dev(rtwdev);
	usb_device_request_t req;
	bool reg_on_section = false;
	usbd_status err;

	if (addr <= 0xff || (addr >= 0x1000 && addr <= 0x10ff))
		reg_on_section = true;
	if (!reg_on_section)
		return;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RTW_USB_CMD_REQ;
	USETW(req.wValue, 0x4e0);
	USETW(req.wIndex, RTW_USB_VENQT_CMD_IDX);
	USETW(req.wLength, 1);

	err = usbd_do_request_flags(usb->udev, &req, data, 0, NULL,
	    RTW_USB_REG_SEC_TIMEOUT);
	if (err != USBD_NORMAL_COMPLETION)
		rtw_warn(rtwdev, "%s: reg 0x%x write failed: %s\n", __func__,
		    0x4e0, usbd_errstr(err));
}

static u32
rtw88_usb_read(struct rtw_dev *rtwdev, u32 addr, u16 len)
{
	struct rtw88_usb *usb = rtw88_usb_from_dev(rtwdev);
	usb_device_request_t req;
	u_int32_t actlen = 0;
	__le32 *data;
	unsigned int idx;
	usbd_status err;

	mutex_enter(&usb->reg_mtx);
	idx = usb->usb_data_index;
	usb->usb_data_index = (idx + 1) & (__arraycount(usb->usb_data) - 1);
	mutex_exit(&usb->reg_mtx);

	data = &usb->usb_data[idx];
	*data = 0;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RTW_USB_CMD_REQ;
	USETW(req.wValue, addr);
	USETW(req.wIndex, RTW_USB_VENQT_CMD_IDX);
	USETW(req.wLength, len);

	err = usbd_do_request_flags(usb->udev, &req, data, 0, &actlen,
	    RTW_USB_VENQT_TIMEOUT);
	if (err != USBD_NORMAL_COMPLETION) {
		rtw_dbg(rtwdev, RTW_DBG_USB,
		    "%s: read 0x%x len %u failed: %s\n", __func__, addr, len,
		    usbd_errstr(err));
		return 0;
	}

	rtw88_usb_reg_sec(rtwdev, addr, data);

	return le32_to_cpu(*data);
}

static u8
rtw88_usb_read8(struct rtw_dev *rtwdev, u32 addr)
{
	return (u8)rtw88_usb_read(rtwdev, addr, 1);
}

static u16
rtw88_usb_read16(struct rtw_dev *rtwdev, u32 addr)
{
	return (u16)rtw88_usb_read(rtwdev, addr, 2);
}

static u32
rtw88_usb_read32(struct rtw_dev *rtwdev, u32 addr)
{
	return rtw88_usb_read(rtwdev, addr, 4);
}

static void
rtw88_usb_write(struct rtw_dev *rtwdev, u32 addr, u32 val, int len)
{
	struct rtw88_usb *usb = rtw88_usb_from_dev(rtwdev);
	usb_device_request_t req;
	__le32 *data;
	unsigned int idx;
	usbd_status err;

	mutex_enter(&usb->reg_mtx);
	idx = usb->usb_data_index;
	usb->usb_data_index = (idx + 1) & (__arraycount(usb->usb_data) - 1);
	mutex_exit(&usb->reg_mtx);

	data = &usb->usb_data[idx];
	*data = cpu_to_le32(val);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RTW_USB_CMD_REQ;
	USETW(req.wValue, addr);
	USETW(req.wIndex, RTW_USB_VENQT_CMD_IDX);
	USETW(req.wLength, len);

	err = usbd_do_request_flags(usb->udev, &req, data, 0, NULL,
	    RTW_USB_VENQT_TIMEOUT);
	if (err != USBD_NORMAL_COMPLETION)
		rtw_dbg(rtwdev, RTW_DBG_USB,
		    "%s: write 0x%x len %d failed: %s\n", __func__, addr, len,
		    usbd_errstr(err));

	rtw88_usb_reg_sec(rtwdev, addr, data);
}

static void
rtw88_usb_write8(struct rtw_dev *rtwdev, u32 addr, u8 val)
{
	rtw88_usb_write(rtwdev, addr, val, 1);
}

static void
rtw88_usb_write16(struct rtw_dev *rtwdev, u32 addr, u16 val)
{
	rtw88_usb_write(rtwdev, addr, val, 2);
}

static void
rtw88_usb_write32(struct rtw_dev *rtwdev, u32 addr, u32 val)
{
	rtw88_usb_write(rtwdev, addr, val, 4);
}

/* ------------------------------------------------------------------ */
/* firmware download                                                   */
/* ------------------------------------------------------------------ */

/*
 * Firmware is written straight into the chip's memory window: each page is
 * selected in REG_MCUFW_CTRL and then written 196 bytes at a time (8 for the
 * tail, 1 for the last odd byte), all as vendor control transfers.
 */
static void
rtw88_usb_write_firmware_page(struct rtw_dev *rtwdev, u32 page,
    const u8 *data, u32 size)
{
	struct rtw88_usb *usb = rtw88_usb_from_dev(rtwdev);
	usb_device_request_t req;
	const u8 *p = data;
	u32 addr = FW_START_ADDR_LEGACY;
	u32 n;
	usbd_status err;

	rtw_write32_mask(rtwdev, REG_MCUFW_CTRL, BIT_ROM_PGE, page);

	while (size > 0) {
		if (size >= RTW_USB_FW_BLOCK)
			n = RTW_USB_FW_BLOCK;
		else if (size >= 8)
			n = 8;
		else
			n = 1;

		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
		req.bRequest = RTW_USB_CMD_REQ;
		USETW(req.wValue, addr);
		USETW(req.wIndex, RTW_USB_VENQT_CMD_IDX);
		USETW(req.wLength, n);

		err = usbd_do_request_flags(usb->udev, &req, __UNCONST(p), 0,
		    NULL, 500);
		if (err != USBD_NORMAL_COMPLETION) {
			rtw_err(rtwdev,
			    "%s: write 0x%x len %u failed: %s\n", __func__,
			    addr, n, usbd_errstr(err));
			break;
		}

		addr += n;
		p += n;
		size -= n;
	}
}

/* ------------------------------------------------------------------ */
/* TX path                                                             */
/* ------------------------------------------------------------------ */

static int
rtw88_dma_mapping_to_ep(enum rtw_dma_mapping dma_mapping)
{
	switch (dma_mapping) {
	case RTW_DMA_MAPPING_HIGH:
		return 0;
	case RTW_DMA_MAPPING_NORMAL:
		return 1;
	case RTW_DMA_MAPPING_LOW:
		return 2;
	case RTW_DMA_MAPPING_EXTRA:
		return 3;
	default:
		return -EINVAL;
	}
}

static void
rtw88_usb_txeof(struct usbd_xfer *xfer, void *priv, usbd_status status)
{
	struct rtw88_tx_xfer *tx = priv;
	struct rtw88_usb *usb = tx->usb;

	if (tx->skb != NULL) {
		rtw88_skb_free(tx->skb);
		tx->skb = NULL;
	}

	if (status == USBD_STALLED)
		usbd_clear_endpoint_stall_async(tx->pipe);

	/*
	 * This runs in the USB completion context, which must not touch the
	 * free list: rtw88_usb_tx_submit() walks it from a thread under
	 * tx_mtx, and an unsynchronised TAILQ insert there loses entries --
	 * after which a pipe reports "no free tx buffer" forever and every
	 * data frame handed to it is dropped.  Flag it and let the worker
	 * (which holds tx_mtx, and can sleep) put it back.
	 */
	tx->done = 1;
	rtw88_work_enqueue(&usb->tx_work);
}

static void
rtw88_usb_tx_submit(struct rtw88_usb *usb)
{
	int i;

	mutex_enter(&usb->tx_mtx);

	/* Return the transfers whose completion arrived since the last pass. */
	for (i = 0; i < RTW88_TX_XFER_NUM; i++) {
		struct rtw88_tx_xfer *tx = &usb->tx[i];

		if (tx->done) {
			tx->done = 0;
			TAILQ_INSERT_TAIL(&usb->tx_free[tx->ep], tx, next);
		}
	}

	for (i = 0; i < usb->n_tx_pipe; i++) {
		struct sk_buff *skb;
		struct rtw88_tx_xfer *tx;
		usbd_status err;

		while ((skb = rtw88_skb_dequeue(&usb->tx_queue[i])) != NULL) {
			tx = TAILQ_FIRST(&usb->tx_free[i]);
			if (tx == NULL) {
				/*
				 * Every buffer of this pipe is in flight: put the
				 * frame back and try again when one completes.  It
				 * used to be dropped here, which silently ate every
				 * data frame once a pipe stalled.
				 */
				if (rtw88_tx_nobuf < 3) {
					rtw_err(usb->rtwdev,
					    "%s: no free tx buffer on pipe %d\n",
					    __func__, i);
					rtw88_tx_nobuf++;
				}
				rtw88_skb_queue_head(&usb->tx_queue[i], skb);
				break;
			}
			TAILQ_REMOVE(&usb->tx_free[i], tx, next);

			if (skb->len > RTW88_TX_BUFSZ) {
				rtw_err(usb->rtwdev,
				    "%s: tx packet %u too large\n", __func__,
				    skb->len);
				rtw88_skb_free(skb);
				TAILQ_INSERT_TAIL(&usb->tx_free[i], tx, next);
				continue;
			}

			memcpy(tx->buf, skb->data, skb->len);
			tx->skb = skb;

			usbd_setup_xfer(tx->xfer, tx, tx->buf, skb->len,
			    USBD_FORCE_SHORT_XFER, RTW88_TX_TIMEOUT,
			    rtw88_usb_txeof);
			err = usbd_transfer(tx->xfer);
			if (err != USBD_NORMAL_COMPLETION &&
			    err != USBD_IN_PROGRESS) {
				rtw88_skb_free(skb);
				tx->skb = NULL;
				TAILQ_INSERT_TAIL(&usb->tx_free[i], tx, next);
				rtw_err(usb->rtwdev,
				    "%s: tx transfer failed: %s\n", __func__,
				    usbd_errstr(err));
			}
		}
	}
	mutex_exit(&usb->tx_mtx);
}

static void
rtw88_usb_tx_work(struct work_struct *w)
{
	struct rtw88_usb *usb = container_of(w, struct rtw88_usb, tx_work);

	rtw88_usb_tx_submit(usb);
}

static int
rtw88_usb_tx_write(struct rtw_dev *rtwdev, struct rtw_tx_pkt_info *pkt_info,
    struct sk_buff *skb)
{
	struct rtw88_usb *usb = rtw88_usb_from_dev(rtwdev);
	const struct rtw_chip_info *chip = rtwdev->chip;
	struct rtw_tx_desc *tx_desc;
	int ep;

	if (pkt_info->qsel >= TX_DESC_QSEL_MAX) {
		rtw_err(rtwdev, "%s: invalid qsel %u\n", __func__,
		    pkt_info->qsel);
		return -EINVAL;
	}
	ep = usb->qsel_to_ep[pkt_info->qsel];
	if (ep < 0 || ep >= usb->n_tx_pipe) {
		rtw_err(rtwdev, "%s: qsel %u has no endpoint\n", __func__,
		    pkt_info->qsel);
		return -EINVAL;
	}

	tx_desc = (struct rtw_tx_desc *)skb_push(skb, chip->tx_pkt_desc_sz);
	memset(tx_desc, 0, chip->tx_pkt_desc_sz);
	rtw_tx_fill_tx_desc(rtwdev, pkt_info, tx_desc);
	rtw_tx_fill_txdesc_checksum(rtwdev, pkt_info, tx_desc);

	rtw88_skb_queue_tail(&usb->tx_queue[ep], skb);
	return 0;
}

static void
rtw88_usb_tx_kick_off(struct rtw_dev *rtwdev)
{
	struct rtw88_usb *usb = rtw88_usb_from_dev(rtwdev);

	/*
	 * Submit right away rather than through the workqueue: the chip code
	 * polls the hardware for packets it has just queued (firmware and
	 * reserved page download) and would otherwise wait for a workqueue
	 * that it is itself occupying.
	 */
	rtw88_usb_tx_submit(usb);
}

/*
 * Reserved page (beacon/keep-alive) and H2C packets are plain TX frames with
 * a fixed queue selection and the packet info filled in by the caller.
 */
static int
rtw88_usb_write_data(struct rtw_dev *rtwdev,
    struct rtw_tx_pkt_info *pkt_info, u8 *buf)
{
	const struct rtw_chip_info *chip = rtwdev->chip;
	struct sk_buff *skb;
	unsigned int size = pkt_info->tx_pkt_size;
	int ret;

	skb = alloc_skb(chip->tx_pkt_desc_sz + size, GFP_KERNEL);
	if (skb == NULL)
		return -ENOMEM;

	/* room for the descriptor that rtw88_usb_tx_write() pushes later */
	skb_reserve(skb, chip->tx_pkt_desc_sz);
	skb_put_data(skb, buf, size);

	ret = rtw88_usb_tx_write(rtwdev, pkt_info, skb);
	if (ret == 0)
		rtw88_usb_tx_kick_off(rtwdev);
	else
		rtw88_skb_free(skb);

	return ret;
}

static int
rtw88_usb_write_data_rsvd_page(struct rtw_dev *rtwdev, u8 *buf, u32 size)
{
	const struct rtw_chip_info *chip = rtwdev->chip;
	struct rtw_tx_pkt_info pkt_info = {0};

	pkt_info.tx_pkt_size = size;
	pkt_info.qsel = TX_DESC_QSEL_BEACON;
	pkt_info.offset = chip->tx_pkt_desc_sz;
	pkt_info.ls = true;

	return rtw88_usb_write_data(rtwdev, &pkt_info, buf);
}

static int
rtw88_usb_write_data_h2c(struct rtw_dev *rtwdev, u8 *buf, u32 size)
{
	struct rtw_tx_pkt_info pkt_info = {0};

	pkt_info.tx_pkt_size = size;
	pkt_info.qsel = TX_DESC_QSEL_H2C;

	return rtw88_usb_write_data(rtwdev, &pkt_info, buf);
}

/* ------------------------------------------------------------------ */
/* RX path                                                             */
/* ------------------------------------------------------------------ */

/*
 * The bulk IN pipe carries a concatenation of packets, each with a receive
 * descriptor and a driver info area in front and padded to 8 bytes; the
 * demultiplexing is done on the workqueue (see rtw88_chip.c).
 */
static void
rtw88_usb_rx_submit(struct rtw88_rx_xfer *rx)
{
	usbd_status err;

	if (rx->xfer == NULL)
		return;
	/* bring-up instrumentation: how many rx urbs are (re)armed */
	{
		static unsigned rx_subs;

		if (rx_subs < 12) {
			printf("rtw88: rx submit #%u (%u bytes)\n", rx_subs++,
			    (unsigned) RTW88_RX_BUFSZ);
		}
	}
	usbd_setup_xfer(rx->xfer, rx, rx->buf, RTW88_RX_BUFSZ,
	    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, rtw88_usb_rxeof);
	err = usbd_transfer(rx->xfer);
	if (err != USBD_NORMAL_COMPLETION && err != USBD_IN_PROGRESS)
		printf("rtw88: rx submit failed: %s\n", usbd_errstr(err));
}

static void
rtw88_usb_rxeof(struct usbd_xfer *xfer, void *priv, usbd_status status)
{
	struct rtw88_rx_xfer *rx = priv;
	struct rtw88_usb *usb = rx->usb;
	u_int32_t len = 0;
	struct sk_buff *skb;

	if (status == USBD_CANCELLED || status == USBD_NOT_STARTED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(usb->rx_pipe);
		rtw88_usb_rx_submit(rx);
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	/* bring-up instrumentation: what the rx urbs complete with */
	{
		static unsigned rx_eofs;

		if (rx_eofs < 6) {
			unsigned i, beacon = 0;

			rx_eofs++;
			printf("rtw88: rx len=%u raw:", (unsigned) len);
			for (i = 0; i < 64 && i < len; i++)
				printf(" %02x", rx->buf[i]);
			printf("\n");
			/* a beacon/probe response starts with fc 0x0080 and a
			 * broadcast address; finding one tells us the transfer
			 * really carries 802.11 frames and where they start */
			for (i = 0; i + 8 <= len; i++) {
				static const uint8_t bc[8] = {
					0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
				};

				if ((rx->buf[i] == 0x80 || rx->buf[i] == 0x50) &&
				    rx->buf[i + 1] == 0x00 && i >= 2 &&
				    memcmp(&rx->buf[i + 4], bc, 8) == 0) {
					beacon = i;
					break;
				}
			}
			printf("rtw88: rx beacon-like frame at offset %u\n",
			    beacon);
		}
	}

	/*
	 * Linux's rtw_usb_read_port_complete() rejects transfers that are
	 * shorter than a receive descriptor or that fill the whole buffer.
	 * A short transfer means the demux would read the descriptor out of
	 * stray bytes; a full-buffer transfer means a packet was split across
	 * URBs, so the demux would walk a truncated run and lose the packets
	 * behind it.  Both produce the "skipping short packet / bad packet"
	 * storm, so drop them exactly like Linux does.
	 */
	if (len < RTW88_RX_MIN_LEN || len >= RTW88_RX_BUFSZ) {
		rtw88_usb_rx_submit(rx);
		return;
	}

	{
		skb = alloc_skb(len, GFP_ATOMIC);
		if (skb != NULL) {
			skb_put_data(skb, rx->buf, len);
			rtw88_skb_queue_tail(&usb->rx_queue, skb);
			rtw88_work_enqueue(&usb->rx_work);
		} else {
			printf("rtw88: rx buffer allocation failed\n");
		}
	}

	rtw88_usb_rx_submit(rx);
}

/* ------------------------------------------------------------------ */
/* remaining HCI ops                                                   */
/* ------------------------------------------------------------------ */

static int
rtw88_usb_setup(struct rtw_dev *rtwdev)
{
	return 0;
}

static int
rtw88_usb_start(struct rtw_dev *rtwdev)
{
	struct rtw88_usb *usb = rtw88_usb_from_dev(rtwdev);
	int i;

	for (i = 0; i < RTW88_RX_XFER_NUM; i++)
		rtw88_usb_rx_submit(&usb->rx[i]);

	/* H2C queue pointers right after RX arming, before general_info and
	 * phydm_info are pushed over bulk OUT; the chip-start dump repeats
	 * them so a delta tells whether the device took the packets. */
	rtw_info(rtwdev,
	    "h2cq pre-gi: head 0x%08x tail 0x%08x read 0x%08x info 0x%02x pktw 0x%08x pktr 0x%08x\n",
	    rtw_read32(rtwdev, REG_H2C_HEAD), rtw_read32(rtwdev, REG_H2C_TAIL),
	    rtw_read32(rtwdev, REG_H2C_READ_ADDR),
	    rtw_read8(rtwdev, REG_H2C_INFO),
	    rtw_read32(rtwdev, REG_H2C_PKT_WRITEADDR),
	    rtw_read32(rtwdev, REG_H2C_PKT_READADDR));

	return 0;
}

static void
rtw88_usb_stop(struct rtw_dev *rtwdev)
{
}

static void
rtw88_usb_deep_ps(struct rtw_dev *rtwdev, bool enter)
{
}

static void
rtw88_usb_link_ps(struct rtw_dev *rtwdev, bool enter)
{
}

/*
 * RXDMA burst size and the "drop" threshold have to follow the negotiated USB
 * speed (the dongle is a high speed device on this board).
 */
static void
rtw88_usb_interface_cfg(struct rtw_dev *rtwdev)
{
	struct rtw88_usb *usb = rtw88_usb_from_dev(rtwdev);
	u8 rxdma, burst_size;

	/* bring-up probe: domain health right before the mac-init tail */
	rtw_info(rtwdev, "interface_cfg entry: 0x290=0x%02x\n",
	    rtw_read8(rtwdev, REG_RXDMA_MODE));

	rxdma = BIT_DMA_BURST_CNT | BIT_DMA_MODE;

	if (usbd_get_speed(usb->udev) == USB_SPEED_HIGH)
		burst_size = BIT_DMA_BURST_SIZE_512;
	else
		burst_size = BIT_DMA_BURST_SIZE_64;

	u8p_replace_bits(&rxdma, burst_size, BIT_DMA_BURST_SIZE);

	rtw_write8(rtwdev, REG_RXDMA_MODE, rxdma);
	rtw_write16_set(rtwdev, REG_TXDMA_OFFSET_CHK, BIT_DROP_DATA_EN);

	rtw_info(rtwdev, "interface_cfg exit: 0x290=0x%02x\n",
	    rtw_read8(rtwdev, REG_RXDMA_MODE));
}

static void
rtw88_usb_dynamic_rx_agg(struct rtw_dev *rtwdev, bool enable)
{
	u8 size, timeout;
	u16 val16;

	/*
	 * Bring-up: keep RX aggregation off.  With agg on (size 3 / timeout
	 * 32) the device hands us cut fragments whenever the timeout flushes
	 * mid-frame, and our demux drops those - enough to lose AUTH/ASSOC
	 * responses.  One whole packet per transfer instead.
	 */
	enable = false;

	switch (rtwdev->chip->id) {
	case RTW_CHIP_TYPE_8822C:
	case RTW_CHIP_TYPE_8822B:
	case RTW_CHIP_TYPE_8821C:
	case RTW_CHIP_TYPE_8814A:
		rtw_write8_set(rtwdev, REG_TXDMA_PQ_MAP, BIT_RXDMA_AGG_EN);
		rtw_write8_clr(rtwdev, REG_RXDMA_AGG_PG_TH + 3, BIT(7));

		if (enable) {
			size = 0x5;
			timeout = 0x20;
		} else {
			size = 0x0;
			timeout = 0x1;
		}
		val16 = u16_encode_bits(size, BIT_RXDMA_AGG_PG_TH) |
		    u16_encode_bits(timeout, BIT_DMA_AGG_TO_V1);
		rtw_write16(rtwdev, REG_RXDMA_AGG_PG_TH, val16);
		break;
	default:
		break;
	}
}

static const struct rtw_hci_ops rtw88_usb_ops = {
	.tx_write = rtw88_usb_tx_write,
	.tx_kick_off = rtw88_usb_tx_kick_off,
	.setup = rtw88_usb_setup,
	.start = rtw88_usb_start,
	.stop = rtw88_usb_stop,
	.deep_ps = rtw88_usb_deep_ps,
	.link_ps = rtw88_usb_link_ps,
	.interface_cfg = rtw88_usb_interface_cfg,
	.dynamic_rx_agg = rtw88_usb_dynamic_rx_agg,
	.write_firmware_page = rtw88_usb_write_firmware_page,

	.write8 = rtw88_usb_write8,
	.write16 = rtw88_usb_write16,
	.write32 = rtw88_usb_write32,
	.read8 = rtw88_usb_read8,
	.read16 = rtw88_usb_read16,
	.read32 = rtw88_usb_read32,

	.write_data_rsvd_page = rtw88_usb_write_data_rsvd_page,
	.write_data_h2c = rtw88_usb_write_data_h2c,
};

const struct rtw_hci_ops *
rtw88_usb_get_ops(void)
{

	return &rtw88_usb_ops;
}


/* ------------------------------------------------------------------ */
/* attach / detach                                                     */
/* ------------------------------------------------------------------ */

static int
rtw88_usb_parse(struct rtw88_chip *chip, struct usbd_interface *iface)
{
	struct rtw_dev *rtwdev = &chip->rtwdev;
	struct rtw88_usb *usb = &chip->usb;
	const struct rtw_chip_info *chipinfo = rtwdev->chip;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	const struct rtw_rqpn *rqpn;
	uint8_t rx_ep = 0, int_ep = 0;
	int i, nout = 0, ret;

	id = usbd_get_interface_descriptor(iface);
	if (id == NULL) {
		rtw_err(rtwdev, "%s: no interface descriptor\n", __func__);
		return EINVAL;
	}

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL)
			continue;

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			if (rx_ep != 0) {
				rtw_err(rtwdev, "%s: too many bulk IN\n",
				    __func__);
				return EINVAL;
			}
			rx_ep = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			int_ep = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			if (nout >= RTW88_TX_EP_MAX) {
				rtw_err(rtwdev, "%s: too many bulk OUT\n",
				    __func__);
				return EINVAL;
			}
			ret = usbd_open_pipe(iface, ed->bEndpointAddress,
			    USBD_EXCLUSIVE_USE, &usb->tx_pipe[nout]);
			if (ret != 0) {
				rtw_err(rtwdev,
				    "%s: cannot open tx pipe 0x%02x: %s\n",
				    __func__, ed->bEndpointAddress,
				    usbd_errstr(ret));
				return ret;
			}
			nout++;
		}
	}

	if (rx_ep == 0 || nout < 1) {
		rtw_err(rtwdev, "%s: unexpected endpoint layout (%d out)\n",
		    __func__, nout);
		return EINVAL;
	}

	ret = usbd_open_pipe(iface, rx_ep, USBD_EXCLUSIVE_USE, &usb->rx_pipe);
	if (ret != 0) {
		rtw_err(rtwdev, "%s: cannot open rx pipe 0x%02x: %s\n",
		    __func__, rx_ep, usbd_errstr(ret));
		return ret;
	}
	if (int_ep != 0) {
		ret = usbd_open_pipe(iface, int_ep, USBD_EXCLUSIVE_USE,
		    &usb->int_pipe);
		if (ret != 0)
			usb->int_pipe = NULL;	/* optional for now */
	}

	usb->n_tx_pipe = nout;
	rtwdev->hci.bulkout_num = nout;

	rqpn = &chipinfo->rqpn_table[nout];
	usb->qsel_to_ep[TX_DESC_QSEL_TID0] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_be);
	usb->qsel_to_ep[TX_DESC_QSEL_TID1] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_bk);
	usb->qsel_to_ep[TX_DESC_QSEL_TID2] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_bk);
	usb->qsel_to_ep[TX_DESC_QSEL_TID3] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_be);
	usb->qsel_to_ep[TX_DESC_QSEL_TID4] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_vi);
	usb->qsel_to_ep[TX_DESC_QSEL_TID5] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_vi);
	usb->qsel_to_ep[TX_DESC_QSEL_TID6] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_vo);
	usb->qsel_to_ep[TX_DESC_QSEL_TID7] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_vo);
	for (i = TX_DESC_QSEL_TID8; i <= TX_DESC_QSEL_TID15; i++)
		usb->qsel_to_ep[i] = -EINVAL;
	usb->qsel_to_ep[TX_DESC_QSEL_BEACON] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_hi);
	usb->qsel_to_ep[TX_DESC_QSEL_HIGH] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_hi);
	usb->qsel_to_ep[TX_DESC_QSEL_MGMT] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_mg);
	usb->qsel_to_ep[TX_DESC_QSEL_H2C] =
	    rtw88_dma_mapping_to_ep(rqpn->dma_map_hi);

	rtw_info(rtwdev, "%d tx pipe(s), rx 0x%02x, interrupt 0x%02x\n",
	    nout, rx_ep, int_ep);
	return 0;
}

int
rtw88_usb_attach(struct rtw88_chip *chip, struct usbd_interface *iface)
{
	struct rtw88_usb *usb = &chip->usb;
	struct rtw_dev *rtwdev = &chip->rtwdev;
	int i, ret;

	usb->rtwdev = rtwdev;
	netbsd_mutex_init(&usb->reg_mtx);
	netbsd_mutex_init(&usb->tx_mtx);
	rtw88_skb_queue_init(&usb->rx_queue);
	for (i = 0; i < RTW88_TX_EP_MAX; i++) {
		rtw88_skb_queue_init(&usb->tx_queue[i]);
		TAILQ_INIT(&usb->tx_free[i]);
	}
	INIT_WORK(&usb->rx_work, rtw88_chip_rx_work);
	INIT_WORK(&usb->tx_work, rtw88_usb_tx_work);

	ret = rtw88_usb_parse(chip, iface);
	if (ret != 0)
		return ret;

	/*
	 * TX buffers are bound to one bulk OUT pipe each, spread as evenly as
	 * possible over the pipes the device exposes.
	 */
	for (i = 0; i < RTW88_TX_XFER_NUM; i++) {
		int ep = i % usb->n_tx_pipe;

		ret = usbd_create_xfer(usb->tx_pipe[ep], RTW88_TX_BUFSZ,
		    USBD_FORCE_SHORT_XFER, 0, &usb->tx[i].xfer);
		if (ret != 0) {
			rtw_err(rtwdev, "%s: tx xfer %d failed: %s\n", __func__,
			    i, usbd_errstr(ret));
			return ret;
		}
		usb->tx[i].buf = usbd_get_buffer(usb->tx[i].xfer);
		usb->tx[i].usb = usb;
		usb->tx[i].pipe = usb->tx_pipe[ep];
		usb->tx[i].ep = ep;
		TAILQ_INSERT_TAIL(&usb->tx_free[ep], &usb->tx[i], next);
	}

	for (i = 0; i < RTW88_RX_XFER_NUM; i++) {
		ret = usbd_create_xfer(usb->rx_pipe, RTW88_RX_BUFSZ, 0, 0,
		    &usb->rx[i].xfer);
		if (ret != 0) {
			rtw_err(rtwdev, "%s: rx xfer %d failed: %s\n", __func__,
			    i, usbd_errstr(ret));
			return ret;
		}
		usb->rx[i].buf = usbd_get_buffer(usb->rx[i].xfer);
		usb->rx[i].usb = usb;
	}

	return 0;
}

void
rtw88_usb_detach(struct rtw88_chip *chip)
{
	struct rtw88_usb *usb = &chip->usb;
	int i;

	if (usb->rx_pipe != NULL)
		usbd_abort_pipe(usb->rx_pipe);
	for (i = 0; i < usb->n_tx_pipe; i++)
		usbd_abort_pipe(usb->tx_pipe[i]);

	for (i = 0; i < RTW88_RX_XFER_NUM; i++) {
		if (usb->rx[i].xfer != NULL)
			usbd_destroy_xfer(usb->rx[i].xfer);
	}
	for (i = 0; i < RTW88_TX_XFER_NUM; i++) {
		if (usb->tx[i].skb != NULL)
			rtw88_skb_free(usb->tx[i].skb);
		if (usb->tx[i].xfer != NULL)
			usbd_destroy_xfer(usb->tx[i].xfer);
	}
	for (i = 0; i < usb->n_tx_pipe; i++)
		rtw88_skb_queue_purge(&usb->tx_queue[i]);
	rtw88_skb_queue_purge(&usb->rx_queue);

	if (usb->int_pipe != NULL)
		usbd_close_pipe(usb->int_pipe);
	if (usb->rx_pipe != NULL)
		usbd_close_pipe(usb->rx_pipe);
	for (i = 0; i < usb->n_tx_pipe; i++)
		usbd_close_pipe(usb->tx_pipe[i]);
	netbsd_mutex_destroy(&usb->reg_mtx);
	netbsd_mutex_destroy(&usb->tx_mtx);
}
