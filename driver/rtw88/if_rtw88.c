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
 * Realtek RTL8811CU/RTL8821CU USB 802.11ac driver (rtw88 family).
 *
 * The rtw88 chips are unrelated to urtwn(4) (RTL8188/8192 family): they need
 * the rtw88 register map, PHY tables, firmware download and H2C/C2H
 * protocols.  That chip logic is imported from the dual-licensed
 * (GPL-2.0 OR BSD-3-Clause) Linux sources under sys/external/bsd/rtw88; this
 * file is the net80211 side of the glue and deliberately contains no Linux
 * types: it talks to the chip through rtw88_chip.c.
 *
 * Layout of a 0bda:c820 (after umodeswitch(4) has moved it off the fake CD):
 *
 *	interface 0  class e0/01/01  interrupt IN 0x81, bulk OUT 0x02, bulk IN 0x82
 *	interface 1  class e0/01/01  two isochronous endpoints (Bluetooth SCO)
 *	interface 2  class ff/ff/ff  bulk IN 0x84, bulk OUT 0x05/0x06/0x08,
 *	                             interrupt IN 0x87
 *
 * Only interface 2 is the WiFi function (the Linux driver binds it as
 * :1.2), and the interrupt endpoint is not used: C2H reports arrive in the
 * aggregated bulk IN stream.
 *
 * net80211 here is the pre-FreeBSD-8 stack: no HT/VHT, no per-driver
 * channel or raw-xmit callbacks, and channel changes are driven through the
 * state machine (ic_newstate) plus a scan callout, as urtwn(4) does.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/callout.h>
#include <sys/mutex.h>
#include <sys/pmf.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_proto.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include "rtw88_glue.h"
#include "rtw88_chipvar.h"

struct rtw88_softc {
	device_t		sc_dev;
	struct usbd_device	*sc_udev;
	struct usbd_interface	*sc_iface;
	struct rtw88_chip	*sc_chip;
	struct rtw88_hw_info	sc_info;
	bool			sc_chip_started;

	struct ieee80211com	sc_ic;
	struct ethercom		sc_ec;
#define	sc_if			sc_ec.ec_if
	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	callout_t		sc_scan_to;
	kmutex_t		sc_media_mtx;

	int			sc_dying;
	bool			sc_attached;
	enum ieee80211_state	sc_cmd_state;
	int			sc_cmd_arg;
};

static const struct usb_devno rtw88_devs[] = {
	{ USB_VENDOR_REALTEK, USB_PRODUCT_REALTEK_RTL8821CU },
};

static int	rtw88_match(device_t, cfdata_t, void *);
static void	rtw88_attach(device_t, device_t, void *);
static int	rtw88_detach(device_t, int);
static int	rtw88_activate(device_t, enum devact);

CFATTACH_DECL_NEW(rtw88u, sizeof(struct rtw88_softc), rtw88_match,
    rtw88_attach, rtw88_detach, rtw88_activate);

static void	rtw88_bringup_task(void *);
static void	rtw88_newstate_cb(void *);
static void	rtw88_next_scan(void *);
static void	rtw88_rx_frame(void *, const uint8_t *, size_t, int);
static int	rtw88_init(struct ifnet *);
static void	rtw88_stop(struct ifnet *, int);
static void	rtw88_start(struct ifnet *);
static void	rtw88_watchdog(struct ifnet *);
static int	rtw88_ioctl(struct ifnet *, u_long, void *);
static int	rtw88_reset(struct ifnet *);
static int	rtw88_newstate(struct ieee80211com *, enum ieee80211_state, int);

static int
rtw88_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (usb_lookup(rtw88_devs, uaa->uaa_vendor, uaa->uaa_product) != NULL)
		return UMATCH_VENDOR_PRODUCT;

	return UMATCH_NONE;
}

/*
 * The WiFi function is the vendor-specific interface carrying one bulk IN
 * and at least one bulk OUT endpoint; everything else belongs to the
 * Bluetooth half of the combo.
 */
