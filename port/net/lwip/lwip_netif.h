/*
 * @file
 * @brief lwIP presentation of the net_80211 port hooks.
 */

#ifndef LWIP_NETIF_H_
#define LWIP_NETIF_H_

#include "lwip/netif.h"

/* Register the rx hooks and add the wlan netif (call once after
 * tcpip_init()). Returns 0 on success. */
int wlan_lwip_init(void);

/* The wlan netif (registered even when the adapter is absent). */
struct netif *wlan_lwip_get_netif(void);

/* For the supplicant lane: the supplicant owns the port event hook,
 * so it reports association state here. Drives the MAC refresh, the
 * link state and DHCP on the tcpip thread. */
void wlan_lwip_assoc_notify(int assoc);

/* No-op on this port (the netif exists from boot); refreshes the MAC
 * like the embox netdev bridge did. Returns 0 when the netif exists. */
int wlan_lwip_ensure(void);

#endif /* LWIP_NETIF_H_ */

/* disable the DHCP started on ASSOC so a static address sticks */
void wlan_lwip_set_dhcp(int enable);
