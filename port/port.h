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
 * Ports live in three categories under port/:
 *   osal/<os>/    the OS adaptation (locks, threads, timers, firmware
 *                 storage) behind the compat/netbsd/ declarations,
 *   bus/<bus>/    one directory per bus backend (usb/ with its
 *                 port_usb.h types, pcie/ and sd/ reserved), bringing
 *                 an attached device to the chip driver,
 *   net/<stack>/  the presentation layer (how the wlan interface
 *                 appears to the host stack: embox netdev + cfg80211,
 *                 lwip netif, ...).
 *
 * This header is the bus-agnostic contract: firmware lookup,
 * presentation hooks, the chip driver registry and the port lifecycle.
 * Bus-specific types sit next to their backend (port/bus/usb/port_usb.h
 * for USB) and are only visible as opaque structs here.
 *
 * The set of compiled-in chip drivers is discovered through
 * WLAN_CHIP_DRIVERS (a weak NULL-terminated array), so adding a driver
 * never touches a port.
 */

#ifndef NET80211_PORT_H_
#define NET80211_PORT_H_

#include <stdint.h>
#include <stddef.h>

/* USB bus types; define WLAN_BUS_USB backends include the full
 * definitions from port/bus/usb/port_usb.h. */
struct wlan_usb_dev;
struct wlan_usb_id;

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
 * Frame delivery (presentation hooks)
 *
 * Data frames that pass the net80211 input path leave the library
 * through these hooks. The presentation layer registers handlers for
 * the frames it wants; without a handler frames are dropped as before.
 * Both run in the USB worker context: the handlers must copy the frame
 * and return without blocking.
 */

/* EAPOL (ethertype 0x888e) frames; buf points at the payload behind
 * the 14-byte ethernet header, src is the ethernet source address. */
typedef void (*wlan_eapol_rx_fn)(const uint8_t src[6],
    const uint8_t *buf, size_t len, void *arg);

/* Every other delivered frame, as a full ethernet frame. */
typedef void (*wlan_data_rx_fn)(const uint8_t *frame, size_t len,
    void *arg);

void wlan_port_set_eapol_rx(wlan_eapol_rx_fn fn, void *arg);
void wlan_port_set_data_rx(wlan_data_rx_fn fn, void *arg);

enum wlan_port_event {
	WLAN_PORT_SCAN_DONE,
	WLAN_PORT_ASSOC,
	WLAN_PORT_DISASSOC,
};

/* Borrowed address, valid only during the callback. Consumers copy and
 * queue notifications; they must not re-enter the protocol state machine. */
typedef void (*wlan_event_fn)(enum wlan_port_event event,
    const uint8_t *addr, void *arg);
void wlan_port_set_event_handler(wlan_event_fn fn, void *arg);

/* Start one complete scan on the device worker, with no automatic join. */
int wlan_port_scan(const uint8_t *ssid, size_t len);

/* Send a full ethernet frame out of the wlan interface (queued to the
 * ifnet, encrypted/encapsulated by net80211). Returns len or -1. */
int wlan_port_xmit(const uint8_t *frame, size_t len);

/* The interface hardware address (after attach). */
int wlan_port_get_hwaddr(uint8_t addr[6]);

/* ------------------------------------------------------------------
 * Driver registry
 */

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
