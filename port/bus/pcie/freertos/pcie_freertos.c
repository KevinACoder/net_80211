/*
 * @file
 * @brief PCIe bus backend of the FreeRTOS test system (DWC root port).
 *
 * Implements the compat PCI autoconf and bus_dma surface the imported
 * iwm driver calls over the pcie_dw core (NetBSD rk_pcie derived):
 * it claims the device behind the pcie3x2 root port, maps BAR0
 * (identity platform), arms the INTx aggregation line and hands the
 * device to the matched chip driver.
 *
 * PCIe-specifics: the link is trained by U-Boot's preboot `pci enum`
 * (cold autonomous training stalls in Polling on this board) and kept
 * as the firmware left it, and the Rockchip DWC host has no real ECAM
 * - config access to the endpoint runs through an outbound CFG0 iATU
 * region kept targeted at the endpoint's bus, and one inbound iATU
 * region maps host RAM plus the MSI doorbell window so the endpoint's
 * posted writes actually land (the DWC RC programs only outbound
 * regions by default, which silently drops them).
 *
 * @date 11.09.2026
 * @author zhugengyu
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "ftypes.h"
#include "fcache.h"
#include "intr.h"
#include "pcie_dw.h"

#include <sys/bus.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/workqueue.h>
#include <dev/pci/pcivar.h>

#include <port/port.h>
#include <port/osal/freertos/wlan_port_freertos.h>
#include <port/bus/pcie/port_pcie.h>

/* the chip driver this backend serves (explicit, like the USB
 * backend's registry) */
extern const struct wlan_chip_driver iwm_driver;

static const struct wlan_chip_driver *const wlan_pcie_drivers[] = {
	&iwm_driver,
	NULL
};

/* pcie3x2: the M.2/xc4 slot link where the 7260 sits. The link is
 * trained by U-Boot's preboot `pci enum` and kept as-is here; the
 * windows, DBI and client APB are flat-mapped device memory (board
 * MMU table). */
static const struct pcie_dw_controller wlan_pcie_ctlr = {
	.apb_base	= 0xFE280000ULL,	/* client APB (0xFE840000 is
						 * the combphy2 PHY, NOT
						 * the PCIe client) */
	.dbi_base	= 0x3C0800000ULL,	/* root port DBI */
	.cfg_base	= 0xF0000000ULL,	/* config aperture (one bus) */
	.cfg_size	= 0x100000ULL,
	.mem_base	= 0xF0200000ULL,	/* outbound MEM window */
	.mem_size	= 0x1E00000ULL,
	/* inbound window for endpoint DMA to/from host RAM (identity
	 * mapped): without it the card's firmware-chunk and descriptor
	 * DMA reads find no root-port address match and silently time
	 * out. One region must also cover the MSI doorbell page so the
	 * endpoint's posted writes are not silently dropped. */
	.dma_base	= 0x0A000000ULL,
	.dma_size	= 0x01000000ULL,
	.doorbell_base	= 0xFD450000ULL,	/* GITS_TRANSLATER page */
	.doorbell_size	= 0x1000ULL,
	.cfg_atu_idx	= 1,
	.mem_atu_idx	= 0,
};

static struct pcie_dw_softc wlan_pcie_softc;

bus_dma_tag_t wlan_bus_dma_tag;

void wlan_bus_space_barrier(bus_space_tag_t t, bus_space_handle_t h,
	bus_size_t o, bus_size_t len, int flags) {
	(void) t; (void) h; (void) o; (void) len;
	if (flags & BUS_SPACE_BARRIER_READ) {
		__asm volatile("dsb sy");
	} else {
		__asm volatile("dsb st");
	}
}

/* ------------------------------------------------------------------ */
/* bus_dma implementation (declared in compat sys/bus.h) */

int bus_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments,
	bus_size_t maxsegsz, bus_size_t boundary, int flags,
	bus_dmamap_t *dmamp) {
	(void) tag; (void) maxsegsz; (void) boundary; (void) flags;
	(void) nsegments;

	*dmamp = wlan_kmalloc(sizeof(**dmamp), M_ZERO, M_DEVBUF);
	if (*dmamp == NULL) {
		return ENOMEM;
	}
	(*dmamp)->dm_mapsize = size;
	return 0;
}

