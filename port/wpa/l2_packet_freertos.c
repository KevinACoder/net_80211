/*
 * @file
 * @brief l2_packet for the net80211 port: EAPOL TX through the port
 * xmit hook.
 *
 * RX is not routed through this file; the USB worker delivers EAPOL
 * frames as EVENT_EAPOL_RX via the port hooks (the l2_packet rx
 * callback is kept for API compatibility but never fires).
 *
 * @date 11.09.2026
 * @author zhugengyu
 */

#include <includes.h>

/* the compat BSD world defines malloc/free as kalloc macros; this
 * unit allocates through the hostap/newlib wrappers instead */
#undef free
#undef malloc
#undef realloc

#include "utils/common.h"
#include "l2_packet/l2_packet.h"

#include <port/port.h>

struct l2_packet_data {
	u8 own_addr[ETH_ALEN];
	void (*rx_callback)(void *ctx, const u8 *src_addr, const u8 *buf,
	    size_t len);
	void *rx_callback_ctx;
};

struct l2_packet_data *l2_packet_init(const char *ifname,
    const u8 *own_addr, unsigned short protocol,
    void (*rx_callback)(void *ctx, const u8 *src_addr, const u8 *buf,
	size_t len),
    void *rx_callback_ctx, int l2_hdr) {
	struct l2_packet_data *l2;

	(void) ifname;
	(void) protocol;
	(void) l2_hdr;

	l2 = os_zalloc(sizeof(*l2));
	if (l2 == NULL) {
		return NULL;
	}
	if (own_addr != NULL) {
		os_memcpy(l2->own_addr, own_addr, ETH_ALEN);
	} else if (wlan_port_get_hwaddr(l2->own_addr) != 0) {
		os_free(l2);
		return NULL;
	}
	l2->rx_callback = rx_callback;
	l2->rx_callback_ctx = rx_callback_ctx;
	return l2;
}

void l2_packet_deinit(struct l2_packet_data *l2) {
	os_free(l2);
}

int l2_packet_get_own_addr(struct l2_packet_data *l2, u8 *addr) {
	os_memcpy(addr, l2->own_addr, ETH_ALEN);
	return 0;
}

int l2_packet_send(struct l2_packet_data *l2, const u8 *dst_addr,
    u16 proto, const u8 *buf, size_t len) {
	u8 frame[1600];

	if (dst_addr == NULL || buf == NULL ||
	    len + 2 * ETH_ALEN + 2 > sizeof(frame)) {
		return -1;
	}
	os_memcpy(frame, dst_addr, ETH_ALEN);
	os_memcpy(frame + ETH_ALEN, l2->own_addr, ETH_ALEN);
	frame[2 * ETH_ALEN] = (u8) (proto >> 8);
	frame[2 * ETH_ALEN + 1] = (u8) proto;
	os_memcpy(frame + 2 * ETH_ALEN + 2, buf, len);

	return wlan_port_xmit(frame, len + 2 * ETH_ALEN + 2) >= 0 ?
	    (int) len : -1;
}

int l2_packet_get_ip_addr(struct l2_packet_data *l2, char *buf,
    size_t len) {
	(void) l2;
	(void) buf;
	(void) len;
	return -1;
}

void l2_packet_notify_auth_start(struct l2_packet_data *l2) {
	(void) l2;
}

int l2_packet_set_packet_filter(struct l2_packet_data *l2,
    enum l2_packet_filter_type type) {
	(void) l2;
	(void) type;
	return 0;
}
