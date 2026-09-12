/*
 * @file
 * @brief USB bus section of the port interface.
 *
 * Mirrors the UsbDevice/UsbPipe operations the imported BSD drivers
 * use through usbd_*: synchronous vendor control transfers for the
 * register file and the firmware download, and exclusive bulk pipes
 * for the frame paths. A bus backend (cherryusb today) implements the
 * bus_ops; a chip driver is matched and attached against a
 * wlan_usb_dev claimed by the backend.
 *
 * @date 09.09.2026
 * @author zhugengyu
 */

#ifndef NET80211_PORT_USB_H_
#define NET80211_PORT_USB_H_

#include <stdint.h>

/* The USB 2.0 speed encoding the topology reports (and the one NetBSD's
 * USB_SPEED_* uses for low/full/high), not an enumeration of its own. */
enum wlan_usb_speed {
	WLAN_USB_SPEED_LOW = 1,
	WLAN_USB_SPEED_FULL = 2,
	WLAN_USB_SPEED_HIGH = 3,
	WLAN_USB_SPEED_SUPER = 5,
};

/* A claimed USB device. The port adapter keeps the endpoint handles
 * here; the NetBSD-shim world only sees opaque pointers. */
struct wlan_usb_dev {
	void *port_priv; /* port-owned object (usbd_device shell) */

	/* environment device handle and its endpoints */
	void *env_dev;
	void *ctrl_endp;
	void *bulk_endp[4];
	int bulk_endp_n;
	uint8_t bulk_endp_addr[4];
	uint8_t bulk_endp_dir_in[4];

	enum wlan_usb_speed speed;
	uint16_t vendor;
	uint16_t product;
};

struct wlan_usb_bus_ops {
	/* Open the device found in the attached topology and claim the
	 * default control pipe. Called once per match. */
	int (*open)(struct wlan_usb_dev *dev);
	void (*close)(struct wlan_usb_dev *dev);

	/* Synchronous control transfer on the default pipe. */
	int (*ctrl_xfer)(struct wlan_usb_dev *dev, uint8_t req_type,
	    uint8_t request, uint16_t value, uint16_t index,
	    uint16_t len, void *buf, int timeout_ms);

	/* Open/close a bulk pipe by endpoint number. */
	int (*bulk_open)(struct wlan_usb_dev *dev, uint8_t endp, int dir_in);
	void (*bulk_close)(struct wlan_usb_dev *dev, uint8_t endp);

	/* Synchronous bulk transfer; returns the transferred length or a
	 * negative errno. dir selects the pipe opened before. */
	int (*bulk_xfer)(struct wlan_usb_dev *dev, uint8_t endp, int dir_in,
	    void *buf, uint16_t len, int timeout_ms);

	/* Clear a halted bulk endpoint. */
	int (*bulk_clear_halt)(struct wlan_usb_dev *dev, uint8_t endp,
	    int dir_in);
};

/* USB match table entry, terminated by vid==0 && pid==0. */
struct wlan_usb_id {
	uint16_t vid;
	uint16_t pid;
};

#endif /* NET80211_PORT_USB_H_ */
