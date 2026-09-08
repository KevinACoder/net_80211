/*
 * @file
 * @brief The stable port interface of the net_80211 library.
 *
 * A port adapts the imported NetBSD net80211 stack and chip drivers to
 * one operating environment. The library never includes an OS header
 * directly: the NetBSD kernel API surface the imported code expects is
 * declared by the shadow headers in compat/netbsd/ and implemented by
 * the port, while the pieces that differ structurally between OSes go
 * through this header.
 *
 * A port provides:
 *   1. implementations for the compat/netbsd/ declarations (memory,
 *      locks, mbuf, ifnet shell, usbd_* and friends),
 *   2. the bus abstraction below (USB host),
 *   3. the firmware lookup,
 *   4. the presentation layer (how the wlan interface appears to the
 *      host stack: embox netdev + cfg80211, lwip netif, ...),
 *   5. one port_wlan_init() call from the environment.
 *
 * The set of compiled-in chip drivers is discovered through
 * WLAN_CHIP_DRIVERS (a weak NULL-terminated array), so adding a driver
 * never touches a port.
 */

#ifndef NET80211_PORT_H_
#define NET80211_PORT_H_

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------
 * Bus: USB host
 *
 * Mirrors the UsbDevice/UsbPipe operations the imported BSD drivers
 * use through usbd_*: synchronous vendor control transfers for the
 * register file and the firmware download, and exclusive bulk pipes
 * for the frame paths.
 */

enum wlan_usb_speed {
	WLAN_USB_SPEED_LOW,
	WLAN_USB_SPEED_FULL,
	WLAN_USB_SPEED_HIGH,
	WLAN_USB_SPEED_SUPER,
};

/* A claimed USB device. The embox adapter keeps the endpoint handles
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

/* ------------------------------------------------------------------
 * Firmware
 *
 * Ports resolve a driver firmware name to bytes; embedding or loading
 * from a file system is up to the port. The blob is not freed.
 */

struct wlan_firmware {
	const uint8_t *data;
	size_t size;
};

int wlan_port_firmware_get(const char *name, struct wlan_firmware *fw);

/* ------------------------------------------------------------------
 * Net attachment (presentation hooks)
 *
 * net80211 runs on a BSD ifnet shell that the port owns. The port
 * decides how that shell maps onto its own stack.
 */

struct wlan_port_ifops {
	/* The interface went up/down (net80211 INIT <-> RUN transitions). */
	int (*up)(void *if_priv);
	int (*down)(void *if_priv);
};

/* ------------------------------------------------------------------
 * Driver registry
 */

struct wlan_usb_id {
	uint16_t vid;
	uint16_t pid;
};

struct wlan_chip_driver {
	const char *name;
	enum { WLAN_BUS_USB = 1 } bus;
	/* USB match table, terminated by vid==0 && pid==0. */
	const struct wlan_usb_id *usb_ids;

	/* Attach the device: bring the chip up, load the firmware,
	 * ieee80211_ifattach. if_priv is the port-owned ifnet shell.
	 * Returns 0 on success; on failure the port closes the device. */
	int (*attach)(struct wlan_usb_dev *usb, void *if_priv);
	void (*detach)(void *if_priv);
	void (*stop)(void *if_priv);
};

/* Each driver translation unit exports one global
 * `const struct wlan_chip_driver <name>_driver`. The port defines its
 * own NULL-terminated `wlan_chip_drivers[]` listing the drivers it was
 * built with - explicit, so ports and drivers stay decoupled without
 * relying on linker sections or constructors. */

/* ------------------------------------------------------------------
 * Port lifecycle
 *
 * Called once by the environment before any use. The port scans the
 * registry, claims matching devices and presents one net interface
 * per attached chip (naming per presentation layer).
 */

/* wlan_port_init()/wlan_port_deinit() are provided per port; the
 * embox port wires wlan_port_init as its unit init. */
void wlan_port_deinit(void);

#endif /* NET80211_PORT_H_ */
