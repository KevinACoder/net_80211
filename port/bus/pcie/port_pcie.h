/*
 * @file
 * @brief PCIe bus section of the port interface.
 *
 * Mirrors the pci_attach_args world the imported BSD drivers use: one
 * MMIO region behind bus_space, DMA-able memory behind bus_dma, and a
 * message-signalled interrupt. A bus backend implements the claim and
 * interrupt plumbing; a chip driver is matched and attached against a
 * wlan_pcie_dev claimed by the backend.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#ifndef NET80211_PORT_PCIE_H_
#define NET80211_PORT_PCIE_H_

#include <stdint.h>
#include <stddef.h>

/* A claimed PCIe device. The backend fills the coordinates and keeps
 * its own handles behind port_priv; the NetBSD-shim world only sees
 * opaque pointers. */
struct wlan_pcie_dev {
	void *port_priv; /* backend-owned object */

	/* environment device handle (backend specific) */
	void *env_dev;

	uint16_t vendor;
	uint16_t device;
	uint8_t rev;
	uint8_t bus_n;
	uint8_t slot;
	uint8_t func;

	/* BAR 0 as MMIO register window */
	uint64_t bar_pa;
	size_t bar_size;
	void *bar_va; /* mapped register base (identity on this platform) */

	/* allocated message interrupt, ready for irq_attach */
	int irq;
};

/* PCI match table entry, terminated by vendor==0 && device==0. */
struct wlan_pcie_id {
	uint16_t vendor;
	uint16_t device;
};

#endif /* NET80211_PORT_PCIE_H_ */
