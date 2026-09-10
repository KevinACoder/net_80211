/*
 * @file
 * @brief urtwn chip driver registration for the port interface.
 *
 * urtwn_attach() keeps its NetBSD autoconf shape; this adapter plays
 * the role of the USB bus attachment: softc allocation, the device
 * shell, and the usb_attach_arg built from the port device.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/queue.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <sys/sockio.h>
#include <net/if_media.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <sys/rndsource.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <driver/urtwn/rtwnreg.h>
#include <driver/urtwn/if_urtwnreg.h>
#include <port/port.h>
#include <port/bus/usb/port_usb.h>

#include <driver/urtwn/if_urtwnvar.h>
#include <port/osal/embox/wlan_port_embox.h>

/* the verbatim import compiled into this unit so its static glue
 * (CFATTACH_DECL_NEW tables) stays intact */
#include "if_urtwn.c"

static const struct wlan_usb_id urtwn_usb_ids[] = {
	{ 0x0bda, 0x8179 }, /* RTL8188EU */
	{ 0x0bda, 0x0179 }, /* RTL8188EUS */
	{ 0, 0 }
};


struct urtwn_softc *urtwn_reg_softc;

int wlan_urtwn_attach(struct wlan_usb_dev *usb, void *if_priv);

static int wlan_urtwn_attach_bus(void *bus_dev, void *if_priv) {
	return wlan_urtwn_attach((struct wlan_usb_dev *) bus_dev, if_priv);
}

int wlan_urtwn_up(void);
void wlan_urtwn_dump(void);
void wlan_urtwn_scan_dump(void);
int wlan_port_scan_urtwn(const uint8_t *ssid, size_t len);
int wlan_port_xmit_urtwn(const uint8_t *frame, size_t len);
int wlan_port_get_hwaddr_urtwn(uint8_t addr[6]);

static struct wlan_port_adapter urtwn_adapter = {
	.name = "urtwn",
	.up = wlan_urtwn_up,
	.scan = wlan_port_scan_urtwn,
	.xmit = wlan_port_xmit_urtwn,
	.get_hwaddr = wlan_port_get_hwaddr_urtwn,
	.status_dump = wlan_urtwn_dump,
	.scan_dump = wlan_urtwn_scan_dump,
};

int wlan_urtwn_attach(struct wlan_usb_dev *usb, void *if_priv) {
	struct urtwn_softc *sc;
	struct device *self;
	struct usb_attach_arg uaa;

	(void) if_priv;
	sc = wlan_kmalloc(sizeof(struct urtwn_softc), M_WAITOK | M_ZERO,
	    M_USBDEV);
	if (sc == NULL) {
		return -ENOMEM;
	}
	self = wlan_kmalloc(sizeof(struct device), M_WAITOK | M_ZERO,
	    M_USBDEV);
	if (self == NULL) {
		wlan_kfree(sc, M_USBDEV);
		return -ENOMEM;
	}
	snprintf(self->dv_xname, sizeof(self->dv_xname), "urtwn%u",
	    (unsigned) (wlan_port_if_n - 1));
	self->dv_private = sc;

	memset(&uaa, 0, sizeof(uaa));
	uaa.uaa_vendor = usb->vendor;
	uaa.uaa_product = usb->product;
	uaa.uaa_device = usb->port_priv;

	urtwn_attach(self, self, &uaa);

	if (!sc->sc_dying && ISSET(sc->sc_flags, URTWN_FLAG_ATTACHED)) {
		/* remember the first healthy unit for the scan trigger */
		if (urtwn_reg_softc == NULL) {
			urtwn_reg_softc = sc;
			urtwn_adapter.ic = &sc->sc_ic;
			wlan_port_adapter_register(&urtwn_adapter);
		}
	}
	return 0;
}

struct urtwn_softc *urtwn_reg_softc;