void bus_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t map) {
	(void) tag;
	wlan_kfree(map, M_DEVBUF);
}

int bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size,
	bus_size_t alignment, bus_size_t boundary,
	struct bus_dma_segment *segs, int nsegs, int *rsegs, int flags) {
	(void) tag; (void) boundary; (void) nsegs; (void) flags;

	if (alignment < 64) {
		alignment = 64;
	}
	size = (size + 63) & ~(bus_size_t) 63;

	void *raw = wlan_kmalloc(size + alignment, 0, M_DEVBUF);
	if (raw == NULL) {
		return ENOMEM;
	}
	uintptr_t va = ((uintptr_t) raw + alignment) & ~(alignment - 1);
	((void **) va)[-1] = raw;
	segs[0].ds_addr = va;
	segs[0].ds_len = size;
	*rsegs = 1;
	return 0;
}

void bus_dmamem_free(bus_dma_tag_t tag, struct bus_dma_segment *segs,
	int nsegs) {
	(void) tag; (void) nsegs;

	if (segs[0].ds_addr != 0) {
		wlan_kfree(((void **) segs[0].ds_addr)[-1], M_DEVBUF);
		segs[0].ds_addr = 0;
	}
}

int bus_dmamem_map(bus_dma_tag_t tag, struct bus_dma_segment *segs,
	int nsegs, bus_size_t size, void **kvap, int flags) {
	(void) tag; (void) nsegs; (void) size; (void) flags;

	*kvap = (void *) segs[0].ds_addr;
	return 0;
}

void bus_dmamem_unmap(bus_dma_tag_t tag, void *kva, bus_size_t size) {
	(void) tag; (void) kva; (void) size;
}

int bus_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t map, void *va,
	bus_size_t size, void *ctx, int flags) {
	(void) tag; (void) ctx; (void) flags;

	map->dm_segs[0].ds_addr = (bus_addr_t) va;
	map->dm_segs[0].ds_len = size;
	map->dm_nsegs = 1;
	if (size > map->dm_mapsize) {
		map->dm_mapsize = size;
	}
	return 0;
}

int bus_dmamap_load_mbuf(bus_dma_tag_t tag, bus_dmamap_t map,
	struct mbuf *m, int flags) {
	(void) tag; (void) flags;

	map->dm_segs[0].ds_addr = (bus_addr_t) m->m_data;
	map->dm_segs[0].ds_len = (m->m_flags & M_PKTHDR) ?
	    m->m_pkthdr.len : m->m_len;
	map->dm_nsegs = 1;
	return 0;
}

void bus_dmamap_unload(bus_dma_tag_t tag, bus_dmamap_t map) {
	(void) tag;
	map->dm_nsegs = 0;
	map->dm_segs[0].ds_addr = 0;
}

void bus_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t map, bus_size_t offset,
	bus_size_t len, int ops) {
	(void) tag;

	uint8_t *va = (uint8_t *) (uintptr_t) map->dm_segs[0].ds_addr;

	if (va == NULL || len == 0 || map->dm_nsegs == 0) {
		return;
	}
	if (ops & BUS_DMASYNC_PREWRITE) {
		FCacheDCacheFlushRange((intptr)(va + offset), len);
		__asm volatile("dsb st");
	}
	if (ops & BUS_DMASYNC_PREREAD) {
		FCacheDCacheInvalidateRange((intptr)(va + offset), len);
	}
	if (ops & BUS_DMASYNC_POSTREAD) {
		FCacheDCacheInvalidateRange((intptr)(va + offset), len);
		__asm volatile("dsb sy");
	}
	if (ops & BUS_DMASYNC_POSTWRITE) {
		__asm volatile("dsb sy");
	}
}

/* ------------------------------------------------------------------ */
/* PCI autoconf shim (declared in compat dev/pci/pcivar.h); the tag is
 * the claimed pcie_dw_func below */

static struct pcie_dw_func wlan_pcie_device;

pcireg_t pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg) {
	struct pcie_dw_func *f = tag;

	(void) pc;
	if (!f->is_present) {
		return 0xffffffffu;
	}
	return pcie_dw_conf_read(&wlan_pcie_softc, f->bus, f->device,
	    f->function, (u_int) reg);
}

void pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg,
	pcireg_t val) {
	struct pcie_dw_func *f = tag;

	(void) pc;
	if (f->is_present) {
		pcie_dw_conf_write(&wlan_pcie_softc, f->bus, f->device,
		    f->function, (u_int) reg, val);
	}
}

int pci_get_capability(pci_chipset_tag_t pc, pcitag_t tag, int cap_id,
	int *offsetp, pcireg_t *valuep) {
	pcireg_t cr = pci_conf_read(pc, tag, PCI_CAPLISTPTR_REG);
	int off = PCI_CAPLIST_PTR(cr);
	int hops;

	for (hops = 0; off != 0 && hops < 48; hops++) {
		pcireg_t val = pci_conf_read(pc, tag, off);

		if (PCI_CAPLIST_CAP(val) == cap_id) {
			if (offsetp != NULL) {
				*offsetp = off;
			}
			if (valuep != NULL) {
				*valuep = val;
			}
			return 1;
		}
		off = (val >> 8) & 0xfc;
	}
	return 0;
}

pcireg_t pci_mapreg_type(pci_chipset_tag_t pc, pcitag_t tag, int reg) {
	(void) pc; (void) tag; (void) reg;
	return 0x00; /* 32-bit non-prefetchable memory */
}

int pci_mapreg_map(const struct pci_attach_args *pa, int reg,
	pcireg_t type, int busflags, bus_space_tag_t *tagp,
	bus_space_handle_t *handlep, bus_addr_t *basep, bus_size_t *sizep) {
	struct pcie_dw_func *d = pa->pa_tag;
	const struct pcie_dw_bar *bar;

	(void) type; (void) busflags;

	if (reg < PCI_MAPREG_START || (reg - PCI_MAPREG_START) % 4 != 0) {
		return EINVAL;
	}
	bar = &d->bars[(reg - PCI_MAPREG_START) / 4];
	if (bar->size == 0 || bar->phys_addr == 0) {
		return ENOENT;
	}
	if (tagp != NULL) {
		static bus_space_tag_t the_tag;
		*tagp = the_tag;
	}
	*handlep = (bus_space_handle_t) bar->phys_addr;
	if (basep != NULL) {
		*basep = bar->phys_addr;
	}
	if (sizep != NULL) {
		*sizep = (bus_size_t) bar->size;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* softint: the ISR only schedules; the worker owns the processing
 * under the port serializer */

struct wlan_softint {
	void (*func)(void *);
	void *arg;
};

static struct wlan_softint *wlan_pcie_soft_si;
static SemaphoreHandle_t wlan_pcie_soft_sem;

void *softint_establish(int flags, void (*func)(void *), void *arg) {
	(void) flags;

	wlan_pcie_soft_si = wlan_kmalloc(sizeof(*wlan_pcie_soft_si),
	    M_ZERO, M_DEVBUF);
	if (wlan_pcie_soft_si == NULL) {
		return NULL;
	}
	wlan_pcie_soft_si->func = func;
	wlan_pcie_soft_si->arg = arg;
	return wlan_pcie_soft_si;
}

void softint_schedule(void *sih) {
	(void) sih;

	/* the only caller is the hard ISR: FromISR APIs are mandatory
	 * here (a plain give would hit the port's enter-critical assert) */
	BaseType_t woken = pdFALSE;

	xSemaphoreGiveFromISR(wlan_pcie_soft_sem, &woken);
	portYIELD_FROM_ISR(woken);
}

static void *wlan_pcie_soft_worker(void *arg) {
	(void) arg;

	for (;;) {
		xSemaphoreTake(wlan_pcie_soft_sem, portMAX_DELAY);
		if (wlan_pcie_soft_si == NULL) {
			continue;
		}
		wlan_port_serializer_lock();
		wlan_pcie_soft_si->func(wlan_pcie_soft_si->arg);
		wlan_port_serializer_unlock();
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/* workqueue(9): one worker task per queue */

struct workqueue {
	workqueue_func_t wq_func;
	void *wq_arg;
	struct work *wq_head;
	struct work **wq_tail;
	kmutex_t wq_mtx;
	SemaphoreHandle_t wq_sem;
};

static void *wlan_pcie_workqueue_worker(void *arg) {
	struct workqueue *wq = arg;

	for (;;) {
		struct work *wk;

		xSemaphoreTake(wq->wq_sem, portMAX_DELAY);
		wlan_mutex_enter(&wq->wq_mtx);
		wk = wq->wq_head;
		if (wk != NULL) {
			wq->wq_head = wk->w_qnext;
			if (wq->wq_head == NULL) {
				wq->wq_tail = &wq->wq_head;
			}
		}
		wlan_mutex_exit(&wq->wq_mtx);
		if (wk == NULL) {
			continue;
		}

		wlan_port_serializer_lock();
		wq->wq_func(wk, wq->wq_arg);
		wlan_port_serializer_unlock();
	}
	return NULL;
}

int workqueue_create(struct workqueue **wqp, const char *name,
	workqueue_func_t func, void *arg, int pri, int ipl, int flags) {
	struct workqueue *wq;
	TaskHandle_t task = NULL;

	(void) name; (void) pri; (void) ipl; (void) flags;

	wq = wlan_kmalloc(sizeof(*wq), M_ZERO, M_DEVBUF);
	if (wq == NULL) {
		return ENOMEM;
	}
	wq->wq_func = func;
	wq->wq_arg = arg;
	wq->wq_tail = &wq->wq_head;
	wlan_mutex_init(&wq->wq_mtx, MUTEX_DEFAULT, IPL_NONE);
	wq->wq_sem = xSemaphoreCreateCounting(0xffff, 0);

	if (xTaskCreate((TaskFunction_t) wlan_pcie_workqueue_worker,
	    "wlan_wq", 4096, wq, 4, &task) != pdPASS) {
		wlan_kfree(wq, M_DEVBUF);
		return ENOMEM;
	}

	*wqp = wq;
	return 0;
}

void workqueue_enqueue(struct workqueue *wq, struct work *wk, void *cpu) {
	(void) cpu;

	wk->w_qnext = NULL;
	wlan_mutex_enter(&wq->wq_mtx);
	*wq->wq_tail = wk;
	wq->wq_tail = &wk->w_qnext;
	wlan_mutex_exit(&wq->wq_mtx);
	xSemaphoreGive(wq->wq_sem);
}

void workqueue_destroy(struct workqueue *wq) {
	(void) wq;
}

/* ------------------------------------------------------------------ */
/* device claim + port lifecycle */

static struct wlan_pcie_dev wlan_pcie_dev;

static int (*wlan_pcie_intr_func)(void *);
static void *wlan_pcie_intr_arg;

/* how many times the endpoint's doorbell reached us; a diagnostics
 * counter for the MSI -> ITS -> LPI transport */
volatile unsigned wlan_pcie_intr_fired;

/* interrupt dispatch calls (vector, param) */
static void wlan_pcie_intr_trampoline(s32 vector, void *param) {
	(void) vector;
	wlan_pcie_intr_fired++;
	(void) wlan_pcie_intr_func(param);
}

/* live INTx view for attach-time diagnostics: bit n of the client
 * legacy status register mirrors endpoint INTn (set while the card
 * holds the line asserted); bit 4 = endpoint PCI_STATUS.INTERRUPT */
unsigned wlan_pcie_intx_status(void) {
	unsigned st = pcie_dw_intx_status(&wlan_pcie_softc);

	if (wlan_pcie_device.is_present) {
		pcireg_t ps = pci_conf_read(NULL, &wlan_pcie_device,
		    PCI_COMMAND_STATUS_REG);

		if ((ps & PCI_STATUS_INTERRUPT) != 0) {
			st |= 0x10u;
		}
	}
	return st;
}

int pci_intr_alloc(const struct pci_attach_args *pa,
	pci_intr_handle_t **ihpp, int *countsp, int mintype) {
	pci_intr_handle_t *ihp;

	(void) pa;
	(void) mintype;

	/* legacy INTA: RK3568 routes the root port's INTx to GIC IRQ 194.
	 * The MSI->ITS->LPI delivery through this root port never reached
	 * the CPU, while SPI delivery is proven by the EHCI path. */
	ihp = wlan_kmalloc(sizeof(*ihp), M_ZERO, M_DEVBUF);
	if (ihp == NULL) {
		return ENOMEM;
	}
	*ihp = wlan_kmalloc(sizeof(**ihp), M_ZERO, M_DEVBUF);
	if (*ihp == NULL) {
		wlan_kfree(ihp, M_DEVBUF);
		return ENOMEM;
	}
	(*ihp)->irq = 194; /* FPCIE_ECAM_INTA_IRQ_NUM */
	(*ihp)->type = PCI_INTR_TYPE_INTX;
	*ihpp = ihp;
	if (countsp != NULL) {
		*countsp = 1;
	}
	return 0;
}

enum pci_intr_type pci_intr_type(pci_chipset_tag_t pc,
	pci_intr_handle_t ih) {
	(void) pc;
	return ih->type;
}

const char *pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih,
	char *buf, size_t len) {
	(void) pc;
	snprintf(buf, len, "msi lpi %d", ih->irq);
	return buf;
}

void *pci_intr_establish_xname(pci_chipset_tag_t pc,
	pci_intr_handle_t ih, int level, int (*func)(void *), void *arg,
	const char *name) {
	wlan_pcie_intr_func = func;
	wlan_pcie_intr_arg = arg;

	/* legacy INTx: keep the MSI capability disabled so the card
	 * asserts INTA; the root port forwards it to GIC IRQ 194 */
	if (wlan_pcie_device.msi_cap != 0) {
		u16 cap = wlan_pcie_device.msi_cap;
		uint32_t mc = pci_conf_read(NULL, &wlan_pcie_device,
		    cap) & 0xffffu;

		pci_conf_write(NULL, &wlan_pcie_device, cap,
		    (pcireg_t) (mc & ~0x1u)); /* MSI disable */
		printf("wlan pcie: msi cap@%02x disabled (INTx mode)\n",
		    cap);
	}

	/* unmask INTA~INTD in the client aggregation register so the
	 * endpoint's INTA assertion is forwarded to GIC INTID 194, and
	 * make sure the root port does not suppress it (PCI command
	 * bit 10 on the root port and in its bridge control) */
	pcie_dw_intx_arm(&wlan_pcie_softc);
	printf("wlan pcie: legacy status=%08x\n",
	    pcie_dw_intx_status(&wlan_pcie_softc));

	/* INTID 194 = SPI 162 aggregates the four INTx lines; the board
	 * dtsi declares that line edge-rising, and a pulse on a
	 * level-configured SPI is never latched */
	InterruptSetTrigerMode((int) ih->irq, IRQ_MODE_TRIG_EDGE);

	/* IRQ 194 is the INTx aggregation SPI; the handler acks the card
	 * (CSR_INT write) which deasserts INTA */
	/* FromISR APIs in the handler chain require a priority at or
	 * below (numerically >=) configMAX_API_CALL_INTERRUPT_PRIORITY */
	InterruptSetPriority((int) ih->irq,
	    configMAX_API_CALL_INTERRUPT_PRIORITY);
	InterruptInstall(ih->irq, wlan_pcie_intr_trampoline,
	    wlan_pcie_intr_arg, (char *) name);
	InterruptUmask(ih->irq);
	printf("wlan pcie: INTx installed on irq %d\n", ih->irq);
	return &wlan_pcie_intr_trampoline;
}

void pci_intr_release(pci_chipset_tag_t pc, pci_intr_handle_t *ihp,
	int count) {
	(void) pc;
	if (ihp != NULL && *ihp != NULL) {
		wlan_kfree(*ihp, M_DEVBUF);
		*ihp = NULL;
	}
	(void) count;
}

void pci_aprint_devinfo(const struct pci_attach_args *pa,
	const char *name) {
	struct pcie_dw_func *d = pa->pa_tag;

	printf("%s at pci%u dev %u func %u (vendor %04x product %04x)\n",
	    name != NULL ? name : "iwm", d->bus, d->device, d->function,
	    d->vendor_id, d->device_id);
}

static int wlan_pcie_match(const struct wlan_chip_driver *drv,
	uint16_t vendor, uint16_t device) {
	const struct wlan_pcie_id *id;

	if (drv->bus != WLAN_BUS_PCIE || drv->pcie_ids == NULL) {
		return 0;
	}
	for (id = drv->pcie_ids; id->vendor != 0; id++) {
		if (id->vendor == vendor && id->device == device) {
			return 1;
		}
	}
	return 0;
}

static int wlan_pcie_claim(void) {
	const struct wlan_chip_driver *drv = NULL;
	struct pci_attach_args pa;
	uint32_t id;
	int ret;

	/* controller bring-up: keep the U-Boot-trained link, program
	 * the outbound MEM window and the config viewport */
	ret = pcie_dw_attach(&wlan_pcie_softc, &wlan_pcie_ctlr);
	if (ret != 0) {
		printf("wlan pcie: controller init failed (link up=%d)\n",
		    wlan_pcie_softc.link_up);
		return -EIO;
	}
	printf("wlan pcie: root port vid/did = %08x, link kept, "
	       "secondary bus %u\n",
	    pcie_dw_conf_read(&wlan_pcie_softc, 0, 0, 0, 0x00),
	    wlan_pcie_softc.secondary_bus);

	/* the function behind the secondary bus: ID, caps, BAR0, BME */
	ret = pcie_dw_claim_func(&wlan_pcie_softc, &wlan_pcie_device);
	if (ret != 0) {
		printf("wlan pcie: no endpoint on the secondary bus\n");
		return -ENODEV;
	}
	id = ((uint32_t) wlan_pcie_device.device_id << 16) |
	    wlan_pcie_device.vendor_id;
	printf("wlan pcie: bus %u device vid/did = %08x, "
	       "caps msi=%02x pcie=%02x\n",
	    wlan_pcie_softc.secondary_bus, id,
	    wlan_pcie_device.msi_cap, wlan_pcie_device.pcie_cap);

	for (u32 k = 0; (drv = wlan_pcie_drivers[k]) != NULL; k++) {
		if (wlan_pcie_match(drv, (uint16_t) id,
		    (uint16_t) (id >> 16))) {
			break;
		}
	}
	if (drv == NULL) {
		printf("wlan pcie: %08x has no driver\n", id);
		return -ENODEV;
	}

	memset(&wlan_pcie_dev, 0, sizeof(wlan_pcie_dev));
	wlan_pcie_dev.vendor = wlan_pcie_device.vendor_id;
	wlan_pcie_dev.device = wlan_pcie_device.device_id;
	wlan_pcie_dev.rev = wlan_pcie_device.revision_id;
	wlan_pcie_dev.bus_n = wlan_pcie_device.bus;
	wlan_pcie_dev.slot = 0;
	wlan_pcie_dev.func = 0;
	wlan_pcie_dev.port_priv = &wlan_pcie_device;
	wlan_pcie_dev.env_dev = &wlan_pcie_device;

	wlan_pcie_dev.bar_pa = wlan_pcie_device.bars[0].phys_addr;
	wlan_pcie_dev.bar_size = wlan_pcie_device.bars[0].size;
	wlan_pcie_dev.bar_va = (void *) (uintptr_t) wlan_pcie_dev.bar_pa;

	memset(&pa, 0, sizeof(pa));
	pa.pa_pc = NULL;
	pa.pa_tag = &wlan_pcie_device;
	pa.pa_id = id;
	pa.pa_dmat = wlan_bus_dma_tag;

	printf("wlan pcie: claimed %04x:%04x at %02x:%02x.%u "
	       "bar0=%llx\n",
	    wlan_pcie_dev.vendor, wlan_pcie_dev.device,
	    wlan_pcie_dev.bus_n, wlan_pcie_dev.slot, wlan_pcie_dev.func,
	    (unsigned long long) wlan_pcie_dev.bar_pa);

	/* the interrupt is allocated inside attach (pci_intr_alloc) */
	return drv->attach(&wlan_pcie_dev, NULL);
}

/* the claim runs on its own task: enumeration and the firmware load
 * that follows want a normal scheduling context */
static void wlan_pcie_claim_task(void *arg) {
	(void) arg;

	vTaskDelay(pdMS_TO_TICKS(300)); /* let ITS/PCI settle */

	wlan_pcie_soft_sem = xSemaphoreCreateCounting(0xffff, 0);
	if (xTaskCreate((TaskFunction_t) wlan_pcie_soft_worker,
	    "wlan_si", 4096, NULL, 4, NULL) != pdPASS) {
		printf("wlan pcie: softint worker failed\n");
		return;
	}

	wlan_pcie_claim();
	vTaskDelete(NULL);
}

int wlan_pcie_port_init(void) {
	wlan_osal_freertos_init();

	if (xTaskCreate(wlan_pcie_claim_task, "pcie_claim", 8192,
	    NULL, 3, NULL) != pdPASS) {
		return -ENOMEM;
	}
	return 0;
}
