/*
 * @file
 * @brief Internal glue between the embox adapter and the BSD shim.
 */

#ifndef WLAN_PORT_EMBOX_H_
#define WLAN_PORT_EMBOX_H_

struct ifnet;
#include <port/port.h>

#define WLAN_PORT_MAX_IF 2
#define WLAN_PORT_MAX_BULK_EP 4

struct wlan_port_iface {
	struct wlan_usb_dev usb;
	const struct wlan_chip_driver *drv;
	struct ifnet *if_shell; /* BSD ifnet shell, owned by the shim */
	void *shim_priv; /* usbd_device / softc world */
	int attached;
};

extern struct wlan_port_iface *wlan_port_ifs[WLAN_PORT_MAX_IF];
extern int wlan_port_if_n;

#endif /* WLAN_PORT_EMBOX_H_ */