static void wlan_print_node_cb(void *arg, struct ieee80211_node *ni) {
	(void) arg;
	struct ieee80211_channel *ch = ni->ni_chan;

	printf("  %02x:%02x:%02x:%02x:%02x:%02x  ch=%d  rssi=%u  ssid=%.*s\n",
	    ni->ni_bssid[0], ni->ni_bssid[1], ni->ni_bssid[2],
	    ni->ni_bssid[3], ni->ni_bssid[4], ni->ni_bssid[5],
	    ch != NULL ? ch->ic_freq : 0, ni->ni_rssi,
	    ni->ni_esslen, ni->ni_essid);
}

int wlan_urtwn_up(void) {
	struct urtwn_softc *sc = urtwn_reg_softc;
	struct ifnet *ifp;

	if (sc == NULL || sc->sc_dying) {
		printf("wlan: no attached urtwn device\n");
		return -1;
	}
	ifp = &sc->sc_if;
	if (!(ifp->if_flags & IFF_RUNNING)) {
		ifp->if_flags |= IFF_UP;
		printf("wlan: calling if_init %p (fw load + power on, ~10s)...\n",
		    (void *) ifp->if_init);
		ifp->if_init(ifp);
		printf("wlan: if_init done, flags=%x\n", ifp->if_flags);
	}
	return 0;
}

struct wlan_scan_request {
	uint8_t ssid[IEEE80211_NWID_LEN];
	uint8_t len;
};

static void wlan_scan_start(struct urtwn_softc *sc, void *arg) {
	const struct wlan_scan_request *req = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct urtwn_cmd_newstate cmd = { .state = IEEE80211_S_INIT, .arg = -1 };

	ic->ic_roaming = IEEE80211_ROAMING_MANUAL;
	/* Like SCAN_REQ, start from INIT so begin_scan resets the channel
	 * bitmap even when the previous manual scan left the state at SCAN.
	 * Run both transitions on the driver worker to preserve their order. */
	urtwn_newstate_cb(sc, &cmd);
	memcpy(ic->ic_des_essid, req->ssid, req->len);
	ic->ic_des_esslen = req->len;
	memcpy(ic->ic_chan_active, ic->ic_chan_avail, sizeof(ic->ic_chan_active));
	cmd.state = IEEE80211_S_SCAN;
	urtwn_newstate_cb(sc, &cmd);
}

int wlan_port_scan_urtwn(const uint8_t *ssid, size_t len) {
	struct urtwn_softc *sc = urtwn_reg_softc;
	struct wlan_scan_request req = {0};

	if (sc == NULL || sc->sc_dying || !(sc->sc_if.if_flags & IFF_RUNNING) ||
	    len > sizeof(req.ssid) || (len != 0 && ssid == NULL)) {
		return -1;
	}
	if (len != 0) {
		memcpy(req.ssid, ssid, len);
	}
	req.len = len;
	urtwn_do_async(sc, wlan_scan_start, &req, sizeof(req));
	return 0;
}

static void wlan_dump_key(const char *name, const struct ieee80211_key *key) {
	printf("%s cipher=%s flags=%x index=%u txpn=%llu rxpn=%llu\n",
	    name, key->wk_cipher ? key->wk_cipher->ic_name : "none",
	    key->wk_flags, key->wk_keyix,
	    (unsigned long long)key->wk_keytsc,
	    (unsigned long long)key->wk_keyrsc);
}

