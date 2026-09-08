/*
 * @file
 * @brief NetBSD usbd_*(9) implementation over the CherryUSB host stack.
 *
 * Semantics (mirroring the FreeBSD/NetBSD usbd(9) shim pattern):
 *  - usbd_do_request -> usbh_control_transfer (blocking, 500 ms timeout
 *    inside cherryusb, 64-byte aligned bounce buffer);
 *  - usbd_transfer -> usbh_submit_urb with urb->timeout = 0, i.e. pure
 *    asynchronous per-URB QH; the NetBSD timeout watchdog stays with the
 *    driver (urtwn_watchdog -> usbd_abort_pipe);
 *  - urb->complete runs in the EHCI interrupt handler, so it only drops
 *    the xfer into an IPL-protected ring and posts a semaphore; the
 *    driver callbacks run on the per-device worker kthread;
 *  - xfer buffers are 64-byte aligned (the driver exclusively uses
 *    usbd_get_buffer, so the shim owns all DMA memory);
 *  - descriptors come from the real cherryusb enumeration, not copies.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <hal/ipl.h>

#include <usbh_core.h>
#include <usb_osal.h>

/* cherryusb's usb_def.h and NetBSD's dev/usb/usb.h disagree on the
 * numeric values of the speed codes above HIGH and on USB_MAX_DEVICES;
 * this unit lives in both worlds, so the cherryusb headers come first
 * and the NetBSD definitions win afterwards */
#undef USB_MAX_DEVICES
#undef USB_SPEED_LOW
#undef USB_SPEED_FULL
#undef USB_SPEED_HIGH
#undef USB_SPEED_WIRELESS
#undef USB_SPEED_SUPER
#undef USB_SPEED_SUPER_PLUS

#include <sys/queue.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <port/port.h>

#include "wlan_port_cherryusb.h"

/* embox thread entry points, provided by net_bridge.c */
extern void *wlan_port_thread_create(void *(*run)(void *), void *arg);
extern void wlan_port_thread_start(void *thread);
extern void ksleep(unsigned int ms);

#define USBD_SHIM_ALIGN 64
#define USBD_SHIFACE_MAX 4
#define USBD_SHIM_RING  16

/* ------------------------------------------------------------------ */

struct usbd_interface {
	struct usbd_device *udev;
	uint8_t ifno;
};

struct usbd_device {
	struct usbh_hubport *hport;
	struct wlan_usb_dev *ud_port;
	usb_device_descriptor_t ddesc;
	struct usbd_interface ifaces[USBD_SHIFACE_MAX];
	volatile int dying;

	/* urb completion ring (producer: EHCI interrupt) */
	struct usbd_xfer *ring[USBD_SHIM_RING];
	volatile unsigned ring_head;
	usb_osal_sem_t ring_sem;

	/* usb task queue ring */
	struct usb_task *tasks[USBD_SHIM_RING];
	volatile unsigned task_head;
	usb_osal_sem_t task_sem;

	kmutex_t wq_mtx;
	kcondvar_t wq_cv;
	void *urb_worker;
	void *taskq_worker;
	int workers_started;
	int running;
};

struct usbd_pipe {
	struct usbd_device *dev;
	struct usb_endpoint_descriptor *cherry_ep; /* inside hport config */
	usb_endpoint_descriptor_t ed;              /* NetBSD view */
	SLIST_HEAD(, usbd_xfer) pending;
};

struct usbd_xfer {
	struct usbd_pipe *pipe;
	void *priv;
	void *buffer;
	uint32_t length;
	uint16_t flags;
	uint32_t timeout;
	usbd_callback callback;
	void *dma_buf;
	usbd_status status;
	uint32_t actlen;
	volatile int in_flight;
	struct usbh_urb urb;
	SLIST_ENTRY(usbd_xfer) next;
};

/* control bounce region: EHCI requires 64-byte aligned setup/data while
 * the driver buffers live on the stack or in the softc */
static struct {
	struct usb_setup_packet setup;
	uint8_t pad[64 - sizeof(struct usb_setup_packet)];
	uint8_t buf[512];
} __attribute__((aligned(64))) s_ctrl;
static kmutex_t s_ctrl_mtx;
static int s_shim_ready;

