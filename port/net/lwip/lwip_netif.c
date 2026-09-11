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
static int wlan_lwip_rx_probe;
static volatile int wlan_lwip_assoc;

/* ---- rx hooks (USB worker context: copy and return) ---- */

static void wlan_lwip_data_rx(const uint8_t *frame, size_t len, void *arg) {
	struct pbuf *p;

	(void) arg;
	if (!wlan_lwip_assoc || len == 0) {
		return;
	}
	if (wlan_lwip_rx_probe && (frame[0] & 0x01) == 0) {
		printf("lwrx len=%u dst=%02x:%02x:%02x:%02x:%02x:%02x "
		    "netif=%02x:%02x:%02x:%02x:%02x:%02x type=%02x%02x\n",
		    (unsigned) len,
		    frame[0], frame[1], frame[2],
		    frame[3], frame[4], frame[5],
		    wlan_netif.hwaddr[0], wlan_netif.hwaddr[1],
		    wlan_netif.hwaddr[2], wlan_netif.hwaddr[3],
		    wlan_netif.hwaddr[4], wlan_netif.hwaddr[5],
		    frame[12], frame[13]);
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

int wlan_lwip_use_dhcp = 1;

static void wlan_lwip_tcpiplink(void *arg) {
	int assoc = (int) (uintptr_t) arg;

	if (assoc) {
		netif_set_link_up(&wlan_netif);
		/* DHCP overwrites a static address when its lease comes
		 * back; only run it when the user has not configured a
		 * static address */
		if (wlan_lwip_use_dhcp) {
			dhcp_start(&wlan_netif);
		}
	} else {
		dhcp_stop(&wlan_netif);
		netif_set_link_down(&wlan_netif);
	}
}

static void wlan_lwip_dhcp_stop_only(void *arg) {
	(void) arg;

	dhcp_stop(&wlan_netif);
}

void wlan_lwip_set_dhcp(int enable) {
	wlan_lwip_use_dhcp = enable;
	if (!enable) {
		tcpip_callback(wlan_lwip_dhcp_stop_only, NULL);
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

	if (wlan_port_get_hwaddr(hwaddr) == 0) {
		MEMCPY(netif->hwaddr, hwaddr, ETH_HWADDR_LEN);
	}
}

static err_t wlan_lwip_ifinit(struct netif *netif) {
	/* the adapter (and with it the MAC) attaches asynchronously; the
	 * address is refreshed by the ASSOC event before any frame goes
	 * out */
	netif->name[0] = 'w';
	netif->name[1] = 'l';
	netif->hwaddr_len = ETH_HWADDR_LEN;
	memset(netif->hwaddr, 0, ETH_HWADDR_LEN);
	netif->mtu = 1500;
	/* ETHARP is required: ethernet_input drops every IP/ARP frame
	 * whose netif lacks the flag */
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
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

/* supplicant lane: the port event hook belongs to the supplicant
 * driver wrapper, which reports association transitions here */
void wlan_lwip_assoc_notify(int assoc) {
	if (assoc) {
		wlan_lwip_assoc = 1;
		tcpip_callback(wlan_lwip_refresh_hwaddr, &wlan_netif);
		tcpip_callback(wlan_lwip_tcpiplink, (void *) (uintptr_t) 1);
	} else {
		wlan_lwip_assoc = 0;
		tcpip_callback(wlan_lwip_tcpiplink, (void *) (uintptr_t) 0);
	}
}

int wlan_lwip_ensure(void) {
	return 0; /* the netif is registered from boot */
}
