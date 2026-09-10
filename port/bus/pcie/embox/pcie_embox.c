/*
 * @file
 * @brief PCIe bus backend for embox: claim the device from the
 * enumerated PCI tree, map its register window, allocate one MSI
 * vector, and run the interrupt-worker / state-machine threads the
 * imported driver expects.
 *
 * The platform runs identity mapped without an SMMU in the PCIe path
 * (the same ground the in-tree NVMe driver was verified on), so DMA
 * addresses are virtual addresses and the bus_dma shim below carries
 * ownership with dcache flush/invalidate.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <embox/unit.h>

#include <drivers/pci/pci.h>
#include <drivers/pci/pci_regs.h>
#include <hal/cache.h>
#include <hal/mem_barriers.h>
#include <kernel/irq.h>
#include <kernel/sched.h>
#include <kernel/sched/waitq.h>
#include <kernel/thread.h>
#include <kernel/thread/thread_sched_wait.h>
#include <kernel/task.h>
#include <kernel/task/kernel_task.h>
#include <kernel/time/ktime.h>
#include <util/err.h>

#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/workqueue.h>

#include <kernel/thread/sync/mutex.h>
#include <dev/pci/pcivar.h>

#include <port/port.h>
#include <port/osal/embox/wlan_port_embox.h>
#include <port/bus/pcie/port_pcie.h>

/* the chip drivers this backend serves (explicit, like the USB
 * backend's registry in net_bridge.c) */
extern const struct wlan_chip_driver iwm_driver;

static const struct wlan_chip_driver *const wlan_pcie_drivers[] = {
	&iwm_driver,
	NULL
};

bus_dma_tag_t wlan_bus_dma_tag;

void wlan_bus_space_barrier(bus_space_tag_t t, bus_space_handle_t h,
	bus_size_t o, bus_size_t len, int flags) {
	(void) t; (void) h; (void) o; (void) len;
	if (flags & BUS_SPACE_BARRIER_READ) {
		dsb(sy);
	} else {
		dsb(st);
	}
}

static void *wlan_port_thread_create_local(void *(*run)(void *),
	void *arg);
static void wlan_port_thread_start_local(void *thread);

/* the one claimed device on this platform */
static struct wlan_pcie_env {
	struct wlan_pcie_dev dev;
	struct waitq soft_wq;
	volatile int soft_pending;
	struct thread *soft_worker;
	int running;
} wlan_pcie;

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

/* One aligned allocation per segment; the raw pointer rides one slot
 * below the aligned address for bus_dmamem_free. */
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

/* the mbuf owns one contiguous cluster (compat sys/mbuf.h): map the
 * data area the driver handed the device */
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

/* Carry the CPU<->device ownership transition through the dcache.
 * Every mapped region is aligned and rounded to full cache lines, so
 * the invalidate cannot destroy neighbouring data. */
void bus_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t map, bus_size_t offset,
	bus_size_t len, int ops) {
	(void) tag;

	uint8_t *va = (uint8_t *) (uintptr_t) map->dm_segs[0].ds_addr;

	if (va == NULL || len == 0 || map->dm_nsegs == 0) {
		return;
	}
	if (ops & BUS_DMASYNC_PREWRITE) {
		dcache_flush(va + offset, len);
		dsb(st);
	}
	if (ops & BUS_DMASYNC_PREREAD) {
		dcache_inval(va + offset, len);
	}
	if (ops & BUS_DMASYNC_POSTREAD) {
		dcache_inval(va + offset, len);
		dsb(sy);
	}
	if (ops & BUS_DMASYNC_POSTWRITE) {
		dsb(sy);
	}
}

/* ------------------------------------------------------------------ */
/* PCI autoconf shim (declared in compat dev/pci/pcivar.h) */

pcireg_t pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg) {
	struct pci_slot_dev *d = tag;
	uint32_t v = 0xffffffffu;

	(void) pc;
	pci_read_config_dword(d, reg, &v);
	return v;
}

void pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg,
	pcireg_t val) {
	(void) pc;
	pci_write_config_dword((struct pci_slot_dev *) tag, reg, val);
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
	struct pci_slot_dev *d = pa->pa_tag;
	uint32_t bar;
	int idx;

	(void) type; (void) busflags;

	if (reg < PCI_MAPREG_START || (reg - PCI_MAPREG_START) % 4 != 0) {
		return EINVAL;
	}
	idx = (reg - PCI_MAPREG_START) / 4;
	bar = pci_resource_start(d, idx);
	if (bar == 0) {
		return ENOENT;
	}
	/* identity mapped platform: the physical BAR address is usable */
	if (tagp != NULL) {
		static bus_space_tag_t the_tag;
		*tagp = the_tag;
	}
	*handlep = (bus_space_handle_t) bar;
	if (basep != NULL) {
		*basep = bar;
	}
	if (sizep != NULL) {
		*sizep = 0x10000; /* CSR window the 7k family decodes */
	}
	return 0;
}

/* one device, one established handler */
static int (*wlan_pcie_intr_func)(void *);
static void *wlan_pcie_intr_arg;

static irq_return_t wlan_pcie_intr_trampoline(unsigned int irq_nr,
	void *arg) {
	(void) irq_nr;
	(void) arg;
	return wlan_pcie_intr_func(wlan_pcie_intr_arg) != 0 ?
	    IRQ_HANDLED : IRQ_NONE;
}

int pci_intr_alloc(const struct pci_attach_args *pa,
	pci_intr_handle_t **ihpp, int *countsp, int mintype) {
	struct pci_slot_dev *d = pa->pa_tag;
	pci_intr_handle_t *ihp;
	int nvec;

	(void) mintype;

	nvec = pci_alloc_irq_vectors(d, 1, 1, PCI_IRQ_MSI);
	if (nvec < 1) {
		return ENODEV;
	}
	ihp = wlan_kmalloc(sizeof(*ihp), M_ZERO, M_DEVBUF);
	if (ihp == NULL) {
		return ENOMEM;
	}
	*ihp = wlan_kmalloc(sizeof(**ihp), M_ZERO, M_DEVBUF);
	if (*ihp == NULL) {
		wlan_kfree(ihp, M_DEVBUF);
		return ENOMEM;
	}
	(*ihp)->irq = pci_irq_vector(d, 0);
	(*ihp)->type = PCI_INTR_TYPE_MSI;
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
	snprintf(buf, len, "msi %d", ih->irq);
	return buf;
}

void *pci_intr_establish_xname(pci_chipset_tag_t pc,
	pci_intr_handle_t ih, int level, int (*func)(void *), void *arg,
	const char *name) {
	(void) pc; (void) level;

	wlan_pcie_intr_func = func;
	wlan_pcie_intr_arg = arg;
	if (irq_attach(ih->irq, wlan_pcie_intr_trampoline, 0, NULL, name)
	    != 0) {
		return NULL;
	}
	return &wlan_pcie_intr_trampoline;
}

void pci_intr_release(pci_chipset_tag_t pc, pci_intr_handle_t *ihp,
	int count) {
	(void) pc;
	if (ihp != NULL) {
		wlan_kfree(ihp, M_DEVBUF);
	}
	(void) count;
}

void pci_aprint_devinfo(const struct pci_attach_args *pa,
	const char *name) {
	struct pci_slot_dev *d = pa->pa_tag;

	printf("%s at pci%u dev %u func %u (vendor %04x product %04x)\n",
	    name != NULL ? name : "iwm", d->busn, d->slot, d->func,
	    d->vendor, d->device);
}

/* ------------------------------------------------------------------ */
/* softint: the driver handler only masks the interrupt and schedules
 * the worker; the worker owns the whole processing under the port
 * serializer */

struct wlan_softint {
	void (*func)(void *);
	void *arg;
};