void wlan_urtwn_dump(void) {
	struct urtwn_softc *sc = urtwn_reg_softc;
	struct ieee80211com *ic;

	if (sc == NULL) {
		printf("wlan: no device\n");
		return;
	}
	ic = &sc->sc_ic;
	if (ic->ic_bss == NULL) {
		printf("wlan: no BSS\n");
		return;
	}
	printf("wlan mac=%s flags=%x mtu=%u\n",
	    ether_sprintf(ic->ic_myaddr), sc->sc_if.if_flags, sc->sc_if.if_mtu);
	printf("wlan bssid=%s ni_flags=%x caps=%x\n",
	    ether_sprintf(ic->ic_bss->ni_bssid), ic->ic_bss->ni_flags, ic->ic_caps);
	wlan_dump_key("unicast", &ic->ic_bss->ni_ucastkey);
	for (unsigned i = 0; i < IEEE80211_WEP_NKID; i++) {
		printf("group[%u] ", i);
		wlan_dump_key("key", &ic->ic_nw_keys[i]);
	}
	printf("wlan stats tx=%llu txerr=%llu rx=%llu rxerr=%llu auth=%x key=%x\n",
	    (unsigned long long)sc->sc_if.if_data.if_opackets,
	    (unsigned long long)sc->sc_if.if_data.if_oerrors,
	    (unsigned long long)sc->sc_if.if_data.if_ipackets,
	    (unsigned long long)sc->sc_if.if_data.if_ierrors,
	    ic->ic_bss->ni_flags, ic->ic_bss->ni_ucastkey.wk_flags);
	printf("wlan crypto no-key=%u wepfail=%u ccmpmic=%u ccmpreplay=%u unauth=%u\n",
	    ic->ic_stats.is_tx_nodefkey, ic->ic_stats.is_rx_wepfail,
	    ic->ic_stats.is_rx_ccmpmic, ic->ic_stats.is_rx_ccmpreplay,
	    ic->ic_stats.is_rx_unauth);
	printf("wlan ccmpformat=%u\n", ic->ic_stats.is_rx_ccmpformat);
	if (sc->sc_if.if_flags & IFF_RUNNING) {
		printf("urtwn RCR=%08x RXFLTMAP=%04x/%04x/%04x SECCFG=%02x\n",
		    urtwn_read_4(sc, R92C_RCR), urtwn_read_2(sc, R92C_RXFLTMAP0),
		    urtwn_read_2(sc, R92C_RXFLTMAP1), urtwn_read_2(sc, R92C_RXFLTMAP2),
		    urtwn_read_1(sc, R92C_SECCFG));
		printf("urtwn BSSID registers=%08x/%04x\n",
		    urtwn_read_4(sc, R92C_BSSID), urtwn_read_2(sc, R92C_BSSID + 4));
	}
	printf("urtwn state=%s opmode=%d ch=%d\n",
	    ic->ic_state >= 0 && ic->ic_state < IEEE80211_S_MAX ?
	        ieee80211_state_name[ic->ic_state] : "?",
	    ic->ic_opmode,
	    ic->ic_curchan != NULL ? ic->ic_curchan->ic_freq : 0);
}

void wlan_urtwn_scan_dump(void) {
	if (urtwn_reg_softc != NULL) {
		ieee80211_iterate_nodes(&urtwn_reg_softc->sc_ic.ic_scan,
		    wlan_print_node_cb, NULL);
	}
}

void wlan_urtwn_detach(void *priv) {
	struct wlan_usb_dev *usb = priv;
	(void) usb;
	/* detach goes through urtwn_detach with the stored device shell */
}

int wlan_port_xmit_urtwn(const uint8_t *frame, size_t len) {
	struct urtwn_softc *sc = urtwn_reg_softc;
	struct ifnet *ifp;
	struct mbuf *m;
	int err;

	if (sc == NULL || frame == NULL || len < sizeof(struct ether_header) ||
	    len > MCLBYTES) {
		return -1;
	}
	ifp = &sc->sc_if;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		return -1;
	}
	m->m_len = m->m_pkthdr.len = (int) len;
	memcpy(mtod(m, void *), frame, len);

	IFQ_ENQUEUE(&ifp->if_snd, m, err);
	if (err != 0) {
		m_freem(m);
		return -1;
	}
	if_start_lock(ifp);
	return (int) len;
}

int wlan_port_get_hwaddr_urtwn(uint8_t addr[6]) {
	struct urtwn_softc *sc = urtwn_reg_softc;
	struct ifnet *ifp;

	if (sc == NULL) {
		return -1;
	}
	ifp = &sc->sc_if;
	if (ifp->if_sadl == NULL) {
		return -1;
	}
	memcpy(addr, CLLADDR(ifp->if_sadl), 6);
	return 0;
}

const struct wlan_chip_driver urtwn_driver = {
	.name = "urtwn",
	.bus = WLAN_BUS_USB,
	.usb_ids = urtwn_usb_ids,
	.attach = wlan_urtwn_attach_bus,
	.detach = wlan_urtwn_detach,
	.stop = NULL,
};
