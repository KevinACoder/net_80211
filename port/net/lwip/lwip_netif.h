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

#endif /* LWIP_NETIF_H_ */
