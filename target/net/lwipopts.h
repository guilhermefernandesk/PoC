#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/*
 * LwIP options for the Modbus TCP PoC.
 *
 * The Modbus transport uses the netconn API. Sockets, DHCP, DNS and IPv6 are
 * intentionally disabled to keep the target small and deterministic.
 */

#define NO_SYS 0
#define LWIP_SOCKET 0
#define LWIP_NETCONN 1

#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_DHCP 0
#define LWIP_AUTOIP 0
#define LWIP_DNS 0

#define LWIP_NETIF_HOSTNAME 0
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1

/* Required by ports/lwip/porttcp.c. */
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_SNDTIMEO 0

/* Keep the MAC simple: LwIP computes checksums in software. */
#define CHECKSUM_BY_HARDWARE 0
#define LWIP_CHECKSUM_CTRL_PER_NETIF 0

#define MEM_ALIGNMENT 4
#define MEM_SIZE (64u * 1024u)
#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_PCB_LISTEN 4
#define MEMP_NUM_TCP_SEG 24
#define MEMP_NUM_PBUF 16
#define MEMP_NUM_NETBUF 8
#define MEMP_NUM_NETCONN 8
#define MEMP_NUM_UDP_PCB 4
#define MEMP_NUM_TCPIP_MSG_API 8
#define MEMP_NUM_TCPIP_MSG_INPKT 16

#define PBUF_POOL_SIZE 16
#define PBUF_POOL_BUFSIZE 592
#define PBUF_LINK_HLEN 14
#define ETH_PAD_SIZE 0

#define TCP_MSS 1460
#define TCP_WND (4u * TCP_MSS)
#define TCP_SND_BUF (4u * TCP_MSS)
#define TCP_SND_QUEUELEN 16
#define TCP_QUEUE_OOSEQ 1
#define TCP_OVERSIZE TCP_MSS

#define TCPIP_THREAD_NAME "tcpip"
#define TCPIP_THREAD_STACKSIZE 2048
#define TCPIP_THREAD_PRIO 3
#define TCPIP_MBOX_SIZE 8
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_TCP_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 8

/* The LwIP FreeRTOS contrib port interprets the thread stack size as bytes
 * unless this is set. Keep all stack sizes in the project in words. */
#define LWIP_FREERTOS_THREAD_STACKSIZE_IS_STACKWORDS 1

#define LWIP_TCPIP_CORE_LOCKING 0
#define SYS_LIGHTWEIGHT_PROT 1

#define LWIP_STATS 0
#define LWIP_DEBUG 0

#endif