static void shim_locks_init(void) {
	if (s_shim_ready) {
		return;
	}
	s_shim_ready = 1;
	mutex_init(&s_ctrl_mtx, MUTEX_DEFAULT, IPL_USB);
}

static usbd_status usbd_map_err(int cherry_err) {
	switch (cherry_err) {
	case 0:
		return USBD_NORMAL_COMPLETION;
	case -USB_ERR_TIMEOUT:
		return USBD_TIMEOUT;
	case -USB_ERR_SHUTDOWN:
	case -USB_ERR_NOTCONN:
		return USBD_CANCELLED;
	case -USB_ERR_STALL:
		return USBD_STALLED;
	case -USB_ERR_NOMEM:
		return USBD_NOMEM;
	default:
		return USBD_IOERROR;
	}
}

static void usbd_ring_post(struct usbd_device *dev, struct usbd_xfer *xfer) {
	ipl_t ipl;

	ipl = ipl_save();
	if (dev->ring_head < USBD_SHIM_RING) {
		dev->ring[dev->ring_head++] = xfer;
	}
	ipl_restore(ipl);
	usb_osal_sem_give(dev->ring_sem);
}

/* ------------------------------------------------------------------ */
/* device/interface shells */

static struct usbd_device *usbd_shim_register_device(struct usbh_hubport *hport,
	struct wlan_usb_dev *port_dev) {
	struct usbd_device *dev;
	uint8_t i;

	dev = wlan_kmalloc(sizeof(*dev), M_WAITOK | M_ZERO, M_USB);
	if (dev == NULL) {
		return NULL;
	}
	dev->hport = hport;
	dev->ud_port = port_dev;
	memcpy(&dev->ddesc, &hport->device_desc, sizeof(dev->ddesc));
	for (i = 0; i < USBD_SHIFACE_MAX; i++) {
		dev->ifaces[i].udev = dev;
		dev->ifaces[i].ifno = i;
	}
	return dev;
}

usb_device_descriptor_t *usbd_get_device_descriptor(struct usbd_device *dev) {
	return &dev->ddesc;
}

usb_interface_descriptor_t *usbd_get_interface_descriptor(
	struct usbd_interface *iface) {
	struct usbh_interface_altsetting *alt;

	if (iface == NULL || iface->udev->hport == NULL) {
		return NULL;
	}
	alt = &iface->udev->hport->config.intf[iface->ifno].altsetting[0];
	return (usb_interface_descriptor_t *) &alt->intf_desc;
}

usb_endpoint_descriptor_t *usbd_interface2endpoint_descriptor(
	struct usbd_interface *iface, uint8_t index) {
	struct usbh_interface_altsetting *alt;

	if (iface == NULL || iface->udev->hport == NULL) {
		return NULL;
	}
	alt = &iface->udev->hport->config.intf[iface->ifno].altsetting[0];
	if (index >= alt->intf_desc.bNumEndpoints ||
		index >= CONFIG_USBHOST_MAX_ENDPOINTS) {
		return NULL;
	}
	return (usb_endpoint_descriptor_t *) (void *) &alt->ep[index].ep_desc;
}

void usbd_interface2device_handle(struct usbd_interface *iface,
	struct usbd_device **dev) {
	*dev = iface->udev;
}

usbd_status usbd_device2interface_handle(struct usbd_device *dev,
	uint8_t ifindex, struct usbd_interface **iface) {
	if (dev == NULL || ifindex >= USBD_SHIFACE_MAX) {
		return USBD_INVAL;
	}
	*iface = &dev->ifaces[ifindex];
	return USBD_NORMAL_COMPLETION;
}

/* ------------------------------------------------------------------ */

