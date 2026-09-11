/*
 * @file
 * @brief RK3568 EHCI glue for the CherryUSB host stack (FreeRTOS port).
 *
 * Covers the usb2host1 controller (0xFD880000, panel USB2.0 Type-A behind
 * an on-board CH334P hub, GIC SPI 133 -> INTID 165) and the board domain
 * bring-up it depends on: PIPE power domain ensure-on, usb2phy reference
 * clock gates, USB2 host hclk/arb/utmi reset cycle, the VBUS enable
 * switches (the 5V rails provide no power until GPIO3_A0/A1 are driven)
 * and the usb2phy1 suspend overrides / 480 MHz UTMI clock output.
 *
 * Register knowledge is shared with the embox glue in this directory;
 * the OS plumbing differs: the hard IRQ only gives a semaphore and the
 * whole CherryUSB interrupt processing runs in a bottom-half task, so
 * every shim/driver entry stays in task context. All touched MMIO lies
 * in the flat device map (0xFD000000..4G).
 *
 * The post-init hook handles devices that are already plugged when the
 * controller comes out of reset: no connect-change edge is generated for
 * them, so a powered port is force-toggled once and the roothub change
 * bitmap is seeded before the hub thread ever looks at it.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdint.h>
#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "ftypes.h"
#include "fcache.h"
#include "intr.h"

#include <usbh_core.h>
#include <usb_osal.h>
#include <usb_hc_ehci.h>

#include <sys/systm.h> /* delay() */

/* ------------------------------------------------------------------ */
/* board constants */

#define CRU_BASE         0xFDD20000UL
#define PMUCRU_BASE      0xFDD00000UL
#define PMU_BASE         0xFDD90000UL
#define USB2PHY_GRF_BASE 0xFDCA8000UL
#define VBUS_GPIO_BASE   0xFE760000UL

/* PMU: PIPE power domain shared with the high speed phys */
#define PMU_BUS_IDLE_SFTCON0 0x050
#define PMU_BUS_IDLE_ACK     0x060
#define PMU_PWR_DWN_ST       0x098
#define PMU_PWR_GATE_SFTCON  0x0a0
#define PMU_BIU_PIPE         (1 << 11)
#define PMU_PD_PIPE          (1 << 8)

/* the USB2 hosts have no clock gates of their own (hclk open at reset) */
#define CRU_SOFTRST_CON(n)        (0x400 + 4 * (n))
#define SOFTRST_CON14_USB2HOST    0x3f0 /* bits 4-9: h/arb/utmi, host0+host1 */
#define SOFTRST_CON28_USB2PHY_GRF (1 << 11)
#define SOFTRST_CON29_USB2PHY1    0x38 /* bits 3-5: phy POR + port resets */

#define PMUCRU_CLKGATE_CON2 0x188
#define CLKGATE_CON2_USBPHY 0x7 /* clk_ref24m, xin_osc0_usbphy0/1_g */

/* GPIO v2 bank: data / data-direction of the low 16 pins */
#define GPIO_SWPORT_DR_L  0x00
#define GPIO_SWPORT_DDR_L 0x08
#define VBUS_PINS         0x3 /* A0: panel USB2.0, A1: USB3.0 dock */

/* usb2phy GRF, one instance per phy */
#define USB2PHY_GRF_OTG_CON0    0x000
#define USB2PHY_GRF_HOST_CON1   0x004
#define USB2PHY_GRF_CLKOUT_CON2 0x008
#define USB2PHY_OTG_HOST_VAL    0x0c00
#define USB2PHY_OTG_HOST_MASK   0xfff
#define USB2PHY_HOST_VAL        0x1d2
#define USB2PHY_HOST_MASK       0x1ff
#define USB2PHY_CLKOUT_480M_DIS (1 << 4)

/* busid -> (controller, irq); this port drives usb2host1 only */
static const struct {
	uintptr_t base;
	unsigned int irq;
} rk3568_ehci_bus[CONFIG_USBHOST_MAX_BUS] = {
	{ 0xFD880000UL, 165 }, /* dts SPI 133 -> INTID 165, level-high */
};

/* ------------------------------------------------------------------ */
/* MMIO helpers (device memory in the flat map) */

static inline uint32_t rd32(unsigned long addr) {
	return *(volatile uint32_t *) addr;
}

static inline void wr32(unsigned long addr, uint32_t val) {
	*(volatile uint32_t *) addr = val;
}

/* ------------------------------------------------------------------ */
/* domain bring-up (Rockchip hiword write-enable protocol) */

static void hiword_write(unsigned long base, unsigned int off, uint32_t mask,
	uint32_t val) {
	wr32(base + off, ((mask & 0xffff) << 16) | (val & mask));
}

static int wait_cleared(unsigned long base, unsigned int off, uint32_t mask) {
	int n;

	for (n = 0; n < 100; n++) {
		if ((rd32(base + off) & mask) == 0) {
			return 0;
		}
		delay(100);
	}
	return -1;
}

static void usb_pipe_domain_power_on(void) {
	hiword_write(PMU_BASE, PMU_BUS_IDLE_SFTCON0, PMU_BIU_PIPE, 0);
	hiword_write(PMU_BASE, PMU_PWR_GATE_SFTCON, PMU_PD_PIPE, 0);

	if (wait_cleared(PMU_BASE, PMU_BUS_IDLE_ACK, PMU_BIU_PIPE) != 0 ||
		wait_cleared(PMU_BASE, PMU_PWR_DWN_ST, PMU_PD_PIPE) != 0) {
		printf("usb_glue: PIPE power domain did not settle, continuing\n");
	}
}

