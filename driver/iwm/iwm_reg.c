/*
 * @file
 * @brief iwm chip driver registration for the port interface.
 *
 * iwm_attach() keeps its NetBSD autoconf shape; this adapter plays the
 * role of the PCI bus attachment: softc allocation, the device shell,
 * the pci_attach_args built from the port device, and the control /
 * diagnostic hooks the port shell dispatches onto. Every driver entry
 * runs under the port serializer (the splnet() discipline of the
 * import); sleeping waits inside drop it.
 *
 * @date 10.09.2026
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
#include <net80211/ieee80211_amrr.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/* if_iwm.c defines these after the headers; the reg header needs
 * them already */
#include <sys/endian.h>
#ifndef le16_to_cpup
#define le16_to_cpup(_a_) (le16toh(*(const uint16_t *)(_a_)))
#define le32_to_cpup(_a_) (le32toh(*(const uint32_t *)(_a_)))
#endif

#include <dev/pci/if_iwmreg.h>
#include <dev/pci/if_iwmvar.h>
#include <port/port.h>
#include <port/bus/pcie/port_pcie.h>

#ifdef WLAN_PORT_FREERTOS
#include <port/osal/freertos/wlan_port_freertos.h>
#else
#include <port/osal/embox/wlan_port_embox.h>
#endif

/* the verbatim import compiled into this unit so its static glue
 * (CFATTACH_DECL_NEW tables) stays intact */
#include "if_iwm.c"

static const struct wlan_pcie_id iwm_pcie_ids[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WIFI_LINK_7260_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WIFI_LINK_7260_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WIFI_LINK_3160_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WIFI_LINK_3160_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WIFI_LINK_7265_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_WIFI_LINK_7265_2 },
	{ 0, 0 }
};

struct iwm_softc *iwm_reg_softc;

int iwm_reg_up(void);
static int iwm_reg_scan(const uint8_t *ssid, size_t len);
int wlan_port_xmit_iwm(const uint8_t *frame, size_t len);
int wlan_port_get_hwaddr_iwm(uint8_t addr[6]);
static void iwm_reg_status_dump(void);
static void iwm_reg_scan_dump(void);

static struct wlan_port_adapter iwm_adapter = {
	.name = "iwm",
	.up = iwm_reg_up,
	.scan = iwm_reg_scan,
	.xmit = wlan_port_xmit_iwm,
	.get_hwaddr = wlan_port_get_hwaddr_iwm,
	.status_dump = iwm_reg_status_dump,
	.scan_dump = iwm_reg_scan_dump,
};