usbd_status usbd_open_pipe(struct usbd_interface *iface, uint8_t address,
	uint8_t flags, struct usbd_pipe **pipep) {
	struct usbd_device *dev;
	struct usbd_pipe *pipe;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	uint8_t i;

	(void) flags; /* per-URB QH: exclusivity is inherent */

	if (iface == NULL) {
		return USBD_INVAL;
	}
	dev = iface->udev;

	pipe = wlan_kmalloc(sizeof(*pipe), M_WAITOK | M_ZERO, M_USB);
	if (pipe == NULL) {
		return USBD_NOMEM;
	}
	pipe->dev = dev;
	SLIST_INIT(&pipe->pending);

	id = usbd_get_interface_descriptor(iface);
	for (i = 0;; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL || (id != NULL && i > id->bNumEndpoints)) {
			ed = NULL;
			break;
		}
		if (ed->bEndpointAddress == address) {
			break;
		}
	}
	if (ed == NULL) {
		wlan_kfree(pipe, M_USB);
		return USBD_INVAL;
	}
	pipe->ed = *ed;
	pipe->cherry_ep = (struct usb_endpoint_descriptor *) (void *) ed;
	*pipep = pipe;
	return USBD_NORMAL_COMPLETION;
}

void usbd_close_pipe(struct usbd_pipe *pipe) {
	struct usbd_xfer *xfer;
	ipl_t ipl;

	if (pipe == NULL) {
		return;
	}
	ipl = ipl_save();
	SLIST_FOREACH(xfer, &pipe->pending, next) {
		if (xfer->in_flight) {
			(void) usbh_kill_urb(&xfer->urb);
		}
	}
	ipl_restore(ipl);
	wlan_kfree(pipe, M_USB);
}

void usbd_abort_pipe(struct usbd_pipe *pipe) {
	struct usbd_xfer *xfer;
	ipl_t ipl;

	if (pipe == NULL) {
		return;
	}
	ipl = ipl_save();
	SLIST_FOREACH(xfer, &pipe->pending, next) {
		if (xfer->in_flight) {
			/* thread context only (watchdog / stop paths) */
			(void) usbh_kill_urb(&xfer->urb);
		}
	}
	ipl_restore(ipl);
}

void usbd_clear_endpoint_stall_async(struct usbd_pipe *pipe) {
	/* the host-side clear-feature is handled on the cherryusb stall
	 * recovery path; returning lets the driver keep re-arming rx */
	(void) pipe;
}

/* ------------------------------------------------------------------ */

int usbd_create_xfer(struct usbd_pipe *pipe, size_t size, unsigned int flags,
	unsigned int nframes, struct usbd_xfer **xp) {
	struct usbd_xfer *xfer;

	(void) nframes;
	xfer = wlan_kmalloc(sizeof(*xfer), M_WAITOK | M_ZERO, M_USB);
	if (xfer == NULL) {
		return USBD_NOMEM;
	}
	xfer->pipe = pipe;
	xfer->flags = (uint16_t) flags;
	if (size != 0) {
		xfer->dma_buf = memalign(USBD_SHIM_ALIGN, size);
		if (xfer->dma_buf == NULL) {
			wlan_kfree(xfer, M_USB);
			return USBD_NOMEM;
		}
		memset(xfer->dma_buf, 0, size);
	}
	*xp = xfer;
	return USBD_NORMAL_COMPLETION;
}

void usbd_destroy_xfer(struct usbd_xfer *xfer) {
	if (xfer == NULL) {
		return;
	}
	if (xfer->in_flight) {
		(void) usbh_kill_urb(&xfer->urb);
	}
	if (xfer->dma_buf != NULL) {
		wlan_kfree(xfer->dma_buf, M_USB);
	}
	wlan_kfree(xfer, M_USB);
}

void *usbd_get_buffer(struct usbd_xfer *xfer) {
	return xfer->dma_buf;
}

void usbd_setup_xfer(struct usbd_xfer *xfer, void *priv, void *buffer,
	uint32_t length, uint16_t flags, uint32_t timeout, usbd_callback cb) {
	xfer->priv = priv;
	xfer->buffer = (buffer != NULL) ? buffer : xfer->dma_buf;
	xfer->length = length;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->callback = cb;
	xfer->status = USBD_NOT_STARTED;
	xfer->actlen = 0;
}

void usbd_get_xfer_status(struct usbd_xfer *xfer, void **priv, void **buffer,
	uint32_t *actlen, usbd_status *status) {
	if (priv != NULL) {
		*priv = xfer->priv;
	}
	if (buffer != NULL) {
		*buffer = xfer->buffer;
	}
	if (actlen != NULL) {
		*actlen = xfer->actlen;
	}
	if (status != NULL) {
		*status = xfer->status;
	}
}

