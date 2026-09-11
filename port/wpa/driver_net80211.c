/*
 * @file
 * @brief wpa_supplicant driver wrapper for the net80211 port.
 *
 * Modeled on src/drivers/driver_bsd.c, but instead of ioctls and the
 * routing socket it calls the net80211 entry points of the port
 * directly and raises events through the eloop event queue. The
 * net80211 state machine owns the MLME: associate() programs the
 * desired SSID/BSSID/RSN parameters and joins the BSS, auth/assoc
 * frames are built by net80211, and state transitions come back
 * through the wrapped ic_newstate hook.
 *
 * The lwIP presentation is chained in: the supplicant owns the port
 * event hook once started, so association transitions are reported to
 * wlan_lwip_assoc_notify() to drive the link state and DHCP.
 *
 * @date 11.09.2026
 * @author zhugengyu
 */

/* The net80211/BSD world must come first: the compat shadow types
 * (kmutex_t, callout_t, ...) have to exist before the driver headers
 * use them, exactly like urtwn_reg.c orders its includes. */
#include <sys/queue.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rndsource.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_proto.h>
#include <net80211/ieee80211_node.h>
#include <net80211/ieee80211_crypto.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <driver/urtwn/rtwnreg.h>
#include <driver/urtwn/if_urtwnreg.h>
#include <driver/urtwn/if_urtwnvar.h>
#include <port/port.h>

/* Then the hostap world; WPA_OUI_TYPE is defined differently on the
 * two sides and this unit needs neither. */
#undef WPA_OUI_TYPE
#undef free
#undef malloc
#undef realloc
#include <includes.h>
#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"

#include "wpa_port_glue.h"
#include "wpa_port_api.h"

/* the lwIP presentation (host world; the netif and the DHCP wiring
 * live there) */
extern void wlan_lwip_assoc_notify(int assoc);
extern int wlan_lwip_ensure(void);

struct freertos_drv_data {
	void *ctx; /* wpa_s */
	struct ieee80211com *ic;
	struct wpa_driver_capa capa;
	int associated;
	int if_up;
	int scan_pending;
};

/* single adapter: the net80211 ic_newstate hook has no priv slot */
static struct freertos_drv_data g_drv;
extern const struct ieee80211_cipher ieee80211_cipher_ccmp;

/* ------------------------------------------------------------------ */
/* net80211 wiring */

extern void *wlan_port_get_ic(void);
extern int wlan_port_up(void);

/* bring the interface up on the calling (shell) thread; the chip init
 * can block on its workers, which must not run on the supplicant thread */
int wlan_supp_ensure_up(void)
{
	struct ieee80211com *ic = wlan_port_get_ic();

	if (ic == NULL) {
		return -1;
	}
	ic->ic_roaming = IEEE80211_ROAMING_MANUAL;
	wlan_port_up();
	return (ic->ic_ifp != NULL &&
	    (ic->ic_ifp->if_flags & IFF_RUNNING)) ? 0 : -1;
}

/*
 * Queue protocol notifications from the driver worker for the supplicant.
 */
static void freertos_wireless_event(enum wlan_port_event event,
    const uint8_t *addr, void *arg) {
	struct freertos_drv_data *drv = arg;
	union wpa_event_data data;

	(void) addr;
	if (drv->ctx == NULL) {
		return;
	}
	memset(&data, 0, sizeof(data));
	switch (event) {
	case WLAN_PORT_SCAN_DONE:
		if (__atomic_exchange_n(&drv->scan_pending, 0, __ATOMIC_ACQ_REL)) {
			wpa_printf(MSG_DEBUG, "wpa: scan complete");
			wpa_send_event(drv->ctx, EVENT_SCAN_RESULTS, &data);
		}
		break;
	case WLAN_PORT_ASSOC:
		drv->associated = 1;
		wlan_lwip_assoc_notify(1);
		wpa_printf(MSG_DEBUG, "wpa: association complete");
		wpa_send_event(drv->ctx, EVENT_ASSOC, NULL);
		break;
	case WLAN_PORT_DISASSOC:
		drv->associated = 0;
		wlan_lwip_assoc_notify(0);
		wpa_send_event(drv->ctx, EVENT_DISASSOC, NULL);
		break;
	}
}

/*
 * The supplicant scans with ap_scan=1: scan2() starts the net80211
 * scan. This timeout reports an aborted scan if no completion arrives.
 */
