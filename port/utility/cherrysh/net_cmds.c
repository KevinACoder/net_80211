/*
 * @file
 * @brief CherrySH lwIP commands: netif status, static IP and ICMP ping.
 *
 * Ping uses the raw API under the core lock: the socket API queues
 * through the tcpip mailbox, which the wireless broadcast flood keeps
 * saturated and would wedge the console.
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
#include "lwip/netifapi.h"
#include "lwip/raw.h"
#include "lwip/dhcp.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/icmp.h"
#include "lwip/tcpip.h"

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
	console_printf("wl0 ip %s\r\n", ip4addr_ntoa(netif_ip4_addr(n)));
	console_printf("wl0 gw %s\r\n", ip4addr_ntoa(netif_ip4_gw(n)));
	return 0;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_netif, netif, "wlan netif status", "netif\r\n");

/* static address: the bench AP bridges to the LAN but its DHCP does
 * not answer wireless clients (embox lane drives it statically too) */
static int cmd_ip(int argc, char **argv) {
	ip4_addr_t ip, mask, gw;

	if (argc != 4) {
		console_printf("usage: ip <addr> <mask> <gw>\r\n");
		return 1;
	}
	if (ip4addr_aton(argv[1], &ip) != 1 ||
	    ip4addr_aton(argv[2], &mask) != 1 ||
	    ip4addr_aton(argv[3], &gw) != 1) {
		console_printf("ip: bad address\r\n");
		return 1;
	}
	/* the DHCP started on ASSOC would overwrite the static address
	 * when its lease comes back; stop it first */
	wlan_lwip_set_dhcp(0);
	netifapi_netif_set_addr(wlan_lwip_get_netif(), &ip, &mask, &gw);
	console_printf("ip: set %s mask %s gw %s\r\n", argv[1], argv[2],
	    argv[3]);
	return 0;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_ip, ip, "set static ipv4",
	"ip <addr> <mask> <gw>\r\n");

/* ---- ping over the raw API ---- */

#define PING_DATALEN 32

struct ping_ctx {
	struct raw_pcb *pcb;
	ip4_addr_t peer;
	uint16_t ident;
	volatile int got_reply;
};

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

static u8_t ping_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p,
    const ip4_addr_t *addr) {
	struct ping_ctx *ctx = arg;
	const struct icmp_echo_hdr *icmp;

	(void) pcb;
	(void) addr;
	/* the pbuf belongs to raw_input: never freed here */
	if (p->tot_len < IP_HLEN + sizeof(struct icmp_echo_hdr)) {
		return 0;
	}
	/* raw_input keeps the payload at the IP header */
	icmp = (const struct icmp_echo_hdr *)
	    ((const uint8_t *) p->payload + IP_HLEN);
	if (ICMPH_TYPE(icmp) == ICMP_ER &&
	    ntohs(icmp->id) == ctx->ident) {
		ctx->got_reply = 1;
		return 1; /* consumed */
	}
	return 0;
}

static int ping_one(struct ping_ctx *ctx) {
	struct pbuf *p;
	struct icmp_echo_hdr *icmp;
	int rep;

	p = pbuf_alloc(PBUF_IP, (u16_t) (sizeof(*icmp) + PING_DATALEN),
	    PBUF_RAM);
	if (p == NULL) {
		return -1;
	}
	icmp = (struct icmp_echo_hdr *) p->payload;
	ICMPH_TYPE_SET(icmp, ICMP_ECHO);
	ICMPH_CODE_SET(icmp, 0);
	icmp->chksum = 0;
	icmp->id = htons(ctx->ident);
	icmp->seqno = htons(1);
	icmp->chksum = icmp_checksum((const uint8_t *) p->payload,
	    sizeof(*icmp) + PING_DATALEN);

	ctx->got_reply = 0;
	LOCK_TCPIP_CORE();
	raw_sendto(ctx->pcb, p, &ctx->peer);
	UNLOCK_TCPIP_CORE();
	pbuf_free(p);

	/* the reply lands through the tcpip thread (rx -> tcpip_input ->
	 * raw_input -> ping_recv_cb); just poll the flag */
	for (rep = 0; rep < 40 && !ctx->got_reply; rep++) {
		sys_msleep(25);
	}

	return ctx->got_reply ? 0 : -1;
}

static int cmd_ping(int argc, char **argv) {
	ip4_addr_t addr;
	struct ping_ctx ctx;
	int ok = 0;
	int i;

	if (argc < 2) {
		console_printf("usage: ping <ip>\r\n");
		return 1;
	}
	if (ip4addr_aton(argv[1], &addr) != 1) {
		console_printf("ping: bad address %s\r\n", argv[1]);
		return 1;
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.peer = addr;
	ctx.ident = (uint16_t) (sys_now() & 0xffff);

	LOCK_TCPIP_CORE();
	ctx.pcb = raw_new(IP_PROTO_ICMP);
	if (ctx.pcb == NULL) {
		UNLOCK_TCPIP_CORE();
		console_printf("ping: no raw pcb\r\n");
		return 1;
	}
	raw_bind(ctx.pcb, IP_ADDR_ANY);
	raw_recv(ctx.pcb, ping_recv_cb, &ctx);
	UNLOCK_TCPIP_CORE();

	for (i = 0; i < 4; i++) {
		if (ping_one(&ctx) == 0) {
			console_printf("ping: seq %d reply\r\n", i + 1);
			ok++;
		} else {
			console_printf("ping: seq %d timeout\r\n", i + 1);
		}
	}

	LOCK_TCPIP_CORE();
	raw_remove(ctx.pcb);
	UNLOCK_TCPIP_CORE();

	console_printf("ping %s: %d/4 replies\r\n", argv[1], ok);
	return (ok > 0) ? 0 : 1;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_ping, ping, "ICMP echo", "ping <ip>\r\n");
