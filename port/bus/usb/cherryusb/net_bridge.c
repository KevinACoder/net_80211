/*
 * @file
 * @brief Port lifecycle: driver registry and the embox entry point.
 *
 * Unit init starts the CherryUSB host stack on the panel EHCI
 * controller; enumeration happens on the cherryusb hub thread, which
 * claims matching devices through the class hook in
 * usbh_urtwn_class.c.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <embox/unit.h>

#include <kernel/thread.h>
#include <kernel/task.h>
#include <kernel/task/kernel_task.h>
#include <util/err.h>

#include <usbh_core.h>

#include <port/port.h>
#include <port/osal/embox/wlan_port_embox.h>
#include "wlan_port_cherryusb.h"

extern const struct wlan_chip_driver urtwn_driver;

const struct wlan_chip_driver *const wlan_chip_drivers[] = {
	&urtwn_driver,
	NULL
};

struct wlan_port_iface *wlan_port_ifs[WLAN_PORT_MAX_IF];
int wlan_port_if_n;

/* embox thread plumbing for the usbdi shim workers */
void *wlan_port_thread_create(void *(*run)(void *), void *arg) {
	struct thread *t = thread_create(THREAD_FLAG_NOTASK | THREAD_FLAG_SUSPENDED,
	    run, arg);
	if (ptr2err(t)) {
		return NULL;
	}
	task_thread_register(task_kernel_task(), t);
	thread_detach(t);
	return t;
}

void wlan_port_thread_start(void *thread) {
	thread_launch((struct thread *) thread);
}

static int wlan_port_init(void) {
	return usbh_initialize(WLAN_CHERRYUSB_EHCI_BUSID,
	    WLAN_CHERRYUSB_EHCI_BASE, NULL);
}

EMBOX_UNIT_INIT(wlan_port_init);

void wlan_port_deinit(void) {
	usbh_deinitialize(WLAN_CHERRYUSB_EHCI_BUSID);
}