static void freertos_scan_poll(void *eloop_ctx, void *timeout_ctx) {
	union wpa_event_data data;

	(void) eloop_ctx;
	(void) timeout_ctx;

	if (__atomic_exchange_n(&g_drv.scan_pending, 0, __ATOMIC_ACQ_REL)) {
		memset(&data, 0, sizeof(data));
		data.scan_info.aborted = 1;
		ieee80211_new_state(g_drv.ic, IEEE80211_S_INIT, -1);
		wpa_printf(MSG_ERROR, "wpa: scan timed out");
		wpa_send_event(g_drv.ctx, EVENT_SCAN_RESULTS, &data);
	}
}

/* ------------------------------------------------------------------ */
/* EAPOL delivery (usb worker context, copies only) */

static void freertos_eapol_rx(const uint8_t src[6], const uint8_t *buf,
    size_t len, void *arg) {
	struct freertos_drv_data *drv = arg;
	union wpa_event_data data;

	if (drv->ctx == NULL) {
		return;
	}
	memset(&data, 0, sizeof(data));
	data.eapol_rx.src = src;
	data.eapol_rx.data = buf;
	data.eapol_rx.data_len = len;
	wpa_send_event(drv->ctx, EVENT_EAPOL_RX, &data);
}

/* ------------------------------------------------------------------ */
/* ops */

static void *freertos_global_init(void *ctx) {
	(void) ctx;
	/* the single adapter is the global state */
	return &g_drv;
}

static void freertos_global_deinit(void *priv) {
	(void) priv;
}

static void *freertos_init2(void *ctx, const char *ifname, void *global_priv) {
	struct ieee80211com *ic;

	(void) ifname;
	(void) global_priv;

	memset(&g_drv, 0, sizeof(g_drv));

	g_drv.ic = wlan_port_get_ic();
	if (g_drv.ic == NULL) {
		wpa_printf(MSG_ERROR, "wpa: no wlan device attached");
		return NULL;
	}
	g_drv.ctx = ctx;

	ic = g_drv.ic;
	ieee80211_crypto_register(&ieee80211_cipher_ccmp);
	/* The supplicant owns BSS selection and receives scan completion. */
	ic->ic_roaming = IEEE80211_ROAMING_MANUAL;

	wlan_port_set_event_handler(freertos_wireless_event, &g_drv);
	wlan_port_set_eapol_rx(freertos_eapol_rx, &g_drv);
	if (wlan_lwip_ensure() != 0) {
		wlan_port_set_event_handler(NULL, NULL);
		wlan_port_set_eapol_rx(NULL, NULL);
		g_drv.ctx = NULL;
		return NULL;
	}

	/* one-shot bring-up: firmware load + power on */
	g_drv.if_up = 1;

	g_drv.capa.key_mgmt = WPA_KEY_MGMT_PSK;
	g_drv.capa.enc = WPA_DRIVER_CAPA_ENC_CCMP;
	g_drv.capa.auth = WPA_DRIVER_AUTH_OPEN;
	g_drv.capa.max_scan_ssids = 1;

	return &g_drv;
}

static void freertos_deinit(void *priv) {
	struct freertos_drv_data *drv = priv;
	(void) drv;
	__atomic_store_n(&g_drv.scan_pending, 0, __ATOMIC_RELEASE);
	eloop_cancel_timeout(freertos_scan_poll, NULL, NULL);
	wlan_port_set_event_handler(NULL, NULL);
	wlan_port_set_eapol_rx(NULL, NULL);
	memset(&g_drv, 0, sizeof(g_drv));
}

static int freertos_get_bssid(void *priv, u8 *bssid) {
	struct freertos_drv_data *drv = priv;
	struct ieee80211com *ic = drv->ic;

	if (!drv->associated) {
		return -1;
	}
	os_memcpy(bssid, ic->ic_bss->ni_bssid, ETH_ALEN);
	return 0;
}

static int freertos_get_ssid(void *priv, u8 *ssid) {
	struct freertos_drv_data *drv = priv;
	struct ieee80211com *ic = drv->ic;

	if (!drv->associated) {
		return -1;
	}
	os_memcpy(ssid, ic->ic_bss->ni_essid, ic->ic_bss->ni_esslen);
	return (int) ic->ic_bss->ni_esslen;
}

static int freertos_set_countermeasures(void *priv, int enabled) {
	(void) priv;
	(void) enabled;
	return 0;
}