static struct usbd_interface *
rtw88_find_wifi_iface(struct rtw88_softc *sc)
{
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct usbd_interface *iface;
	int i, j, error;
	int nrx, ntx;

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL)
		return NULL;

	for (i = 0; i < cdesc->bNumInterface; i++) {
		error = usbd_device2interface_handle(sc->sc_udev, i, &iface);
		if (error != 0)
			continue;
		id = usbd_get_interface_descriptor(iface);
		if (id == NULL || id->bInterfaceClass != 0xff)
			continue;

		nrx = ntx = 0;
		for (j = 0; j < id->bNumEndpoints; j++) {
			ed = usbd_interface2endpoint_descriptor(iface, j);
			if (ed == NULL)
				continue;
			if (UE_GET_XFERTYPE(ed->bmAttributes) != UE_BULK)
				continue;
			if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
				nrx++;
			else
				ntx++;
		}
#ifdef RTW88_DEBUG
		aprint_debug_dev(sc->sc_dev,
		    "interface %d: %d bulk in, %d bulk out\n",
		    id->bInterfaceNumber, nrx, ntx);
#endif
		if (nrx < 1 || ntx < 1)
			continue;

		aprint_normal_dev(sc->sc_dev,
		    "using interface %d (class %#x/%#x/%#x, %d endpoints)\n",
		    id->bInterfaceNumber, id->bInterfaceClass,
		    id->bInterfaceSubClass, id->bInterfaceProtocol,
		    id->bNumEndpoints);
		return iface;
	}

	return NULL;
}

static void
rtw88_attach(device_t parent, device_t self, void *aux)
{
	struct rtw88_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	char *devinfop;
	int error;

	sc->sc_dev = self;
	sc->sc_udev = uaa->uaa_device;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	error = usbd_set_config_no(sc->sc_udev, 1, 0);
	if (error != 0) {
		aprint_error_dev(self, "failed to set configuration, err=%s\n",
		    usbd_errstr(error));
		return;
	}

	sc->sc_iface = rtw88_find_wifi_iface(sc);
	if (sc->sc_iface == NULL) {
		aprint_error_dev(self, "no WiFi interface found\n");
		return;
	}

	/*
	 * The chip needs its firmware, and firmware(9) cannot read files
	 * until the root file system is mounted -- which happens after USB
	 * enumeration on this board.  Bring the chip up from a task that
	 * retries until the firmware is reachable, then register with
	 * net80211; the interface simply does not exist until then.
	 */
	rtw88_workqueue_ready();
	if (rtw88_call_async(rtw88_bringup_task, sc) != 0) {
		aprint_error_dev(self, "cannot schedule chip bring-up\n");
		return;
	}

	pmf_device_register(self, NULL, NULL);
	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);
}

