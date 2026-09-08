/*
 * @file
 * @brief Firmware resolution over embedded blobs.
 *
 * The embox template has no file system mount for firmware; the blobs
 * under firmware/ are generated into C arrays and resolved by name.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <string.h>

#include <port/port.h>

#include "rtl8188eufw_gen.c"

struct embedded_firmware {
	const char *ef_name;
	const uint8_t *ef_data;
	size_t ef_size;
};

static const struct embedded_firmware wlan_firmwares[] = {
	{ "rtl8188eufw.bin", rtl8188eufw_data, rtl8188eufw_size },
	{ NULL, NULL, 0 }
};

int wlan_port_firmware_get(const char *name, struct wlan_firmware *fw) {
	const struct embedded_firmware *ef;

	for (ef = wlan_firmwares; ef->ef_name != NULL; ef++) {
		if (strcmp(ef->ef_name, name) == 0) {
			fw->data = ef->ef_data;
			fw->size = ef->ef_size;
			return 0;
		}
	}
	return -1;
}

/* ------------------------------------------------------------------
 * NetBSD firmload(9) API over the embedded blobs.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dev/firmload.h>

/* The resolved descriptor behind a firmware_handle_t. The NetBSD
 * firmload(9) contract hands the driver an opaque handle that stays
 * valid until firmware_close(); a single firmware image is in flight at
 * a time (urtwn_load_firmware holds sc_write_mtx for the whole load). */
static struct wlan_firmware wlan_fw_desc;

int firmware_open(const char *drv, const char *img, firmware_handle_t *h) {
	char name[64];

	(void) drv; /* the blobs are keyed by the bare image name */
	snprintf(name, sizeof(name), "%s", img);
	if (wlan_port_firmware_get(name, &wlan_fw_desc) != 0) {
		return -1;
	}
	*h = &wlan_fw_desc;
	return 0;
}

size_t firmware_get_size(firmware_handle_t h) {
	return h->size;
}

void *firmware_malloc(size_t len) {
	return malloc(len);
}

int firmware_read(firmware_handle_t h, size_t offset, void *buf, size_t len) {
	if (offset + len > h->size) {
		return -1;
	}
	memcpy(buf, h->data + offset, len);
	return 0;
}

void firmware_close(firmware_handle_t h) {
	(void) h;
}

void firmware_free(void *buf, size_t len) {
	(void) len;
	free(buf);
}
