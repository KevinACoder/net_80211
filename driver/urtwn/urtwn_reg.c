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

#include <driver/urtwn/if_urtwnvar.h>
#include <port/embox/wlan_port_embox.h>

/* the verbatim import compiled into this unit so its static glue
 * (CFATTACH_DECL_NEW tables) stays intact */
#include "if_urtwn.c"

static const struct wlan_usb_id urtwn_usb_ids[] = {
	{ 0x0bda, 0x8179 }, /* RTL8188EU */
	{ 0x0bda, 0x0179 }, /* RTL8188EUS */
	{ 0, 0 }
};


struct urtwn_softc *urtwn_reg_softc;

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

void wlan_urtwn_up(void) {
	struct urtwn_softc *sc = urtwn_reg_softc;
	struct ifnet *ifp;

	if (sc == NULL || sc->sc_dying) {
		printf("wlan: no attached urtwn device\n");
		return;
	}
	ifp = &sc->sc_if;
	if (!(ifp->if_flags & IFF_RUNNING)) {
		ifp->if_flags |= IFF_UP;
		printf("wlan: calling if_init %p (fw load + power on, ~10s)...\n",
		    (void *) ifp->if_init);
		ifp->if_init(ifp);
		printf("wlan: if_init done, flags=%x\n", ifp->if_flags);
	}
}

void wlan_urtwn_dump(void) {
	struct urtwn_softc *sc = urtwn_reg_softc;
	struct ieee80211com *ic;

	if (sc == NULL) {
		printf("wlan: no device\n");
		return;
	}
	ic = &sc->sc_ic;
	printf("urtwn state=%s opmode=%d ch=%d\n",
	    ic->ic_state >= 0 && ic->ic_state < IEEE80211_S_MAX ?
	        ieee80211_state_name[ic->ic_state] : "?",
	    ic->ic_opmode,
	    ic->ic_curchan != NULL ? ic->ic_curchan->ic_freq : 0);
	ieee80211_iterate_nodes(&ic->ic_scan, wlan_print_node_cb, NULL);
}

void wlan_urtwn_detach(void *priv) {
	struct wlan_usb_dev *usb = priv;
	(void) usb;
	/* detach goes through urtwn_detach with the stored device shell */
}

int wlan_port_xmit(const uint8_t *frame, size_t len) {
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

int wlan_port_get_hwaddr(uint8_t addr[6]) {
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
	.attach = wlan_urtwn_attach,
	.detach = wlan_urtwn_detach,
	.stop = NULL,
};
