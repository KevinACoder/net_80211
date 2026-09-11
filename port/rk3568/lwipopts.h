/*
 * @file lwipopts.h
 * @brief lwIP 2.2.1 configuration for the net_80211 FreeRTOS test system.
 *
 * Full tcpip-thread mode (NO_SYS=0) with the upstream contrib/ports/
 * freertos sys_arch; DHCP for the wlan netif, ICMP for the shell ping.
 *
 * @author zhugengyu
 * @date 10.09.2026
 */

#ifndef LWIPOPTS_H_
#define LWIPOPTS_H_

/* ---- system ---- */
#define NO_SYS 0
#define LWIP_SOCKET 1
#define LWIP_NETCONN 1
#define LWIP_NETIF_API 1
#define LWIP_TCPIP_CORE_LOCKING 1
#define LWIP_ERRNO_STDINCLUDE 1
#define LWIP_TIMEVAL_PRIVATE 0

/* ---- memory ---- */
#define MEM_SIZE (8 * 1024 * 1024)
#define MEMP_MEM_MALLOC 0
#define MEM_ALIGNMENT 64
#define PBUF_POOL_SIZE 96
#define PBUF_POOL_BUFSIZE 1700
#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_SEG 128
#define TCP_SND_QUEUELEN 128
#define MEMP_NUM_UDP_PCB 8
#define MEMP_NUM_TCPIP_MSG_INPKT 64
#define MEMP_NUM_TCPIP_MSG_API 32

/* ---- protocols ---- */
#define LWIP_ARP 1
#define LWIP_ICMP 1
#define LWIP_IGMP 0
#define LWIP_DNS 0
#define LWIP_RAW 1
#define MEMP_NUM_RAW_PCB 4
#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_TCP 1
#define LWIP_UDP 1
#define TCP_MSS 1460
#define TCP_WND (8 * TCP_MSS)
#define TCP_SND_BUF (8 * TCP_MSS)

/* ---- threads / mailboxes (defaults are 0 and assert in sys_arch) ---- */
#define TCPIP_THREAD_STACKSIZE 4096
#define TCPIP_THREAD_PRIO 24
#define TCPIP_MBOX_SIZE 64
#define DEFAULT_THREAD_STACKSIZE 2048
#define DEFAULT_THREAD_PRIO 8
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_TCP_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 4
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_RCVBUF 1

/* ---- dhcp ---- */
#define LWIP_DHCP 1
#define DHCP_DOES_ARP_CHECK 0

/* ---- checksums (software) ---- */
#define LWIP_CHECKSUM_CTRL_PER_NETIF 1

/* ---- misc ---- */
#define LWIP_NETIF_STATUS_CALLBACK 0
#define LWIP_STATS 0
#define LWIP_DEBUG 0

#endif /* LWIPOPTS_H_ */