/* ------------------------------------------------------------------ */
/* async transfer plumbing */

/* Trace verbosity, off by default. 1 logs async events (urb/taskq),
 * 2 additionally logs every control transfer. Errors always print. */
static unsigned wlan_trace_lvl;
/* budgeted async (task/urb) event prints, reset by wlan_usbdi_trace_reset() */
static unsigned wlan_async_trace_seq;

/* EHCI interrupt context: record the result and hand the xfer to the
 * worker; never touch the driver callback here */
static void usbd_shim_urb_complete(void *arg, int nbytes_or_err) {
	struct usbd_xfer *xfer = arg;
	struct usbd_device *dev = xfer->pipe != NULL ? xfer->pipe->dev : NULL;

	if (nbytes_or_err < 0) {
		xfer->status = usbd_map_err(nbytes_or_err);
		xfer->actlen = 0;
	} else {
		xfer->status = USBD_NORMAL_COMPLETION;
		xfer->actlen = (uint32_t) nbytes_or_err;
	}

	if (dev != NULL) {
		usbd_ring_post(dev, xfer);
	}
}

/* worker context: unlink from the pipe, then run the driver callback */
static void usbd_shim_urb_work(struct usbd_xfer *xfer) {
	usbd_callback cb;
	void *priv;
	usbd_status status;
	ipl_t ipl;

	if (wlan_trace_lvl >= 1 && wlan_async_trace_seq < 48) {
		printf("[wlan] urb done: status=%d actlen=%u\n",
		    (int) xfer->status, xfer->actlen);
	}

	xfer->in_flight = 0;
	if (xfer->pipe != NULL) {
		ipl = ipl_save();
		SLIST_REMOVE(&xfer->pipe->pending, xfer, usbd_xfer, next);
		ipl_restore(ipl);
	}

	cb = xfer->callback;
	priv = xfer->priv;
	status = xfer->status;
	if (cb != NULL) {
		cb(xfer, priv, status);
	}
}

usbd_status usbd_transfer(struct usbd_xfer *xfer) {
	struct usbd_device *dev;
	struct usbh_hubport *hport;
	ipl_t ipl;
	int ret;

	if (xfer->pipe == NULL) {
		return USBD_INVAL;
	}
	dev = xfer->pipe->dev;
	hport = dev->hport;
	if (hport == NULL) {
		return USBD_IOERROR;
	}
	shim_locks_init();

	memset(&xfer->urb, 0, sizeof(xfer->urb));
	usbh_bulk_urb_fill(&xfer->urb, hport, xfer->pipe->cherry_ep,
		xfer->buffer, xfer->length,
		0 /* timeout=0: asynchronous */, usbd_shim_urb_complete, xfer);

	xfer->status = USBD_IN_PROGRESS;
	xfer->actlen = 0;
	xfer->in_flight = 1;
	ipl = ipl_save();
	SLIST_INSERT_HEAD(&xfer->pipe->pending, xfer, next);
	ipl_restore(ipl);
	{
		static unsigned urb_subs;

		if (wlan_trace_lvl >= 1 && urb_subs < 48) {
			printf("[wlan] urb submit #%u: ep=%02x len=%u\n",
			    urb_subs, xfer->pipe->ed.bEndpointAddress,
			    xfer->length);
		}
		urb_subs++;
	}

	ret = usbh_submit_urb(&xfer->urb);
	if (ret != 0) {
		/* not connected / busy: synthesize the callback so the
		 * driver can reclaim its tx_data */
		xfer->in_flight = 0;
		xfer->status = usbd_map_err(ret);
		usbd_ring_post(dev, xfer);
	}
	return USBD_IN_PROGRESS;
}

/* ------------------------------------------------------------------ */
/* control transfers */

static usbd_status usbd_ctrl_xfer(struct usbd_device *dev,
	usb_device_request_t *req, void *data, int *actlen);
/* control-transfer trace counter, reset by wlan_usbdi_trace_reset() */
static unsigned wlan_ctrl_trace_seq;
void wlan_usbdi_trace_reset(void);
void wlan_usbdi_trace_set(unsigned level);