static int iwm_reg_attach_bus(void *bus_dev, void *if_priv) {
	struct wlan_pcie_dev *pcie = bus_dev;
	struct iwm_softc *sc;
	struct device *self;
	struct pci_attach_args pa;

	(void) if_priv;
	sc = wlan_kmalloc(sizeof(struct iwm_softc), M_WAITOK | M_ZERO,
	    M_DEVBUF);
	if (sc == NULL) {
		return -ENOMEM;
	}
	self = wlan_kmalloc(sizeof(struct device), M_WAITOK | M_ZERO,
	    M_DEVBUF);
	if (self == NULL) {
		wlan_kfree(sc, M_DEVBUF);
		return -ENOMEM;
	}
	snprintf(self->dv_xname, sizeof(self->dv_xname), "iwm%u",
	    (unsigned) (wlan_port_if_n > 0 ? wlan_port_if_n - 1 : 0));
	self->dv_private = sc;

	memset(&pa, 0, sizeof(pa));
	pa.pa_tag = pcie->env_dev;
	pa.pa_id = ((uint32_t) pcie->device << 16) | pcie->vendor;

	wlan_port_serializer_lock();
	{
		/* bring the load sequence's DPRINTFs up while the port
		 * settles; the imported file keeps its own knob */
		extern int iwm_debug;

		iwm_debug = 2;
	}
	iwm_attach(NULL, self, &pa);
	wlan_port_serializer_unlock();
	{
		/* the load-sequence verbosity served its purpose; keep
		 * the run-time console quiet (rx probe prints are its
		 * own always-on lines) */
		extern int iwm_debug;

		iwm_debug = 0;
	}

	if (!ISSET(sc->sc_flags, IWM_FLAG_ATTACHED)) {
		{
			extern volatile unsigned wlan_pcie_intr_fired;

			/* card side: did the firmware DMA finish? */
			uint32_t buf_sts = IWM_READ(sc,
			    IWM_FH_TCSR_CHNL_TX_BUF_STS_REG(
				IWM_FH_SRVC_CHNL));
			uint32_t cfg = IWM_READ(sc,
			    IWM_FH_TCSR_CHNL_TX_CONFIG_REG(
				IWM_FH_SRVC_CHNL));
			uint32_t tssr = IWM_READ(sc,
			    IWM_FH_TSSR_TX_STATUS_REG);
			int idle = !!(tssr &
			    IWM_FH_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(
				IWM_FH_SRVC_CHNL));

			printf("iwm: fh buf_sts=%08x cfg=%08x "
			    "tssr=%08x idle=%d\n",
			    buf_sts, cfg, tssr, idle);
		}
		{
			/* host side: ITS command queue progress and the
			 * LPI's property/pending state */
			volatile uint64_t *its =
			    (volatile uint64_t *) (uintptr_t) 0xFD440000ULL;
			volatile uint8_t *sgi =
			    (volatile uint8_t *) (uintptr_t) 0xFD420000ULL;
			uint64_t cw = its[0x88 / 8];
			uint64_t cr = its[0x90 / 8];
			extern volatile unsigned wlan_pcie_intr_fired;

			printf("iwm: its cw=%llu cr=%llu\n",
			    (unsigned long long) cw,
			    (unsigned long long) cr);
			u8 lpi_prop = 0xff, lpi_pend = 0xff;

			{
				extern void intr_dump_lpi(uint32_t, u8 *,
				    u8 *);

				intr_dump_lpi(8192, &lpi_prop,
				    &lpi_pend);
			}
			printf("iwm: lpi 8192 enable=%02x pend=%02x "
			    "intr_fired=%u\n",
			    lpi_prop, lpi_pend, wlan_pcie_intr_fired);
		}
		return -EIO;
	}
	/* remember the first healthy unit for the shell hooks */
	if (iwm_reg_softc == NULL) {
		iwm_reg_softc = sc;
		iwm_adapter.ic = &sc->sc_ic;
		wlan_port_adapter_register(&iwm_adapter);
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* control and diagnostics */

int iwm_reg_up(void) {
	struct iwm_softc *sc = iwm_reg_softc;
	struct ifnet *ifp;

	if (sc == NULL || ISSET(sc->sc_flags, IWM_FLAG_STOPPED)) {
		printf("wlan: no attached iwm device\n");
		return -1;
	}
	ifp = IC2IFP(&sc->sc_ic);
	if (!(ifp->if_flags & IFF_RUNNING)) {
		ifp->if_flags |= IFF_UP;
		printf("wlan: calling if_init %p (fw load + power on)...\n",
		    (void *) ifp->if_init);
		wlan_port_serializer_lock();
		ifp->if_init(ifp);
		wlan_port_serializer_unlock();
		printf("wlan: if_init done, flags=%x\n", ifp->if_flags);
	}
	return 0;
}

static int iwm_reg_scan(const uint8_t *ssid, size_t len) {
	struct iwm_softc *sc = iwm_reg_softc;
	struct ieee80211com *ic;
	struct ifnet *ifp;
	int err;

	if (sc == NULL || len > IEEE80211_NWID_LEN ||
	    (len != 0 && ssid == NULL)) {
		return -1;
	}
	ic = &sc->sc_ic;
	ifp = IC2IFP(ic);
	if (!(ifp->if_flags & IFF_RUNNING)) {
		return -1;
	}

	ic->ic_roaming = IEEE80211_ROAMING_MANUAL;
	memcpy(ic->ic_des_essid, ssid, len);
	ic->ic_des_esslen = (uint8_t) len;

	wlan_port_serializer_lock();
	err = ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	wlan_port_serializer_unlock();
	return err != 0 ? -1 : 0;
}

static void iwm_reg_print_node_cb(void *arg, struct ieee80211_node *ni) {
	(void) arg;
	struct ieee80211_channel *ch = ni->ni_chan;

	printf("  %02x:%02x:%02x:%02x:%02x:%02x  ch=%d  rssi=%u  %s  ssid=%.*s\n",
	    ni->ni_bssid[0], ni->ni_bssid[1], ni->ni_bssid[2],
	    ni->ni_bssid[3], ni->ni_bssid[4], ni->ni_bssid[5],
	    ch != NULL ? ch->ic_freq : 0, ni->ni_rssi,
	    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ? "enc " : "open",
	    ni->ni_esslen, ni->ni_essid);
}

int wlan_port_xmit_iwm(const uint8_t *frame, size_t len) {
	struct iwm_softc *sc = iwm_reg_softc;
	struct ifnet *ifp;
	struct mbuf *m;
	int err;

	if (sc == NULL || frame == NULL || len < sizeof(struct ether_header) ||
	    len > MCLBYTES) {
		return -1;
	}
	ifp = IC2IFP(&sc->sc_ic);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		return -1;
	}
	m->m_len = m->m_pkthdr.len = (int) len;
	memcpy(mtod(m, void *), frame, len);

	/* The queue itself is an unprotected mbuf list. Hold the same
	 * serializer as the consumer before publishing a new packet. */
	wlan_port_serializer_lock();
	IFQ_ENQUEUE(&ifp->if_snd, m, err);
	if (err != 0) {
		wlan_port_serializer_unlock();
		m_freem(m);
		return -1;
	}
	if_start_lock(ifp);
	wlan_port_serializer_unlock();
	return (int) len;
}

int wlan_port_get_hwaddr_iwm(uint8_t addr[6]) {
	struct iwm_softc *sc = iwm_reg_softc;
	struct ifnet *ifp;

	if (sc == NULL) {
		return -1;
	}
	ifp = IC2IFP(&sc->sc_ic);
	if (ifp->if_sadl == NULL) {
		return -1;
	}
	memcpy(addr, CLLADDR(ifp->if_sadl), 6);
	return 0;
}

static void iwm_reg_status_dump(void) {
	struct iwm_softc *sc = iwm_reg_softc;
	struct ieee80211com *ic;
	struct ifnet *ifp;

	if (sc == NULL) {
		printf("wlan: no device\n");
		return;
	}
	ic = &sc->sc_ic;
	ifp = IC2IFP(ic);
	printf("iwm mac=%s flags=%x fw=%s hw_rev=%x\n",
	    ether_sprintf(sc->sc_nvm.hw_addr), sc->sc_flags, sc->sc_fwver,
	    sc->sc_hw_rev & IWM_CSR_HW_REV_TYPE_MSK);
	printf("iwm state=%s intmask=%x generation=%d wantresp=%d\n",
	    ic->ic_state >= 0 && ic->ic_state < IEEE80211_S_MAX ?
	        ieee80211_state_name[ic->ic_state] : "?",
	    sc->sc_intmask, sc->sc_generation, sc->sc_wantresp);
	if (ic->ic_bss != NULL) {
		printf("iwm bssid=%s ni_flags=%x rssi=%u\n",
		    ether_sprintf(ic->ic_bss->ni_bssid), ic->ic_bss->ni_flags,
		    ic->ic_bss->ni_rssi);
	}
	printf("iwm stats tx=%llu txerr=%llu rx=%llu rxerr=%llu\n",
	    (unsigned long long) ifp->if_data.if_opackets,
	    (unsigned long long) ifp->if_data.if_oerrors,
	    (unsigned long long) ifp->if_data.if_ipackets,
	    (unsigned long long) ifp->if_data.if_ierrors);
	printf("iwm rxq cur=%d cmdq queued=%d cur=%d qfullmsk=%x\n",
	    sc->rxq.cur, sc->txq[IWM_CMD_QUEUE].queued,
	    sc->txq[IWM_CMD_QUEUE].cur, sc->qfullmsk);
}

static void iwm_reg_scan_dump(void) {
	if (iwm_reg_softc != NULL) {
		ieee80211_iterate_nodes(&iwm_reg_softc->sc_ic.ic_scan,
		    iwm_reg_print_node_cb, NULL);
	}
}

void iwm_reg_detach(void *priv) {
	(void) priv;
	/* detach goes through iwm_stop with the stored device shell */
}

const struct wlan_chip_driver iwm_driver = {
	.name = "iwm",
	.bus = WLAN_BUS_PCIE,
	.usb_ids = NULL,
	.pcie_ids = iwm_pcie_ids,
	.attach = iwm_reg_attach_bus,
	.detach = iwm_reg_detach,
	.stop = NULL,
};
