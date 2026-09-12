/*
 * @file
 * @brief Registration and port adapter for the rtw88 USB driver.
 *
 * The ported net80211 driver (if_rtw88.c) is compiled into this unit so
 * its attachment table and state machine stay static; the chip layer
 * behind it is the imported rtw88 code plus the Linux-API compat layer
 * under compat/.  Everything the shell reaches goes through the
 * bus-neutral driver adapter registered at attach time.
 *
 * @date 12.09.2026
 * @author zhugengyu
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>

#include <port/port.h>
#include <port/bus/usb/port_usb.h>

#include <driver/rtw88/compat/rtw88_glue.h>
#include <driver/rtw88/compat/rtw88_chipvar.h>

/* the ported net80211 driver compiled into this unit; it brings in the
 * net80211 headers the adapter below uses */
#include "if_rtw88.c"

/*
 * After the CD-ROM personality is switched away the dongle enumerates as
 * this; the fake CD-ROM it boots as is handled by the bus layer, which
 * must not claim the WiFi function it turns into.
 */
static const struct wlan_usb_id rtw88u_usb_ids[] = {
	{ 0x0bda, 0xc820 },	/* RTL8821CU */
	{ 0, 0 }
};

/* the first unit that came up: the shell drives one device at a time */
static struct rtw88_softc *rtw88u_reg_sc;
static unsigned int rtw88u_reg_units;

int wlan_rtw88_attach(struct wlan_usb_dev *, void *);

static int wlan_rtw88_attach_bus(void *bus_dev, void *if_priv);
static void wlan_rtw88_detach(void *if_priv);
static int wlan_rtw88_up(void);
static int wlan_rtw88_scan(const uint8_t *ssid, size_t len);
static int wlan_rtw88_xmit(const uint8_t *frame, size_t len);
static int wlan_rtw88_get_hwaddr(uint8_t addr[6]);
static void wlan_rtw88_status(void);
static void wlan_rtw88_scan_dump(void);

static struct wlan_port_adapter rtw88u_adapter = {
	.name = "rtw88u",
	.up = wlan_rtw88_up,
	.scan = wlan_rtw88_scan,
	.xmit = wlan_rtw88_xmit,
	.get_hwaddr = wlan_rtw88_get_hwaddr,
	.status_dump = wlan_rtw88_status,
	.scan_dump = wlan_rtw88_scan_dump,
};

int
wlan_rtw88_attach(struct wlan_usb_dev *usb, void *if_priv)
{
	struct rtw88_softc *sc;
	device_t self;
	struct usb_attach_arg uaa;

	(void) if_priv;

	sc = wlan_kmalloc(sizeof(*sc), M_WAITOK | M_ZERO, M_USBDEV);
	if (sc == NULL)
		return -ENOMEM;
	self = wlan_kmalloc(sizeof(*self), M_WAITOK | M_ZERO, M_USBDEV);
	if (self == NULL) {
		wlan_kfree(sc, M_USBDEV);
		return -ENOMEM;
	}
	snprintf(self->dv_xname, sizeof(self->dv_xname), "rtw88u%u",
	    rtw88u_reg_units++);
	self->dv_private = sc;

	memset(&uaa, 0, sizeof(uaa));
	uaa.uaa_vendor = usb->vendor;
	uaa.uaa_product = usb->product;
	uaa.uaa_device = usb->port_priv;

	/*
	 * The attach only starts the chip bring-up on the rtw88 worker;
	 * the net80211 interface exists once that has run, so register the
	 * adapter right away and let its up() entry wait for it (the
	 * bring-up must not block the USB enumeration thread).
	 */
	rtw88_attach(self, self, &uaa);

	if (!sc->sc_dying && rtw88u_reg_sc == NULL) {
		rtw88u_reg_sc = sc;
		rtw88u_adapter.ic = &sc->sc_ic;
		wlan_port_adapter_register(&rtw88u_adapter);
	}
	return 0;
}

static void
wlan_rtw88_detach(void *if_priv)
{
	(void) if_priv;
	/* detach goes through rtw88_detach with the stored device shell */
}

/*
 * The chip bring-up runs asynchronously (firmware load, efuse, phy
 * calibration take seconds); the shell entry that needs the interface
 * waits for it here instead of in the USB enumeration path.
 */
