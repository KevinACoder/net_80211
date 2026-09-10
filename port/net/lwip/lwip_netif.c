/*
 * @file
 * @brief lwIP presentation of the net_80211 port hooks.
 *
 * Maps the port hooks in port/port.h onto an lwIP netif: data frames
 * go into pbufs handed to tcpip_input, the linkoutput feeds
 * wlan_port_xmit, and the scan/assoc events drive the link state and
 * DHCP. EAPOL is counted but dropped until a supplicant exists.
 *
 * Call wlan_lwip_init() once after tcpip_init(); it registers the rx
 * hooks, adds the netif and starts a DHCP bind when the adapter
 * associates.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <string.h>
#include <stdio.h>

#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/sys.h"

#include <port/port.h>

static struct netif wlan_netif;
static volatile int wlan_lwip_assoc;

/* ---- rx hooks (USB worker context: copy and return) ---- */

static void wlan_lwip_data_rx(const uint8_t *frame, size_t len, void *arg) {
	struct pbuf *p;

	(void) arg;
	if (!wlan_lwip_assoc || len == 0) {
		return;
	}
	p = pbuf_alloc(PBUF_RAW, (u16_t) len, PBUF_RAM);
	if (p == NULL) {
		return;
	}
	if (pbuf_take(p, frame, (u16_t) len) != ERR_OK) {
		pbuf_free(p);
		return;
	}
	if (wlan_netif.input(p, &wlan_netif) != ERR_OK) {
		pbuf_free(p);
	}
}

static void wlan_lwip_eapol_rx(const uint8_t src[6],
	const uint8_t *buf, size_t len, void *arg) {
	(void) src;
	(void) buf;
	(void) len;
	(void) arg;
	/* no supplicant on this port yet */
}

/* ---- events run in the port worker context; defer the lwIP calls
 * into the tcpip thread ---- */

static void wlan_lwip_tcpiplink(void *arg) {
	int assoc = (int) (uintptr_t) arg;

	if (assoc) {
		netif_set_link_up(&wlan_netif);
		dhcp_start(&wlan_netif);
	} else {
		dhcp_stop(&wlan_netif);
		netif_set_link_down(&wlan_netif);
	}
}

static void wlan_lwip_refresh_hwaddr(struct netif *netif);

static void wlan_lwip_event(enum wlan_port_event event,
	const uint8_t *addr, void *arg) {
	(void) addr;
	(void) arg;

	switch (event) {
	case WLAN_PORT_ASSOC:
		wlan_lwip_assoc = 1;
		tcpip_callback(wlan_lwip_refresh_hwaddr, &wlan_netif);
		tcpip_callback(wlan_lwip_tcpiplink, (void *) (uintptr_t) 1);
		break;
	case WLAN_PORT_DISASSOC:
		wlan_lwip_assoc = 0;
		tcpip_callback(wlan_lwip_tcpiplink, (void *) (uintptr_t) 0);
		break;
	default:
		break;
	}
}

/* ---- tx (tcpip thread context) ---- */

static err_t wlan_lwip_linkoutput(struct netif *netif, struct pbuf *p) {
	static uint8_t frame[1544];
	size_t len;

	(void) netif;
	if (p->tot_len > sizeof(frame)) {
		return ERR_BUF;
	}
	len = pbuf_copy_partial(p, frame, p->tot_len, 0);
	if (len == 0) {
		return ERR_BUF;
	}

	return (wlan_port_xmit(frame, len) >= 0) ? ERR_OK : ERR_IF;
}

/* pull the MAC once the adapter is attached (tcpip thread context) */
static void wlan_lwip_refresh_hwaddr(struct netif *netif) {
	uint8_t hwaddr[6];

	if (netif->hwaddr[0] == 0 && netif->hwaddr[1] == 0 &&
	    netif->hwaddr[2] == 0 && wlan_port_get_hwaddr(hwaddr) == 0) {
		MEMCPY(netif->hwaddr, hwaddr, ETH_HWADDR_LEN);
	}
}

static err_t wlan_lwip_ifinit(struct netif *netif) {
	/* the adapter (and with it the MAC) attaches asynchronously; the
	 * address is refreshed by the event handler */
	uint8_t hwaddr[6];

	(void) wlan_port_get_hwaddr(hwaddr);
	netif->name[0] = 'w';
	netif->name[1] = 'l';
	netif->hwaddr_len = ETH_HWADDR_LEN;
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_BROADCAST;

	/* etharp_output needs the hwaddr set before use */
	MEMCPY(netif->hwaddr, hwaddr, ETH_HWADDR_LEN);
	netif->output = etharp_output;
	netif->linkoutput = wlan_lwip_linkoutput;

	return ERR_OK;
}

int wlan_lwip_init(void) {
	ip4_addr_t ip, netmask, gw;

	wlan_port_set_data_rx(wlan_lwip_data_rx, NULL);
	wlan_port_set_eapol_rx(wlan_lwip_eapol_rx, NULL);
	wlan_port_set_event_handler(wlan_lwip_event, NULL);

	ip4_addr_set_zero(&ip);
	ip4_addr_set_zero(&netmask);
	ip4_addr_set_zero(&gw);
	if (netif_add(&wlan_netif, &ip, &netmask, &gw, NULL,
	    wlan_lwip_ifinit, tcpip_input) == NULL) {
		return -1;
	}
	netif_set_up(&wlan_netif);

	return 0;
}

struct netif *wlan_lwip_get_netif(void) {
	return &wlan_netif;
}
