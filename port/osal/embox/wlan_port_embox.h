/*
 * @file
 * @brief Internal glue between the embox adapter and the BSD shim.
 */

#ifndef WLAN_PORT_EMBOX_H_
#define WLAN_PORT_EMBOX_H_

struct ifnet;
#include <port/port.h>
#include <port/bus/usb/port_usb.h>

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

/* Port serializer: replaces the splnet() discipline of the imported
 * PCI drivers (spl is a no-op on this port). Driver adapter entries
 * take it; tsleep drops it around the wait. */
void wlan_port_serializer_lock(void);
void wlan_port_serializer_unlock(void);
void *wlan_port_serializer_owner(void);

/* Release the lock around a sleep and restore the saved hold count
 * (0 when the caller was not holding it). */
int wlan_port_serializer_suspend(void);
void wlan_port_serializer_resume(int depth);

#endif /* WLAN_PORT_EMBOX_H_ */