static void
rtw88_bringup_task(void *arg)
{
	struct rtw88_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int attempt, i;

	for (attempt = 0; attempt < 40; attempt++) {
		if (sc->sc_dying)
			return;
		sc->sc_chip = rtw88_chip_attach(sc->sc_udev, sc->sc_iface,
		    sc->sc_dev, &sc->sc_info);
		if (sc->sc_chip != NULL)
			break;
		/* the firmware is not reachable yet: delay and retry */
		tsleep(sc, 0, "rtw88up", MAX(1, (int)mstohz(100)));
	}
	if (sc->sc_chip == NULL) {
		aprint_error_dev(sc->sc_dev, "failed to bring up the chip\n");
		return;
	}
	rtw88_chip_set_callbacks(sc->sc_chip, sc, rtw88_rx_frame, NULL);

	/*
	 * Set up the 802.11 device.
	 */
	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;

	ic->ic_caps =
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_WPA;		/* 802.11i (software crypto) */

	/* 11b/g rates; the chip runs them in legacy mode (no HT stack here) */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = rtw88_init;
	ifp->if_ioctl = rtw88_ioctl;
	ifp->if_start = rtw88_start;
	ifp->if_watchdog = rtw88_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	if_initialize(ifp);
	if (sc->sc_info.efuse_valid)
		if_set_sadl(ifp, sc->sc_info.mac_addr, IEEE80211_ADDR_LEN,
		    false);
	ieee80211_ifattach(ic);

	/* override default methods */
	ic->ic_reset = rtw88_reset;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = rtw88_newstate;
	callout_init(&sc->sc_scan_to, 0);
	callout_setfunc(&sc->sc_scan_to, rtw88_next_scan, sc);

	/*
	 * The media lock is only there because the net80211 media layer
	 * wants one; the driver serialises chip access through the rtw88
	 * workqueue.
	 */
	mutex_init(&sc->sc_media_mtx, MUTEX_DEFAULT, IPL_SOFTUSB);
	ieee80211_media_init_with_lock(ic, ieee80211_media_change,
	    ieee80211_media_status, &sc->sc_media_mtx);

	ifp->if_percpuq = if_percpuq_create(ifp);
	if_register(ifp);

	if (sc->sc_info.efuse_valid) {
		if_set_sadl(ifp, sc->sc_info.mac_addr, IEEE80211_ADDR_LEN,
		    false);
		memcpy(ic->ic_myaddr, sc->sc_info.mac_addr,
		    IEEE80211_ADDR_LEN);
	}

	if (sc->sc_info.efuse_valid)
		aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
		    ether_sprintf(sc->sc_info.mac_addr));
	ieee80211_announce(ic);
	aprint_normal_dev(sc->sc_dev, "firmware %s\n",
	    sc->sc_info.fw_version);
	sc->sc_attached = true;
}

static int
rtw88_detach(device_t self, int flags)
{
	struct rtw88_softc *sc = device_private(self);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	int s;

	pmf_device_deregister(self);
	s = splusb();
	sc->sc_dying = 1;
	callout_halt(&sc->sc_scan_to, NULL);

	if (sc->sc_chip != NULL) {
		if (sc->sc_chip_started)
			rtw88_stop(ifp, 1);
		rtw88_chip_detach(sc->sc_chip);
		sc->sc_chip = NULL;
	}

	if (sc->sc_attached) {
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		ieee80211_ifdetach(ic);
		if_detach(ifp);
		mutex_destroy(&sc->sc_media_mtx);
		sc->sc_attached = false;
	}
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	return 0;
}

static int
rtw88_activate(device_t self, enum devact act)
{
	struct rtw88_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if_deactivate(sc->sc_ic.ic_ifp);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/* ------------------------------------------------------------------ */
/* RX: called from the rtw88 workqueue (thread context)                */
/* ------------------------------------------------------------------ */

static void
rtw88_rx_frame(void *ctx, const uint8_t *data, size_t len, int rssi)
{
	struct rtw88_softc *sc = ctx;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int s;

	if (sc->sc_dying || len == 0)
		return;
	if (len > IEEE80211_MAX_LEN) {
		/* not a frame net80211 could ever accept: device garbage */
		if_statinc(ifp, if_ierrors);
		return;
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		if_statinc(ifp, if_ierrors);
		return;
	}
	MCLAIM(m, &sc->sc_ec.ec_rx_mowner);
	if (len > MHLEN) {
		if (len > MCLBYTES) {
			MEXTMALLOC(m, len, M_DONTWAIT);
		} else {
			MCLGET(m, M_DONTWAIT);
		}
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			if_statinc(ifp, if_ierrors);
			return;
		}
	}

	m_set_rcvif(m, ifp);
	memcpy(mtod(m, void *), data, len);
	m->m_pkthdr.len = m->m_len = (int)len;

	s = splnet();
	ni = ieee80211_find_rxnode(ic,
	    (const struct ieee80211_frame_min *)mtod(m, const void *));
	ieee80211_input(ic, m, ni, rssi, 0);
	ieee80211_free_node(ni);
	splx(s);
}

