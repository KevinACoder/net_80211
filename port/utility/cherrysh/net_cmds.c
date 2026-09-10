/*
 * @file
 * @brief CherrySH lwIP commands: netif status and ICMP ping.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdio.h>
#include <string.h>

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/icmp.h"

#include "csh.h"
#include "csh_console.h"
#include "lwip_netif.h"

static int cmd_netif(int argc, char **argv) {
	struct netif *n = wlan_lwip_get_netif();

	(void) argc;
	(void) argv;
	if (n == NULL) {
		console_printf("netif: not registered\r\n");
		return 1;
	}
	console_printf("wl0 hwaddr %02x:%02x:%02x:%02x:%02x:%02x %s\r\n",
	    n->hwaddr[0], n->hwaddr[1], n->hwaddr[2],
	    n->hwaddr[3], n->hwaddr[4], n->hwaddr[5],
	    netif_is_link_up(n) ? "link-up" : "link-down");
	if (netif_is_link_up(n)) {
		console_printf("wl0 ip %s\r\n", ip4addr_ntoa(netif_ip4_addr(n)));
		console_printf("wl0 gw %s\r\n", ip4addr_ntoa(netif_ip4_gw(n)));
	}
	return 0;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_netif, netif, "wlan netif status", "netif\r\n");

static uint16_t icmp_checksum(const uint8_t *p, size_t len) {
	uint32_t sum = 0;
	size_t i;

	for (i = 0; i + 1 < len; i += 2) {
		sum += (uint32_t) (p[i] << 8) | p[i + 1];
	}
	if (i < len) {
		sum += (uint32_t) p[i] << 8;
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (uint16_t) ~sum;
}

static int ping_one(uint32_t ipaddr_be) {
	static uint8_t req[64];
	static uint8_t resp[128];
	struct icmp_echo_hdr *icmp = (struct icmp_echo_hdr *) req;
	struct sockaddr_in to;
	uint16_t ident = (uint16_t) (sys_now() & 0xffff);
	int s;
	int i;
	int ok = 0;

	memset(req, 0, sizeof(req));
	ICMPH_TYPE_SET(icmp, ICMP_ECHO);
	ICMPH_CODE_SET(icmp, 0);
	icmp->chksum = 0;
	icmp->id = htons(ident);
	icmp->seqno = htons(1);
	icmp->chksum = icmp_checksum(req, sizeof(req));

	s = socket(AF_INET, SOCK_RAW, (int) IP_PROTO_ICMP);
	if (s < 0) {
		console_printf("ping: socket failed\r\n");
		return 0;
	}

	memset(&to, 0, sizeof(to));
	to.sin_len = sizeof(to);
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = ipaddr_be;

	for (i = 0; i < 4; i++) {
		uint32_t t0 = sys_now();
		struct sockaddr_in from;
		socklen_t fromlen = sizeof(from);
		int timeout_ms = 1000;
		int n;

		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout_ms,
		    sizeof(timeout_ms));
		if (sendto(s, req, sizeof(req), 0,
		    (struct sockaddr *) &to, sizeof(to)) < 0) {
			continue;
		}
		n = recvfrom(s, resp, sizeof(resp), 0,
		    (struct sockaddr *) &from, &fromlen);
		if (n >= (int) sizeof(struct icmp_echo_hdr)) {
			struct icmp_echo_hdr *ricmp =
			    (struct icmp_echo_hdr *) resp;

			if (ICMPH_TYPE(ricmp) == ICMP_ER &&
			    ricmp->id == icmp->id) {
				console_printf("ping: seq %d time %ums\r\n",
				    i + 1, sys_now() - t0);
				ok++;
			}
		}
		sys_msleep(200);
	}
	closesocket(s);

	return ok;
}

static int cmd_ping(int argc, char **argv) {
	ip4_addr_t addr;
	int ok;

	if (argc < 2) {
		console_printf("usage: ping <ip>\r\n");
		return 1;
	}
	if (ip4addr_aton(argv[1], &addr) != 1) {
		console_printf("ping: bad address %s\r\n", argv[1]);
		return 1;
	}
	ok = ping_one(addr.addr);
	console_printf("ping %s: %d/4 replies\r\n", argv[1], ok);

	return (ok > 0) ? 0 : 1;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_ping, ping, "ICMP echo", "ping <ip>\r\n");
