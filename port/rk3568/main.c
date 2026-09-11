/*
 * @file main.c
 * @brief net_80211 FreeRTOS test system entry (RK3568 ITX).
 *
 * Runs after the standalone-style startup has MMU, GIC and UART2 printf
 * in place. Brings up the CherrySH console and a heartbeat task, then
 * starts the scheduler.
 *
 * @author zhugengyu
 * @date 10.09.2026
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/tcpip.h"

#include "csh_console.h"
#include "lwip_netif.h"

extern int wlan_port_init(void); /* bus/usb cherryusb bridge */
extern int wlan_pcie_port_init(void); /* bus/pcie freertos backend */

static void app_task(void *param)
{
	(void)param;

	/* lwIP before the host stack: the rx hooks must be in place
	 * before enumeration can attach an adapter */
	tcpip_init(NULL, pdTRUE);
	if (wlan_lwip_init() != 0) {
		console_printf("wlan: lwip netif failed\r\n");
	}
	if (wlan_port_init() != 0) {
		console_printf("wlan: usb host failed\r\n");
	}
	console_printf("wlan: usb host up, waiting for adapter\r\n");
	wlan_pcie_port_init();
	vTaskDelete(NULL);
}

static void uptime_task(void *param)
{
	uint32_t seconds = 0;

	(void)param;

	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(10000));
		seconds += 10;
		printf("uptime %us\r\n", seconds);
	}
}

int main(void)
{
	printf("\r\nnet_80211 freertos test system (rk3568 itx)\r\n");

	if (console_start() != pdPASS) {
		printf("console_start failed\r\n");
	}

	xTaskCreate(app_task, "app", 4096, NULL, 2, NULL);
	xTaskCreate(uptime_task, "uptime", 512, NULL, 2, NULL);

	vTaskStartScheduler();

	for (;;) {
	}
}