static int
wlan_rtw88_wait_attached(struct rtw88_softc *sc)
{
	int i;

	for (i = 0; i < 300; i++) {
		if (sc->sc_dying)
			return -1;
		if (sc->sc_attached)
			return 0;
		tsleep(sc, 0, "rtw88up", MAX(1, (int)mstohz(100)));
	}
	return -1;
}

static int
wlan_rtw88_up(void)
{
	struct rtw88_softc *sc = rtw88u_reg_sc;
	struct ifnet *ifp;

	if (sc == NULL || sc->sc_dying) {
		printf("wlan: no attached rtw88u device\n");
		return -1;
	}
	if (wlan_rtw88_wait_attached(sc) != 0) {
		printf("wlan: chip bring-up did not complete\n");
		return -1;
	}
	ifp = &sc->sc_if;
	if (!(ifp->if_flags & IFF_RUNNING)) {
		ifp->if_flags |= IFF_UP;
		printf("wlan: if_init (firmware + power on, a few seconds)\n");
		ifp->if_init(ifp);
		printf("wlan: if_init done, flags=%x\n", ifp->if_flags);
	}
	return 0;
}

/*
 * Scan start, on the rtw88 worker: net80211 walks the channel list by
 * re-entering S_SCAN, so the transition order matters and has to happen
 * where the chip calls are allowed to sleep.
 */
struct rtw88u_scan_req {
	uint8_t ssid[IEEE80211_NWID_LEN];
	uint8_t len;
};

static void
wlan_rtw88_scan_start(void *arg)
{
	const struct rtw88u_scan_req *req = arg;
	struct rtw88_softc *sc = rtw88u_reg_sc;
	struct ieee80211com *ic = &sc->sc_ic;

	ic->ic_roaming = IEEE80211_ROAMING_MANUAL;
	/* start from INIT so begin_scan resets the channel bitmap even when
	 * a previous manual scan left the state at SCAN */
	sc->sc_cmd_state = IEEE80211_S_INIT;
	sc->sc_cmd_arg = -1;
	rtw88_newstate_cb(sc);

	if (req->len != 0) {
		memset(ic->ic_des_essid, 0, sizeof(ic->ic_des_essid));
		memcpy(ic->ic_des_essid, req->ssid, req->len);
	}
	ic->ic_des_esslen = req->len;
	memcpy(ic->ic_chan_active, ic->ic_chan_avail,
	    sizeof(ic->ic_chan_active));

	sc->sc_cmd_state = IEEE80211_S_SCAN;
	sc->sc_cmd_arg = -1;
	rtw88_newstate_cb(sc);
}

static int
wlan_rtw88_scan(const uint8_t *ssid, size_t len)
{
	struct rtw88_softc *sc = rtw88u_reg_sc;
	struct rtw88u_scan_req *req;

	if (sc == NULL || sc->sc_dying || !sc->sc_attached ||
	    len > IEEE80211_NWID_LEN || (len != 0 && ssid == NULL)) {
		return -1;
	}
	req = wlan_kmalloc(sizeof(*req), M_WAITOK | M_ZERO, M_DEVBUF);
	if (req == NULL)
		return -1;
	if (len != 0)
		memcpy(req->ssid, ssid, len);
	req->len = (uint8_t) len;

	/*
	 * The worker owns the request from here; the wrapper frees it so
	 * the allocation cannot leak when the queue is already gone.
	 */
	if (rtw88_call_async(wlan_rtw88_scan_start, req) != 0) {
		wlan_kfree(req, M_DEVBUF);
		return -1;
	}
	return 0;
}