/* ------------------------------------------------------------------ */
/* TX                                                                  */
/* ------------------------------------------------------------------ */

static void
rtw88_start(struct ifnet *ifp)
{
	struct rtw88_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;
	bool is_mgmt;
	int error;

	if (sc->sc_chip == NULL || !rtw88_chip_ready(sc->sc_chip)) {
		if_statinc(ifp, if_oerrors);
		return;
	}
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		if_statinc(ifp, if_oerrors);
		return;
	}

	for (;;) {
		is_mgmt = false;
		ni = NULL;

		IF_POLL(&ic->ic_mgtq, m);
		if (m != NULL) {
			IF_DEQUEUE(&ic->ic_mgtq, m);
			ni = M_GETCTX(m, struct ieee80211_node *);
			is_mgmt = true;
		} else if (ic->ic_state == IEEE80211_S_RUN) {
			struct ether_header *eh;
			struct ieee80211_frame *wh;

			IFQ_POLL(&ifp->if_snd, m);
			if (m == NULL)
				break;
			IFQ_DEQUEUE(&ifp->if_snd, m);

			eh = mtod(m, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				if_statinc(ifp, if_oerrors);
				m_freem(m);
				continue;
			}

			if ((m = ieee80211_encap(ic, m, ni)) == NULL) {
				if_statinc(ifp, if_oerrors);
				ieee80211_free_node(ni);
				continue;
			}

			/*
			 * ieee80211_encap() only builds the 802.11 header, sets
			 * the Protected bit and reserves room for the crypto
			 * header; a software-crypto driver has to finish the
			 * encapsulation itself (see ieee80211_output.c).  Without
			 * this the frame leaves with Protected set but no CCMP
			 * header/MIC, so the AP discards every data frame -- the
			 * "association succeeds, ping never answers" symptom.
			 */
			wh = mtod(m, struct ieee80211_frame *);
			if ((wh->i_fc[1] & IEEE80211_FC1_WEP) != 0 &&
			    ieee80211_crypto_encap(ic, ni, m) == NULL) {
				if_statinc(ifp, if_oerrors);
				m_freem(m);
				ieee80211_free_node(ni);
				continue;
			}
		} else {
			break;
		}

		/* rtw88_chip_tx() always consumes the mbuf */
		error = rtw88_chip_tx(sc->sc_chip, m, is_mgmt);
		if (error != 0)
			if_statinc(ifp, if_oerrors);

		if (ni != NULL)
			ieee80211_free_node(ni);
	}
}

static void
rtw88_watchdog(struct ifnet *ifp)
{
	struct rtw88_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;
	ieee80211_watchdog(&sc->sc_ic);
}

/* ------------------------------------------------------------------ */
/* State machine and scanning                                          */
/* ------------------------------------------------------------------ */

/*
 * net80211 runs the state machine at splnet; the chip below sleeps, so hand
 * the transition to the rtw88 workqueue and drive net80211's own machine
 * from there.
 */
static int
rtw88_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct rtw88_softc *sc = ic->ic_ifp->if_softc;

	callout_stop(&sc->sc_scan_to);
	sc->sc_cmd_state = nstate;
	sc->sc_cmd_arg = arg;
	if (rtw88_call_async(rtw88_newstate_cb, sc) != 0) {
		/* out of memory: fall back to the generic machine */
		return sc->sc_newstate(ic, nstate, arg);
	}

	return 0;
}