static usbd_status usbd_ctrl_xfer(struct usbd_device *dev,
	usb_device_request_t *req, void *data, int *actlen) {
	uint16_t len;
	int is_read;
	int ret;

	if (dev->hport == NULL) {
		return USBD_IOERROR;
	}
	shim_locks_init();
	mutex_enter(&s_ctrl_mtx);

	if (wlan_trace_lvl >= 2 && wlan_ctrl_trace_seq < 800) {
		printf("[wlan] ctrl #%u: type=%02x req=%02x val=%04x len=%u\n",
		    wlan_ctrl_trace_seq, req->bmRequestType, req->bRequest,
		    UGETW(req->wValue), UGETW(req->wLength));
	}
	wlan_ctrl_trace_seq++;

	s_ctrl.setup.bmRequestType = req->bmRequestType;
	s_ctrl.setup.bRequest = req->bRequest;
	s_ctrl.setup.wValue = (uint16_t) UGETW(req->wValue);
	s_ctrl.setup.wIndex = (uint16_t) UGETW(req->wIndex);
	len = (uint16_t) UGETW(req->wLength);
	s_ctrl.setup.wLength = len;

	is_read = (req->bmRequestType & 0x80U) != 0U;
	if (len > 0 && !is_read && data != NULL) {
		memcpy(s_ctrl.buf, data, len);
	}

	ret = usbh_control_transfer(dev->hport, &s_ctrl.setup, s_ctrl.buf);
	/* >= 0: actual length; < 0: cherryusb error code */
	if (ret < 0) {
		printf("[wlan] ctrl xfer failed: type=%02x req=%02x raw=%d\n",
		    req->bmRequestType, req->bRequest, ret);
		mutex_exit(&s_ctrl_mtx);
		return usbd_map_err(ret);
	}
	if (wlan_trace_lvl >= 2 && wlan_ctrl_trace_seq < 802) {
		printf("[wlan] ctrl #%u done: %d\n", wlan_ctrl_trace_seq - 1,
		    ret);
	}
	if (len > 0 && is_read && data != NULL) {
		uint16_t n = (len < (uint16_t) ret) ? len : (uint16_t) ret;

		memcpy(data, s_ctrl.buf, n);
	}
	if (actlen != NULL) {
		*actlen = ret;
	}

	mutex_exit(&s_ctrl_mtx);
	return USBD_NORMAL_COMPLETION;
}

usbd_status usbd_do_request(struct usbd_device *dev,
	usb_device_request_t *req, void *data) {
	return usbd_ctrl_xfer(dev, req, data, NULL);
}

usbd_status usbd_do_request_flags(struct usbd_device *dev,
	usb_device_request_t *req, void *data, uint16_t flags, int *actlen,
	uint32_t timeout) {
	(void) flags;
	(void) timeout; /* usbh_control_transfer: fixed 500 ms inside */
	return usbd_ctrl_xfer(dev, req, data, actlen);
}

usbd_status usbd_set_config_no(struct usbd_device *dev, int config,
	int flags) {
	usb_device_request_t req;
	uint8_t val = 0;
	usbd_status err;

	(void) flags;
	if (dev->hport == NULL) {
		return USBD_NOT_CONFIGURED;
	}
	/* cherryusb enumeration already issued SET_CONFIGURATION(1); read
	 * it back through the aligned bounce and only complain on drift */
	memset(&req, 0, sizeof(req));
	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_CONFIG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);
	err = usbd_do_request(dev, &req, &val);
	if (err != USBD_NORMAL_COMPLETION) {
		return err;
	}
	if (config != 0 && val != config) {
		printf("[wlan] shim: cfg %d != %d (enumerated), ignored\n",
		    val, config);
	}
	return USBD_NORMAL_COMPLETION;
}

/* ------------------------------------------------------------------ */
/* misc glue */

usbd_status usbd_delay_ms(struct usbd_device *dev, unsigned int ms) {
	(void) dev;
	ksleep(ms);
	return USBD_NORMAL_COMPLETION;
}