static int
wlan_rtw88_xmit(const uint8_t *frame, size_t len)
{
	struct rtw88_softc *sc = rtw88u_reg_sc;
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

static int
wlan_rtw88_get_hwaddr(uint8_t addr[6])
{
	struct rtw88_softc *sc = rtw88u_reg_sc;
	struct ifnet *ifp;

	if (sc == NULL || !sc->sc_attached) {
		return -1;
	}
	ifp = &sc->sc_if;
	if (ifp->if_sadl == NULL) {
		return -1;
	}
	memcpy(addr, CLLADDR(ifp->if_sadl), 6);
	return 0;
}

static void
wlan_rtw88_dump_key(const char *name, const struct ieee80211_key *key)
{
	printf("%s cipher=%s flags=%x index=%u txpn=%llu rxpn=%llu\n",
	    name, key->wk_cipher ? key->wk_cipher->ic_name : "none",
	    key->wk_flags, key->wk_keyix,
	    (unsigned long long) key->wk_keytsc,
	    (unsigned long long) key->wk_keyrsc);
}

static void
wlan_rtw88_status(void)
{
	struct rtw88_softc *sc = rtw88u_reg_sc;
	struct ieee80211com *ic;

	if (sc == NULL) {
		printf("wlan: no device\n");
		return;
	}
	printf("wlan: rtw88u chip=%s fw=%s efuse-mac=%s\n",
	    sc->sc_chip != NULL && rtw88_chip_ready(sc->sc_chip) ?
	        "ready" : "not ready",
	    sc->sc_info.fw_version[0] != '\0' ?
	        (const char *) sc->sc_info.fw_version : "?",
	    sc->sc_info.efuse_valid ?
	        (const char *) ether_sprintf(sc->sc_info.mac_addr) : "?");
	if (!sc->sc_attached) {
		printf("wlan: net80211 interface not attached yet\n");
		return;
	}

	ic = &sc->sc_ic;
	printf("wlan mac=%s flags=%x mtu=%u\n", ether_sprintf(ic->ic_myaddr),
	    sc->sc_if.if_flags, sc->sc_if.if_mtu);
	printf("wlan bssid=%s ni_flags=%x caps=%x\n",
	    ether_sprintf(ic->ic_bss->ni_bssid), ic->ic_bss->ni_flags,
	    ic->ic_caps);
	wlan_rtw88_dump_key("unicast", &ic->ic_bss->ni_ucastkey);
	for (unsigned i = 0; i < IEEE80211_WEP_NKID; i++) {
		printf("group[%u] ", i);
		wlan_rtw88_dump_key("key", &ic->ic_nw_keys[i]);
	}
	printf("wlan stats tx=%llu txerr=%llu rx=%llu rxerr=%llu\n",
	    (unsigned long long) sc->sc_if.if_data.if_opackets,
	    (unsigned long long) sc->sc_if.if_data.if_oerrors,
	    (unsigned long long) sc->sc_if.if_data.if_ipackets,
	    (unsigned long long) sc->sc_if.if_data.if_ierrors);
	printf("wlan crypto no-key=%u wepfail=%u ccmpmic=%u ccmpreplay=%u "
	    "badkeyid=%u unauth=%u\n",
	    ic->ic_stats.is_tx_nodefkey, ic->ic_stats.is_rx_wepfail,
	    ic->ic_stats.is_rx_ccmpmic, ic->ic_stats.is_rx_ccmpreplay,
	    ic->ic_stats.is_rx_badkeyid, ic->ic_stats.is_rx_unauth);
	printf("wlan ccmpformat=%u sw-ccmp=%u\n", ic->ic_stats.is_rx_ccmpformat,
	    ic->ic_stats.is_crypto_ccmp);
	printf("wlan state=%s opmode=%d ch=%d\n",
	    ic->ic_state < IEEE80211_S_MAX ?
	        ieee80211_state_name[ic->ic_state] : "?",
	    ic->ic_opmode,
	    ic->ic_curchan != NULL ? ic->ic_curchan->ic_freq : 0);
}

static void
wlan_rtw88_print_node(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211_channel *ch = ni->ni_chan;

	(void) arg;
	printf("  %02x:%02x:%02x:%02x:%02x:%02x  ch=%d  rssi=%u  %s  ssid=%.*s\n",
	    ni->ni_bssid[0], ni->ni_bssid[1], ni->ni_bssid[2],
	    ni->ni_bssid[3], ni->ni_bssid[4], ni->ni_bssid[5],
	    ch != NULL ? ch->ic_freq : 0, ni->ni_rssi,
	    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ? "enc " : "open",
	    ni->ni_esslen, ni->ni_essid);
}

static void
wlan_rtw88_scan_dump(void)
{
	if (rtw88u_reg_sc != NULL && rtw88u_reg_sc->sc_attached) {
		ieee80211_iterate_nodes(&rtw88u_reg_sc->sc_ic.ic_scan,
		    wlan_rtw88_print_node, NULL);
	}
}

const struct wlan_chip_driver rtw88u_driver = {
	.name = "rtw88u",
	.bus = WLAN_BUS_USB,
	.usb_ids = rtw88u_usb_ids,
	.attach = wlan_rtw88_attach_bus,
	.detach = wlan_rtw88_detach,
	.stop = NULL,
};

static int
wlan_rtw88_attach_bus(void *bus_dev, void *if_priv)
{
	return wlan_rtw88_attach((struct wlan_usb_dev *) bus_dev, if_priv);
}