static int freertos_scan2(void *priv, struct wpa_driver_scan_params *params) {
	struct freertos_drv_data *drv = priv;
	const u8 *ssid = params->num_ssids ? params->ssids[0].ssid : NULL;
	size_t len = params->num_ssids ? params->ssids[0].ssid_len : 0;

	if (params->num_ssids > 1 || len > IEEE80211_NWID_LEN ||
	    __atomic_exchange_n(&drv->scan_pending, 1, __ATOMIC_ACQ_REL)) {
		return -1;
	}
	eloop_cancel_timeout(freertos_scan_poll, NULL, NULL);
	if (eloop_register_timeout(30, 0, freertos_scan_poll, NULL, NULL) < 0 ||
	    wlan_port_scan(ssid, len) < 0) {
		__atomic_store_n(&drv->scan_pending, 0, __ATOMIC_RELEASE);
		eloop_cancel_timeout(freertos_scan_poll, NULL, NULL);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "wpa: scan requested ssid_len=%u", (unsigned) len);
	return 0;
}

/*
 * One scan result: SSID IE + supported rates IE + the RSN/WPA IE the
 * stack kept from the beacon, laid out after the wpa_scan_res header.
 */
static struct wpa_scan_res *freertos_scan_entry(struct ieee80211_node *ni) {
	struct wpa_scan_res *r;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	size_t ssid_ie_len, rates_ie_len, wpa_ie_len, ie_len, len;
	u8 *ie;
	int i;

	ssid_ie_len = 2 + ni->ni_esslen;
	rates_ie_len = rs->rs_nrates > 0 ? 2 + rs->rs_nrates : 0;
	wpa_ie_len = ni->ni_wpa_ie != NULL ? 2 + ni->ni_wpa_ie[1] : 0;
	ie_len = ssid_ie_len + rates_ie_len + wpa_ie_len;

	len = sizeof(struct wpa_scan_res) + ie_len;
	r = os_zalloc(len);
	if (r == NULL) {
		return NULL;
	}
	os_memcpy(r->bssid, ni->ni_bssid, ETH_ALEN);
	r->freq = ni->ni_chan != NULL ? ni->ni_chan->ic_freq : 0;
	r->beacon_int = ni->ni_intval;
	r->caps = ni->ni_capinfo;
	r->level = ni->ni_rssi;
	r->ie_len = ie_len;
	r->beacon_ie_len = 0;

	ie = (u8 *) (r + 1);
	{
		*ie++ = WLAN_EID_SSID;
		*ie++ = (u8) ni->ni_esslen;
		os_memcpy(ie, ni->ni_essid, ni->ni_esslen);
		ie += ni->ni_esslen;
	}
	if (rates_ie_len > 0) {
		*ie++ = WLAN_EID_SUPP_RATES;
		*ie++ = (u8) rs->rs_nrates;
		for (i = 0; i < rs->rs_nrates; i++) {
			*ie++ = rs->rs_rates[i];
		}
	}
	if (wpa_ie_len > 0) {
		*ie++ = ni->ni_wpa_ie[0];
		*ie++ = ni->ni_wpa_ie[1];
		os_memcpy(ie, ni->ni_wpa_ie + 2, ni->ni_wpa_ie[1]);
		ie += ni->ni_wpa_ie[1];
	}
	return r;
}

struct freertos_scan_collect {
	struct wpa_scan_results *res;
	int overflow;
};

/* ieee80211_iter_func signature: (void *arg, struct ieee80211_node *) */
static void freertos_collect_cb(void *arg, struct ieee80211_node *ni) {
	struct freertos_scan_collect *collect = arg;
	struct wpa_scan_res *r;

	if (collect->res->num >= 64) {
		collect->overflow = 1;
		return;
	}
	r = freertos_scan_entry(ni);
	if (r == NULL) {
		return;
	}
	collect->res->res[collect->res->num++] = r;
}

static struct wpa_scan_results *freertos_get_scan_results2(void *priv) {
	struct freertos_drv_data *drv = priv;
	struct ieee80211com *ic = drv->ic;
	struct wpa_scan_results *res;
	struct freertos_scan_collect collect;

	res = os_zalloc(sizeof(*res));
	if (res == NULL) {
		return NULL;
	}
	res->res = os_calloc(64, sizeof(struct wpa_scan_res *));
	if (res->res == NULL) {
		os_free(res);
		return NULL;
	}

	collect.res = res;
	collect.overflow = 0;
	ieee80211_iterate_nodes(&ic->ic_scan, freertos_collect_cb, &collect);
	if (collect.overflow) {
		wpa_printf(MSG_INFO, "wpa: scan table truncated");
	}
	return res;
}