static void
rtw88_newstate_cb(void *arg)
{
	struct rtw88_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate = ic->ic_state;
	enum ieee80211_state nstate = sc->sc_cmd_state;
	int s;

	if (sc->sc_dying) {
		sc->sc_newstate(ic, nstate, sc->sc_cmd_arg);
		return;
	}

	s = splnet();
	callout_stop(&sc->sc_scan_to);

	switch (nstate) {
	case IEEE80211_S_SCAN:
		/*
		 * One channel per pass: net80211 walks the channel list by
		 * re-entering this state, so program the radio for the
		 * channel it picked and let the scan callout move on.
		 */
		rtw88_chip_set_channel(sc->sc_chip,
		    ieee80211_chan2ieee(ic, ic->ic_curchan));
		callout_schedule(&sc->sc_scan_to, hz / 5);
		break;

	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
	case IEEE80211_S_RUN:
		if (ostate != nstate) {
			rtw88_chip_set_channel(sc->sc_chip,
			    ieee80211_chan2ieee(ic, ic->ic_curchan));
			/*
			 * The BSSID filter and station net-type must be
			 * programmed before the AUTH exchange, like Linux's
			 * bss_info_changed does before mgd_prepare_tx.
			 */
			rtw88_chip_set_bssid(sc->sc_chip,
			    ic->ic_bss->ni_bssid);
		}
		if (nstate == IEEE80211_S_RUN)
			rtw88_chip_set_assoc(sc->sc_chip, ic->ic_bss->ni_bssid,
			    true);
		break;

	case IEEE80211_S_INIT:
		rtw88_chip_set_assoc(sc->sc_chip, ic->ic_bss->ni_bssid, false);
		break;
	}
	splx(s);

	sc->sc_newstate(ic, nstate, sc->sc_cmd_arg);
}

static void
rtw88_next_scan(void *arg)
{
	struct rtw88_softc *sc = arg;
	int s;

	s = splnet();
	if (sc->sc_ic.ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(&sc->sc_ic);
	splx(s);
}

static int
rtw88_reset(struct ifnet *ifp)
{
	struct rtw88_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (sc->sc_chip != NULL) {
		rtw88_chip_set_assoc(sc->sc_chip, ic->ic_bss->ni_bssid, false);
		rtw88_chip_set_channel(sc->sc_chip,
		    ieee80211_chan2ieee(ic, ic->ic_curchan));
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* ifnet entry points                                                  */
/* ------------------------------------------------------------------ */

static int
rtw88_init(struct ifnet *ifp)
{
	struct rtw88_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int error, s;

	if (sc->sc_dying)
		return ENXIO;

	s = splnet();
	rtw88_stop(ifp, 0);

	if (sc->sc_chip == NULL ||
	    (error = rtw88_chip_start(sc->sc_chip)) != 0) {
		aprint_error_dev(sc->sc_dev, "failed to start the chip (%d)\n",
		    error);
		splx(s);
		return error != 0 ? error : ENXIO;
	}
	sc->sc_chip_started = true;

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	splx(s);
	return 0;
}

static void
rtw88_stop(struct ifnet *ifp, int disable)
{
	struct rtw88_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	callout_stop(&sc->sc_scan_to);
	splx(s);

	if (sc->sc_chip != NULL && sc->sc_chip_started) {
		sc->sc_chip_started = false;
		rtw88_chip_stop(sc->sc_chip);
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

static int
rtw88_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct rtw88_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0, s;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (sc->sc_dying) {
			error = EIO;
			break;
		}
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_RUNNING:
			rtw88_stop(ifp, 1);
			break;
		case IFF_UP:
			rtw88_init(ifp);
			break;
		default:
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		/*
		 * net80211 asks for a refresh of hardware settings (channel,
		 * filters).  A full chip restart (MAC power cycle + firmware
		 * download) is disruptive and not what this asks for: bring
		 * the chip up only if it is not running yet, otherwise just
		 * reprogram the radio and let the state machine push the rest.
		 */
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING) &&
		    ic->ic_roaming != IEEE80211_ROAMING_MANUAL) {
			if (!sc->sc_chip_started)
				rtw88_init(ifp);
			else if (sc->sc_chip != NULL)
				rtw88_chip_set_channel(sc->sc_chip,
				    ieee80211_chan2ieee(ic, ic->ic_curchan));
		}
		error = 0;
	}

	splx(s);
	return error;
}
