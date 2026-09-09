/*
 * @file
 * @brief Internal glue between the cherryusb adapter and the BSD shim.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#ifndef WLAN_PORT_CHERRYUSB_H_
#define WLAN_PORT_CHERRYUSB_H_

/* usb2host1: panel USB2.0 Type-A (on-board CH334P hub), SPI 133 -> IRQ 165 */
#define WLAN_CHERRYUSB_EHCI_BASE 0xFD880000UL
#define WLAN_CHERRYUSB_EHCI_BUSID 0

struct wlan_chip_driver;
struct wlan_usb_dev;

int wlan_usbdi_attach(struct wlan_usb_dev *port,
	const struct wlan_chip_driver *drv);
void wlan_usbdi_detach(struct wlan_usb_dev *port);

#endif /* WLAN_PORT_CHERRYUSB_H_ */