static struct wlan_softint *wlan_pcie_soft_si;

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
	if (!wlan_pcie.running) {
		return;
	}
	wlan_pcie.soft_pending = 1;
	waitq_wakeup(&wlan_pcie.soft_wq, 1);
}

static void *wlan_pcie_soft_worker(void *arg) {
	(void) arg;

	thread_yield();
	while (wlan_pcie.running) {
		struct waitq_link *wql = &thread_self()->schedee.waitq_link;

		waitq_link_init(wql);
		for (;;) {
			waitq_wait_prepare(&wlan_pcie.soft_wq, wql);
			if (wlan_pcie.soft_pending || !wlan_pcie.running) {
				break;
			}
			sched_wait_timeout(1000, NULL);
		}
		waitq_wait_cleanup(&wlan_pcie.soft_wq, wql);

		if (!wlan_pcie.running || wlan_pcie_soft_si == NULL) {
			break;
		}
		wlan_pcie.soft_pending = 0;

		wlan_port_serializer_lock();
		wlan_pcie_soft_si->func(wlan_pcie_soft_si->arg);
		wlan_port_serializer_unlock();
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/* workqueue(9): one worker thread per queue, driver callback runs
 * with (work item, context) */

struct workqueue {
	workqueue_func_t wq_func;
	void *wq_arg;
	struct work *wq_head;
	struct work **wq_tail;
	struct mutex wq_mtx;
	struct waitq wq_wait;
	volatile int wq_work;
	struct thread *wq_worker;
	int wq_running;
};

static void *wlan_pcie_workqueue_worker(void *arg) {
	struct workqueue *wq = arg;

	thread_yield();
	while (wq->wq_running) {
		struct work *wk;

		struct waitq_link *wql = &thread_self()->schedee.waitq_link;

		waitq_link_init(wql);
		for (;;) {
			waitq_wait_prepare(&wq->wq_wait, wql);
			if (wq->wq_work) {
				break;
			}
			sched_wait_timeout(1000, NULL);
		}
		waitq_wait_cleanup(&wq->wq_wait, wql);

		mutex_lock(&wq->wq_mtx);
		wk = wq->wq_head;
		if (wk != NULL) {
			wq->wq_head = wk->w_qnext;
			if (wq->wq_head == NULL) {
				wq->wq_tail = &wq->wq_head;
				wq->wq_work = 0;
			}
		}
		mutex_unlock(&wq->wq_mtx);
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

	(void) name; (void) pri; (void) ipl; (void) flags;

	wq = wlan_kmalloc(sizeof(*wq), M_ZERO, M_DEVBUF);
	if (wq == NULL) {
		return ENOMEM;
	}
	wq->wq_func = func;
	wq->wq_arg = arg;
	wq->wq_tail = &wq->wq_head;
	mutex_init_default(&wq->wq_mtx, NULL);
	waitq_init(&wq->wq_wait);
	wq->wq_running = 1;

	wq->wq_worker = wlan_port_thread_create_local(
	    wlan_pcie_workqueue_worker, wq);
	if (wq->wq_worker == NULL) {
		wlan_kfree(wq, M_DEVBUF);
		return ENOMEM;
	}
	wlan_port_thread_start_local(wq->wq_worker);

	*wqp = wq;
	return 0;
}

void workqueue_enqueue(struct workqueue *wq, struct work *wk, void *cpu) {
	(void) cpu;

	wk->w_qnext = NULL;
	mutex_lock(&wq->wq_mtx);
	*wq->wq_tail = wk;
	wq->wq_tail = &wk->w_qnext;
	wq->wq_work = 1;
	mutex_unlock(&wq->wq_mtx);
	waitq_wakeup(&wq->wq_wait, 1);
}

void workqueue_destroy(struct workqueue *wq) {
	(void) wq;
}

/* ------------------------------------------------------------------ */
/* device claim + port lifecycle */

static void *wlan_port_thread_create_local(void *(*run)(void *), void *arg) {
	struct thread *t = thread_create(
	    THREAD_FLAG_NOTASK | THREAD_FLAG_SUSPENDED, run, arg);

	if (ptr2err(t)) {
		return NULL;
	}
	task_thread_register(task_kernel_task(), t);
	thread_detach(t);
	return t;
}

static void wlan_port_thread_start_local(void *thread) {
	thread_launch((struct thread *) thread);
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
	struct pci_slot_dev *dev;

	pci_foreach_dev(dev) {
		const struct wlan_chip_driver *drv;
		struct pci_attach_args pa;
		uint32_t bar;
		int i;

		for (i = 0; (drv = wlan_pcie_drivers[i]) != NULL; i++) {
			if (wlan_pcie_match(drv, dev->vendor, dev->device)) {
				break;
			}
		}
		if (drv == NULL) {
			continue;
		}

		bar = pci_resource_start(dev, 0);
		if (bar == 0) {
			printf("wlan pcie: matched %04x:%04x without BAR0\n",
			    dev->vendor, dev->device);
			continue;
		}

		memset(&wlan_pcie.dev, 0, sizeof(wlan_pcie.dev));
		wlan_pcie.dev.env_dev = dev;
		wlan_pcie.dev.vendor = dev->vendor;
		wlan_pcie.dev.device = dev->device;
		wlan_pcie.dev.rev = dev->rev;
		wlan_pcie.dev.bus_n = (uint8_t) dev->busn;
		wlan_pcie.dev.slot = dev->slot;
		wlan_pcie.dev.func = dev->func;
		wlan_pcie.dev.bar_pa = bar;
		wlan_pcie.dev.bar_size = 0x10000;
		wlan_pcie.dev.bar_va = (void *) (uintptr_t) bar;
		wlan_pcie.dev.port_priv = &wlan_pcie;

		printf("wlan pcie: claimed %04x:%04x at %02x:%02x.%u "
		       "bar0=%#x\n",
		    dev->vendor, dev->device, dev->busn, dev->slot,
		    dev->func, bar);

		memset(&pa, 0, sizeof(pa));
		pa.pa_pc = NULL;
		pa.pa_tag = dev;
		pa.pa_id = ((uint32_t) dev->device << 16) | dev->vendor;
		pa.pa_dmat = wlan_bus_dma_tag;

		drv->attach(&wlan_pcie.dev, NULL);
		return 0;
	}

	printf("wlan pcie: no matching device in the PCI tree\n");
	return -ENODEV;
}

/* The claim runs on its own thread, not in the unit-init context: the
 * MSI vector it allocates comes from the ITS, and the firmware load
 * and ring setup that follow want a normal scheduling context. */
static void *wlan_pcie_claim_thread(void *arg) {
	(void) arg;

	/* let the remaining runlevels (ITS, PCI enumeration) finish */
	{
		int64_t deadline = ktime_get_ns() + 300 * 1000 * 1000;
		while (ktime_get_ns() < deadline) {
			thread_yield();
		}
	}

	waitq_init(&wlan_pcie.soft_wq);
	wlan_pcie.running = 1;
	wlan_pcie.soft_worker = wlan_port_thread_create_local(
	    wlan_pcie_soft_worker, NULL);
	if (wlan_pcie.soft_worker != NULL) {
		wlan_port_thread_start_local(wlan_pcie.soft_worker);
	}

	wlan_pcie_claim();
	return NULL;
}

static int wlan_pcie_port_init(void) {
	struct thread *t;

	waitq_init(&wlan_pcie.soft_wq);

	t = wlan_port_thread_create_local(wlan_pcie_claim_thread, NULL);
	if (t == NULL) {
		return -ENOMEM;
	}
	wlan_port_thread_start_local(t);

	return 0;
}

EMBOX_UNIT_INIT(wlan_pcie_port_init);
