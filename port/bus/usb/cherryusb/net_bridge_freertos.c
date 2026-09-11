/*
 * @file
 * @brief Port lifecycle: driver registry and the FreeRTOS entry point.
 *
 * wlan_port_init() starts the CherryUSB host stack on the panel EHCI
 * controller; enumeration happens on the cherryusb hub thread, which
 * claims matching devices through the class hook in
 * usbh_urtwn_class.c.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include <usbh_core.h>

#include <port/port.h>
#include <port/osal/freertos/wlan_port_freertos.h>
#include "wlan_port_cherryusb.h"

extern const struct wlan_chip_driver urtwn_driver;

const struct wlan_chip_driver *const wlan_chip_drivers[] = {
	&urtwn_driver,
	NULL
};

struct wlan_port_iface *wlan_port_ifs[WLAN_PORT_MAX_IF];
int wlan_port_if_n;

/* worker-thread plumbing for the usbdi shim: create suspended, then
 * start on demand (the shim relies on the embox semantics). The shim's
 * pthread-style entry has the same ABI as a FreeRTOS task on this
 * target, so the pointer is used directly. */
void *wlan_port_thread_create(void *(*run)(void *), void *arg) {
	TaskHandle_t task = NULL;

	if (xTaskCreate((TaskFunction_t) run, "wlan_wrk", 4096,
	    arg, 4, &task) != pdPASS) {
		return NULL;
	}
	vTaskSuspend(task);
	return (void *) task;
}

void wlan_port_thread_start(void *thread) {
	vTaskResume((TaskHandle_t) thread);
}

int wlan_port_init(void) {
	wlan_osal_freertos_init();

	return usbh_initialize(WLAN_CHERRYUSB_EHCI_BUSID,
	    WLAN_CHERRYUSB_EHCI_BASE, NULL);
}

void wlan_port_deinit(void) {
	usbh_deinitialize(WLAN_CHERRYUSB_EHCI_BUSID);
}