static int freertos_deauthenticate(void *priv, const u8 *addr,
    u16 reason_code) {
	struct freertos_drv_data *drv = priv;
	struct ieee80211com *ic = drv->ic;

	(void) addr;
	__atomic_store_n(&drv->scan_pending, 0, __ATOMIC_RELEASE);
	eloop_cancel_timeout(freertos_scan_poll, NULL, NULL);
	drv->associated = 0;
	ieee80211_new_state(ic, IEEE80211_S_INIT, reason_code);
	return 0;
}

static int freertos_associate(void *priv,
    struct wpa_driver_associate_params *params) {
	struct freertos_drv_data *drv = priv;
	struct ieee80211com *ic = drv->ic;
	struct ieee80211_node *ni;
	u8 *opt_ie;

	if (params->ssid == NULL || params->ssid_len == 0 ||
	    params->ssid_len > sizeof(ic->ic_des_essid)) {
		return -1;
	}

	/* desired SSID (and BSSID when the supplicant picked one) */
	memset(ic->ic_des_essid, 0, sizeof(ic->ic_des_essid));
	memcpy(ic->ic_des_essid, params->ssid, params->ssid_len);
	ic->ic_des_esslen = params->ssid_len;
	ic->ic_flags &= ~IEEE80211_F_DESBSSID;
	if (params->bssid != NULL) {
		IEEE80211_ADDR_COPY(ic->ic_des_bssid, params->bssid);
		ic->ic_flags |= IEEE80211_F_DESBSSID;
	}

	/* privacy/WPA mode flags, the ioctl equivalents */
	ic->ic_flags &= ~(IEEE80211_F_WPA1 | IEEE80211_F_WPA2);
	if (params->wpa_ie != NULL && params->wpa_ie_len > 0) {
		ic->ic_flags |= (params->wpa_ie[0] == WLAN_EID_RSN ? IEEE80211_F_WPA2
						       : IEEE80211_F_WPA1);
	}
	if (params->pairwise_suite != WPA_CIPHER_NONE ||
	    params->group_suite != WPA_CIPHER_NONE) {
		ic->ic_flags |= IEEE80211_F_PRIVACY;
	} else {
		ic->ic_flags &= ~IEEE80211_F_PRIVACY;
	}
	if (params->drop_unencrypted) {
		ic->ic_flags |= IEEE80211_F_DROPUNENC;
	} else {
		ic->ic_flags &= ~IEEE80211_F_DROPUNENC;
	}

	/* the RSN IE travels in ic_opt_ie: the assoc req builder appends
	 * it verbatim (the SIOCS80211 OPTIE path) */
	opt_ie = NULL;
	if (params->wpa_ie != NULL && params->wpa_ie_len > 0) {
		opt_ie = os_memdup(params->wpa_ie, params->wpa_ie_len);
		if (opt_ie == NULL) {
			return -1;
		}
	}
	if (ic->ic_opt_ie != NULL) {
		os_free(ic->ic_opt_ie);
	}
	ic->ic_opt_ie = opt_ie;
	ic->ic_opt_ie_len = opt_ie != NULL ? params->wpa_ie_len : 0;

	/* join the BSS the supplicant selected: net80211 refetches the
	 * node from the scan table and runs AUTH -> ASSOC -> RUN */
	ni = params->bssid != NULL ?
	    ieee80211_find_node(&ic->ic_scan, params->bssid) : NULL;
	if (ni == NULL) {
		wpa_printf(MSG_INFO,
		    "wpa: bss not in scan table, rescanning");
		return -1;
	}
	/* the radio must sit on the BSS channel before the join runs;
	 * the port has no channel-change callback of its own */
	if (ni->ni_chan != NULL) {
		ic->ic_curchan = ni->ni_chan;
	}
	/* sta_join consumes the reference. Gate the port on 802.1X only
	 * for encrypted networks: for open ones the state machine
	 * authorizes the node at RUN, with 802.1X set unconditionally no
	 * data frame would ever pass the authorized check */
	ic->ic_bss->ni_authmode =
	    (ic->ic_flags & IEEE80211_F_PRIVACY) != 0 ?
	    IEEE80211_AUTH_8021X : IEEE80211_AUTH_OPEN;
	ieee80211_sta_join(ic, ni);
	return 0;
}

