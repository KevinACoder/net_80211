/*
 * @file
 * @brief Bus-neutral port core: adapter registry and dispatch.
 *
 * Driver adapters register their control/diagnostic hooks here at
 * attach time; the shell-facing calls (up/scan/xmit/hwaddr/dumps)
 * dispatch onto the first adapter that registered. This keeps the
 * command layer free of per-driver symbols.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdio.h>
#include <string.h>

#include <port/port.h>

#define WLAN_PORT_ADAPTER_MAX 2

static const struct wlan_port_adapter *
	wlan_port_adapters[WLAN_PORT_ADAPTER_MAX];

void wlan_port_adapter_register(const struct wlan_port_adapter *adapter) {
	for (int i = 0; i < WLAN_PORT_ADAPTER_MAX; i++) {
		if (wlan_port_adapters[i] == NULL) {
			wlan_port_adapters[i] = adapter;
			return;
		}
	}
}

/* Explicit selection, or the first attached adapter by default. */
static const struct wlan_port_adapter *wlan_port_active;

static const struct wlan_port_adapter *wlan_port_adapter_first(void) {
	if (wlan_port_active != NULL) {
		return wlan_port_active;
	}
	for (int i = 0; i < WLAN_PORT_ADAPTER_MAX; i++) {
		if (wlan_port_adapters[i] != NULL) {
			return wlan_port_adapters[i];
		}
	}
	return NULL;
}

void *wlan_port_get_ic(void) {
	const struct wlan_port_adapter *ad = wlan_port_adapter_first();

	return ad != NULL ? ad->ic : NULL;
}

int wlan_port_select(const char *name) {
	for (int i = 0; i < WLAN_PORT_ADAPTER_MAX; i++) {
		if (wlan_port_adapters[i] != NULL &&
		    strcmp(wlan_port_adapters[i]->name, name) == 0) {
			wlan_port_active = wlan_port_adapters[i];
			return 0;
		}
	}
	return -1;
}

int wlan_port_up(void) {
	const struct wlan_port_adapter *ad = wlan_port_adapter_first();

	if (ad == NULL || ad->up == NULL) {
		return -1;
	}
	return ad->up();
}

int wlan_port_scan(const uint8_t *ssid, size_t len) {
	const struct wlan_port_adapter *ad = wlan_port_adapter_first();

	if (ad == NULL || ad->scan == NULL) {
		return -1;
	}
	return ad->scan(ssid, len);
}

int wlan_port_xmit(const uint8_t *frame, size_t len) {
	const struct wlan_port_adapter *ad = wlan_port_adapter_first();

	if (ad == NULL || ad->xmit == NULL) {
		return -1;
	}
	return ad->xmit(frame, len);
}

int wlan_port_get_hwaddr(uint8_t addr[6]) {
	const struct wlan_port_adapter *ad = wlan_port_adapter_first();

	if (ad == NULL || ad->get_hwaddr == NULL) {
		return -1;
	}
	return ad->get_hwaddr(addr);
}

void wlan_port_status_dump(void) {
	const struct wlan_port_adapter *ad = wlan_port_adapter_first();

	if (ad != NULL && ad->status_dump != NULL) {
		ad->status_dump();
	}
	else {
		printf("wlan: no adapter\n");
	}
}

void wlan_port_scan_dump(void) {
	const struct wlan_port_adapter *ad = wlan_port_adapter_first();

	if (ad != NULL && ad->scan_dump != NULL) {
		ad->scan_dump();
	}
}