char *usbd_devinfo_alloc(struct usbd_device *dev, int showclass) {
	struct usbh_hubport *hport = dev->hport;
	const char *vend;
	const char *prod;
	char *buf;

	(void) showclass;
	vend = (hport && hport->iManufacturer) ? hport->iManufacturer : "Realtek";
	prod = (hport && hport->iProduct) ? hport->iProduct : "RTL8188EU";
	buf = wlan_kmalloc(128, M_WAITOK, M_USB);
	if (buf == NULL) {
		return NULL;
	}
	snprintf(buf, 128, "%s %s, addr %d", vend, prod,
	    hport ? hport->dev_addr : 0);
	return buf;
}

void usbd_devinfo_free(char *devinfop) {
	wlan_kfree(devinfop, M_USB);
}

void usbd_add_drv_event(int type, struct usbd_device *udev, device_t self) {
	(void) type;
	(void) udev;
	(void) self;
}

const char *usbd_errstr(usbd_status err) {
	switch (err) {
	case USBD_NORMAL_COMPLETION:
		return "no error";
	case USBD_IN_PROGRESS:
		return "io in progress";
	case USBD_NOT_STARTED:
		return "io not started";
	case USBD_INVAL:
		return "invalid argument";
	case USBD_NOMEM:
		return "out of memory";
	case USBD_CANCELLED:
		return "io cancelled";
	case USBD_IOERROR:
		return "usb io error";
	case USBD_NOT_CONFIGURED:
		return "device not configured";
	case USBD_STALLED:
		return "device stalled";
	case USBD_TIMEOUT:
		return "io timeout";
	default:
		return "unknown usb error";
	}
}

const struct usb_devno *usb_match_device(const struct usb_devno *tbl,
	u_int n, u_int entsize, uint16_t vendor, uint16_t product) {
	const struct usb_devno *ed = tbl;

	while (n-- != 0) {
		if (ed->ud_vendor == vendor &&
			(ed->ud_product == product ||
			ed->ud_product == USB_PRODUCT_ANY)) {
			return ed;
		}
		ed = (const struct usb_devno *)((const char *)ed + entsize);
	}
	return NULL;
}

/* wlan_cmd calls this right before if_init so the trace starts from
 * zero inside the driver init sequence */
void wlan_usbdi_trace_reset(void) {
	wlan_ctrl_trace_seq = 0;
	wlan_async_trace_seq = 0;
	wlan_trace_lvl = 0;
}

void wlan_usbdi_trace_set(unsigned level) {
	wlan_trace_lvl = level;
}

/* ------------------------------------------------------------------ */
/* usb task queues (NetBSD semantics: a queued task is not re-queued;
 * rem_task_wait blocks until the task has run or been removed) */

void usb_add_task(struct usbd_device *dev, struct usb_task *task,
	int queue) {
	ipl_t ipl;

	(void) queue;
	if (dev == NULL) {
		return;
	}

	ipl = ipl_save();
	if (task->queue != USB_NUM_TASKQS || dev->task_head >= USBD_SHIM_RING) {
		ipl_restore(ipl);
		return;
	}
	task->queue = (volatile unsigned) USB_TASKQ_DRIVER;
	dev->tasks[dev->task_head++] = task;
	ipl_restore(ipl);
	if (wlan_trace_lvl >= 1 && wlan_async_trace_seq < 48) {
		printf("[wlan] add_task fun=%p\n", task->fun);
		wlan_async_trace_seq++;
	}
	usb_osal_sem_give(dev->task_sem);
}

bool usb_rem_task(struct usbd_device *dev, struct usb_task *task) {
	ipl_t ipl;
	unsigned i;
	bool found = false;

	if (dev == NULL) {
		return false;
	}
	ipl = ipl_save();
	for (i = 0; i < dev->task_head; i++) {
		if (dev->tasks[i] == task) {
			dev->tasks[i] = NULL;
			task->queue = USB_NUM_TASKQS;
			found = true;
		}
	}
	ipl_restore(ipl);
	return found;
}