static int freertos_set_key(void *priv, struct wpa_driver_set_key_params *p) {
	struct freertos_drv_data *drv = priv;
	struct ieee80211com *ic = drv->ic;
	struct ieee80211_key *wk;
	struct ieee80211_node *ni;
	int is_group;
	int ret = -1;

	if (p->key_idx < 0 || p->key_idx >= IEEE80211_WEP_NKID ||
	    (p->alg != WPA_ALG_NONE && (p->alg != WPA_ALG_CCMP ||
	    p->key == NULL || p->key_len != 16))) {
		wpa_printf(MSG_INFO, "wpa: unsupported key alg %d", p->alg);
		return -1;
	}
	is_group = p->addr == NULL || (p->addr[0] & 0x01) != 0;

	ieee80211_key_update_begin(ic);
	if (is_group) {
		wk = &ic->ic_nw_keys[p->key_idx];
	} else {
		ni = ieee80211_ref_node(ic->ic_bss);
		wk = &ni->ni_ucastkey;
	}

	if (p->alg == WPA_ALG_NONE) {
		ret = ieee80211_crypto_delkey(ic, wk) ? 0 : -1;
		goto out;
	}
	if (ieee80211_crypto_newkey(ic, IEEE80211_CIPHER_AES_CCM,
		IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV |
		(is_group ? IEEE80211_KEY_GROUP : 0), wk)) {
		wk->wk_keylen = (u_int) p->key_len;
		wk->wk_keyrsc = 0;
		wk->wk_keytsc = 0;
		memset(wk->wk_key, 0, sizeof(wk->wk_key));
		os_memcpy(wk->wk_key, p->key, p->key_len);
		if (p->seq != NULL && p->seq_len > 0 &&
		    p->seq_len <= sizeof(u64)) {
			u64 pn = 0;
			int i;

			for (i = p->seq_len - 1; i >= 0; i--) {
				pn = (pn << 8) | p->seq[i];
			}
			wk->wk_keyrsc = pn;
		}
		ret = ieee80211_crypto_setkey(ic, wk,
		    is_group ? (u8 *) etherbroadcastaddr
			     : ic->ic_bss->ni_macaddr) ? 0 : -1;
		if (is_group && p->set_tx) {
			ic->ic_def_txkey = p->key_idx;
		}
		if (!is_group) {
			/* net80211 gates data frames on node authorization;
			 * the supplicant core opens the port only through
			 * set_supp_port, which a PSK build (EAPOL compiled
			 * in) never reaches. The pairwise key installs only
			 * after the 4-way handshake completed, so open the
			 * port here */
			ieee80211_node_authorize(ic->ic_bss);
		}
	} else {
		wpa_printf(MSG_ERROR, "wpa: newkey failed");
	}
out:
	ieee80211_key_update_end(ic);
	if (!is_group) {
		ieee80211_free_node(ni);
	}

	wpa_printf(ret ? MSG_ERROR : MSG_DEBUG, "wpa: %s key %d alg=%d result=%d",
	    is_group ? "group" : "pairwise", p->key_idx, p->alg, ret);
	return ret;
}

static int freertos_set_supp_port(void *priv, int authorized) {
	struct freertos_drv_data *drv = priv;
	struct ieee80211_node *ni = drv->ic->ic_bss;

	if (ni != NULL) {
		if (authorized) {
			ieee80211_node_authorize(ni);
		} else {
			ieee80211_node_unauthorize(ni);
		}
	}
	return 0;
}

static int freertos_get_capa(void *priv, struct wpa_driver_capa *capa) {
	struct freertos_drv_data *drv = priv;

	*capa = drv->capa;
	return 0;
}

/* the symbol keeps the vanilla drivers.c export name
 * (CONFIG_DRIVER_EMBOX) so the vendored tree stays untouched */
const struct wpa_driver_ops wpa_driver_embox_ops = {
	.name = "embox",
	.desc = "net80211 port on the FreeRTOS test system",
	.global_init = freertos_global_init,
	.global_deinit = freertos_global_deinit,
	.init2 = freertos_init2,
	.deinit = freertos_deinit,
	.get_bssid = freertos_get_bssid,
	.get_ssid = freertos_get_ssid,
	.set_countermeasures = freertos_set_countermeasures,
	.scan2 = freertos_scan2,
	.get_scan_results2 = freertos_get_scan_results2,
	.deauthenticate = freertos_deauthenticate,
	.associate = freertos_associate,
	.get_capa = freertos_get_capa,
	.set_key = freertos_set_key,
	.set_supp_port = freertos_set_supp_port,
};