static int s_domain_done;

static void rk3568_usb2host_domain_init(void) {
	if (s_domain_done) {
		return;
	}
	s_domain_done = 1;

	usb_pipe_domain_power_on();

	hiword_write(PMUCRU_BASE, PMUCRU_CLKGATE_CON2, CLKGATE_CON2_USBPHY, 0);

	hiword_write(CRU_BASE, CRU_SOFTRST_CON(14), SOFTRST_CON14_USB2HOST,
		SOFTRST_CON14_USB2HOST);
	delay(100);
	hiword_write(CRU_BASE, CRU_SOFTRST_CON(14), SOFTRST_CON14_USB2HOST, 0);
	delay(100);

	hiword_write(CRU_BASE, CRU_SOFTRST_CON(29), SOFTRST_CON29_USB2PHY1, 0);
	hiword_write(CRU_BASE, CRU_SOFTRST_CON(28), SOFTRST_CON28_USB2PHY_GRF, 0);

	hiword_write(VBUS_GPIO_BASE, GPIO_SWPORT_DDR_L, VBUS_PINS, VBUS_PINS);
	hiword_write(VBUS_GPIO_BASE, GPIO_SWPORT_DR_L, VBUS_PINS, VBUS_PINS);

	hiword_write(USB2PHY_GRF_BASE, USB2PHY_GRF_OTG_CON0,
		USB2PHY_OTG_HOST_MASK, USB2PHY_OTG_HOST_VAL);
	delay(2 * 1000);
	hiword_write(USB2PHY_GRF_BASE, USB2PHY_GRF_HOST_CON1,
		USB2PHY_HOST_MASK, USB2PHY_HOST_VAL);
	delay(2 * 1000);
	hiword_write(USB2PHY_GRF_BASE, USB2PHY_GRF_CLKOUT_CON2,
		USB2PHY_CLKOUT_480M_DIS, 0);
	delay(100);
}

/* ------------------------------------------------------------------ */
/* cherryusb hooks */

/* CherryUSB EHCI processes the interrupt inline in the ISR (t113 /
 * lab-SDK style); the shim ring is guarded by the ipl shim, which maps
 * to the port's ISR-grade interrupt mask. */
static void rk3568_ehci_irq_handler(s32 vector, void *param) {
	(void) vector;
	USBH_IRQHandler((uint8_t) (uintptr_t) param);
}

void usb_hc_low_level_init(struct usbh_bus *bus) {
	int slot = bus->busid;

	rk3568_usb2host_domain_init();

	(void) slot;
	/* FromISR APIs require a priority at or below (numerically >=)
	 * configMAX_API_CALL_INTERRUPT_PRIORITY; the GIC driver defaults
	 * SPIs to 0xa0 which is above the line */
	InterruptSetPriority((int) rk3568_ehci_bus[bus->busid].irq,
	    configMAX_API_CALL_INTERRUPT_PRIORITY);
	InterruptInstall((int) rk3568_ehci_bus[bus->busid].irq,
	    rk3568_ehci_irq_handler, (void *) (uintptr_t) slot,
	    "rk3568_ehci");
	InterruptUmask(rk3568_ehci_bus[bus->busid].irq);
}

/* Pre-inserted devices: force one port power toggle so the controller
 * latches a connect-change edge, then seed the roothub bitmap and wake
 * the hub thread (it is waiting on its mailbox at this point). */
void usb_hc_ehci_post_init(struct usbh_bus *bus) {
	struct ehci_hcd *hcd = &g_ehci_hcd[bus->hcd.hcd_id];
	uint8_t port;

	bus->hcd.roothub.nports = hcd->n_ports;

	for (port = 1; port <= hcd->n_ports; port++) {
		uint32_t regval = EHCI_HCOR->portsc[port - 1];

		if (hcd->ppc && (regval & EHCI_PORTSC_CCS)) {
			regval &= ~EHCI_PORTSC_PP;
			EHCI_HCOR->portsc[port - 1] = regval;
			usb_osal_msleep(30);
			regval |= EHCI_PORTSC_PP;
			EHCI_HCOR->portsc[port - 1] = regval;
			usb_osal_msleep(30);
		}

		bus->hcd.roothub.int_buffer[port / 8] |= (1 << (port % 8));
	}

	usbh_hub_thread_wakeup(&bus->hcd.roothub);
}

uint8_t usbh_get_port_speed(struct usbh_bus *bus, const uint8_t port) {
	uint32_t regval = EHCI_HCOR->portsc[port - 1];

	(void) bus;
	if ((regval & EHCI_PORTSC_LSTATUS_MASK) == EHCI_PORTSC_LSTATUS_KSTATE) {
		return USB_SPEED_LOW;
	}
	if (regval & EHCI_PORTSC_PE) {
		return USB_SPEED_HIGH;
	}
	return USB_SPEED_FULL;
}

/* ------------------------------------------------------------------ */
/* dcache maintenance (CONFIG_USB_DCACHE_ENABLE) */

void usb_dcache_clean(uintptr_t addr, size_t size) {
	/* no clean-only op on this port: writeback+invalidate also
	 * guarantees the controller sees the fresh data */
	FCacheDCacheFlushRange((intptr) addr, size);
}

void usb_dcache_invalidate(uintptr_t addr, size_t size) {
	/* pure invalidate: a writeback here would push CPU dirty lines over
	 * data the DMA just wrote */
	FCacheDCacheInvalidateRange((intptr) addr, size);
}

void usb_dcache_flush(uintptr_t addr, size_t size) {
	FCacheDCacheFlushRange((intptr) addr, size);
}