bool usb_rem_task_wait(struct usbd_device *dev, struct usb_task *task,
	int queue, kmutex_t *interlock) {
	(void) queue;
	(void) interlock; /* the detach call site passes NULL */

	if (dev == NULL) {
		return false;
	}
	usb_rem_task(dev, task);
	mutex_enter(&dev->wq_mtx);
	while (task->queue != USB_NUM_TASKQS) {
		cv_wait(&dev->wq_cv, &dev->wq_mtx);
	}
	mutex_exit(&dev->wq_mtx);
	return true;
}

bool usb_task_pending(struct usbd_device *dev, struct usb_task *task) {
	(void) dev;
	return task->queue != USB_NUM_TASKQS;
}

/* ------------------------------------------------------------------ */
/* workers */

static void *wlan_urb_worker_loop(void *arg) {
	struct usbd_device *dev = arg;

	while (dev->running) {
		struct usbd_xfer *xfer;
		ipl_t ipl;

		(void) usb_osal_sem_take(dev->ring_sem, USB_OSAL_WAITING_FOREVER);
		ipl = ipl_save();
		if (dev->ring_head == 0) {
			ipl_restore(ipl);
			continue;
		}
		xfer = dev->ring[0];
		memmove(&dev->ring[0], &dev->ring[1],
		    (dev->ring_head - 1) * sizeof(xfer));
		dev->ring_head--;
		ipl_restore(ipl);

		usbd_shim_urb_work(xfer);
	}
	return NULL;
}

static void *wlan_taskq_worker_loop(void *arg) {
	struct usbd_device *dev = arg;

	while (dev->running) {
		struct usb_task *task;
		ipl_t ipl;

		(void) usb_osal_sem_take(dev->task_sem, USB_OSAL_WAITING_FOREVER);
		ipl = ipl_save();
		if (dev->task_head == 0) {
			ipl_restore(ipl);
			continue;
		}
		task = dev->tasks[0];
		memmove(&dev->tasks[0], &dev->tasks[1],
		    (dev->task_head - 1) * sizeof(task));
		dev->task_head--;
		ipl_restore(ipl);

		if (wlan_trace_lvl >= 1 && wlan_async_trace_seq < 48) {
			printf("[wlan] taskq run fun=%p\n", task->fun);
		}
		task->fun(task->arg);
		if (wlan_trace_lvl >= 1 && wlan_async_trace_seq < 48) {
			printf("[wlan] taskq done fun=%p\n", task->fun);
		}

		mutex_enter(&dev->wq_mtx);
		task->queue = USB_NUM_TASKQS;
		cv_broadcast(&dev->wq_cv);
		mutex_exit(&dev->wq_mtx);
	}
	return NULL;
}

static void wlan_workers_start(struct usbd_device *dev) {
	if (dev->workers_started) {
		return;
	}
	dev->workers_started = 1;
	dev->running = 1;
	dev->ring_sem = usb_osal_sem_create(0);
	dev->task_sem = usb_osal_sem_create(0);
	mutex_init(&dev->wq_mtx, MUTEX_DEFAULT, IPL_USB);
	cv_init(&dev->wq_cv, "wlanurb");

	dev->urb_worker = wlan_port_thread_create(wlan_urb_worker_loop, dev);
	wlan_port_thread_start(dev->urb_worker);
	dev->taskq_worker = wlan_port_thread_create(wlan_taskq_worker_loop, dev);
	wlan_port_thread_start(dev->taskq_worker);
}

/* ------------------------------------------------------------------ */
/* attach: called by the cherryusb class hook when the dongle appears */

int wlan_usbdi_attach(struct wlan_usb_dev *port,
	const struct wlan_chip_driver *drv) {
	struct usbd_device *dev;
	struct usbh_hubport *hport = port->env_dev;

	dev = usbd_shim_register_device(hport, port);
	if (dev == NULL) {
		return -ENOMEM;
	}
	port->port_priv = dev;

	wlan_workers_start(dev);

	return drv->attach(port, NULL);
}

void wlan_usbdi_detach(struct wlan_usb_dev *port) {
	struct usbd_device *dev = port->port_priv;

	if (dev == NULL) {
		return;
	}
	dev->running = 0;
	usb_osal_sem_give(dev->ring_sem);
	usb_osal_sem_give(dev->task_sem);
	port->port_priv = NULL;
	wlan_kfree(dev, M_USB);
}
