/*
 * @file
 * @brief CherryUSB class hook for the chip drivers in the registry.
 *
 * During enumeration usbh_core matches VID/PID against the class info
 * below; on connect the hubport is wrapped into a wlan_usb_dev and the
 * matched chip driver attaches through the port interface.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <usbh_core.h>

#include <port/port.h>
#include <port/bus/usb/port_usb.h>
#ifdef WLAN_PORT_FREERTOS
#include <port/osal/freertos/wlan_port_freertos.h>
#else
#include <port/osal/embox/wlan_port_embox.h>
#endif
#include "wlan_port_cherryusb.h"

extern const struct wlan_chip_driver *const wlan_chip_drivers[];

/*
 * VID/PID pairs CherryUSB matches on, built from the driver registry by
 * usbh_wlan_class_init() before the host stack starts enumerating.  The
 * class hook is the only place a device becomes a chip driver, so this
 * is what keeps a new driver from having to touch the port: it declares
 * its ids in its own registry entry.
 */
#define WLAN_ID_TABLE_MAX 16
static uint16_t wlan_id_table[WLAN_ID_TABLE_MAX][2];

void usbh_wlan_class_init(void) {
	const struct wlan_chip_driver *drv;
	const struct wlan_usb_id *id;
	unsigned int n = 0;
	int i;

	for (i = 0; wlan_chip_drivers[i] != NULL && n + 1 < WLAN_ID_TABLE_MAX;
		i++) {
		drv = wlan_chip_drivers[i];
		if (drv->bus != WLAN_BUS_USB || drv->usb_ids == NULL) {
			continue;
		}
		/* the registry tables are terminated by vid == 0 */
		for (id = drv->usb_ids; id->vid != 0; id++) {
			if (n + 1 >= WLAN_ID_TABLE_MAX) {
				printf("wlan: id table full, %s ids truncated\n",
				    drv->name);
				break;
			}
			wlan_id_table[n][0] = id->vid;
			wlan_id_table[n][1] = id->pid;
			n++;
		}
	}
	wlan_id_table[n][0] = 0;
	wlan_id_table[n][1] = 0;
}

static const struct wlan_chip_driver *wlan_id_match(uint16_t vid,
	uint16_t pid) {
	const struct wlan_chip_driver *drv;
	const struct wlan_usb_id *id;
	int i;

	for (i = 0; wlan_chip_drivers[i] != NULL; i++) {
		drv = wlan_chip_drivers[i];
		if (drv->bus != WLAN_BUS_USB) {
			continue;
		}
		for (id = drv->usb_ids; id->vid != 0 || id->pid != 0; id++) {
			if (id->vid == vid && id->pid == pid) {
				return drv;
			}
		}
	}
	return NULL;
}

static int usbh_wlan_connect(struct usbh_hubport *hport, uint8_t intf) {
	struct usbh_interface *intf_desc = &hport->config.intf[intf];
	struct wlan_port_iface *pif;
	struct wlan_usb_dev *port_dev;
	const struct wlan_chip_driver *drv;
	int i;

	drv = wlan_id_match(hport->device_desc.idVendor,
	    hport->device_desc.idProduct);
	if (drv == NULL) {
		return -USB_ERR_INVAL;
	}
	if (wlan_port_if_n >= WLAN_PORT_MAX_IF) {
		printf("wlan: another adapter already claimed\n");
		return -USB_ERR_BUSY;
	}

	pif = malloc(sizeof(*pif));
	if (pif == NULL) {
		return -USB_ERR_NOMEM;
	}
	memset(pif, 0, sizeof(*pif));
	pif->drv = drv;

	port_dev = &pif->usb;
	port_dev->env_dev = hport;
	port_dev->ctrl_endp = &hport->ep0;
	port_dev->vendor = hport->device_desc.idVendor;
	port_dev->product = hport->device_desc.idProduct;
	port_dev->speed = (enum wlan_usb_speed) hport->speed;

	for (i = 0; i < intf_desc->altsetting[0].intf_desc.bNumEndpoints &&
		port_dev->bulk_endp_n < WLAN_PORT_MAX_BULK_EP; i++) {
		struct usb_endpoint_descriptor *ep =
			&intf_desc->altsetting[0].ep[i].ep_desc;

		if (USB_GET_ENDPOINT_TYPE(ep->bmAttributes) !=
			USB_ENDPOINT_TYPE_BULK) {
			continue;
		}
		port_dev->bulk_endp[port_dev->bulk_endp_n] = ep;
		port_dev->bulk_endp_addr[port_dev->bulk_endp_n] =
			(uint8_t) ep->bEndpointAddress;
		port_dev->bulk_endp_dir_in[port_dev->bulk_endp_n] =
			(ep->bEndpointAddress & 0x80U) != 0U;
		port_dev->bulk_endp_n++;
	}

	/*
	 * The chip drivers need the bulk data function; a composite device
	 * that carries one vendor-specific interface per function would
	 * otherwise be claimed on whichever interface came first.
	 */
	if (port_dev->bulk_endp_n < 2) {
		return -USB_ERR_INVAL;
	}

	wlan_port_ifs[wlan_port_if_n++] = pif;
	intf_desc->priv = pif; /* the instance the disconnect hook recovers */

	printf("wlan: %s found at bus %u addr %u (hub %u port %u)\n",
	    drv->name, hport->bus->busid, hport->dev_addr,
	    hport->parent != NULL ? hport->parent->index : 0, hport->port);

	return wlan_usbdi_attach(port_dev, drv);
}

static int usbh_wlan_disconnect(struct usbh_hubport *hport, uint8_t intf) {
	struct usbh_interface *intf_desc;
	struct wlan_port_iface *pif;

	(void) intf;
	intf_desc = &hport->config.intf[intf];
	pif = (struct wlan_port_iface *) intf_desc->priv;
	if (pif == NULL) {
		return 0;
	}
	intf_desc->priv = NULL;

	/* hot-unplug teardown is out of scope for this harness: the rx/tx
	 * workers are stopped but the claim (and the net80211 state
	 * behind it) is kept; a reboot recovers. */
	wlan_usbdi_detach(&pif->usb);
	printf("wlan: adapter removed (claim retained; reboot to recover)\n");
	return 0;
}

static const struct usbh_class_driver wlan_class_driver = {
	.driver_name = "wlan80211",
	.connect = usbh_wlan_connect,
	.disconnect = usbh_wlan_disconnect,
};

/*
 * Only the vendor-specific interface is a WiFi function: on a composite
 * dongle the mass-storage and Bluetooth functions sit in front of it,
 * and they must not be claimed here.
 */
CLASS_INFO_DEFINE const struct usbh_class_info wlan_class_info = {
	.match_flags = USB_CLASS_MATCH_VID_PID | USB_CLASS_MATCH_INTF_CLASS,
	.bInterfaceClass = 0xff,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.bInterfaceNumber = 0,
	.id_table = wlan_id_table,
	.class_driver = &wlan_class_driver,
};
