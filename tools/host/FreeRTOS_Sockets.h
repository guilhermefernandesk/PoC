/* Mock minimo de FreeRTOS_Sockets.h para testar mbtcp.c no host. */
#ifndef FREERTOS_SOCKETS_H
#define FREERTOS_SOCKETS_H

#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"

typedef int Socket_t;

#define FREERTOS_INVALID_SOCKET (-1)
#define FREERTOS_AF_INET        2
#define FREERTOS_SOCK_STREAM    1
#define FREERTOS_IPPROTO_TCP    6
#define FREERTOS_SHUT_RDWR      2

#define FREERTOS_SO_RCVTIMEO    1
#define FREERTOS_SO_SNDTIMEO    2
#define FREERTOS_SO_RCVBUF      3
#define FREERTOS_SO_SNDBUF      4
#define FREERTOS_SO_TCPNODELAY  5

struct freertos_sockaddr {
    uint8_t  sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
};

Socket_t FreeRTOS_socket(BaseType_t family, BaseType_t type, BaseType_t proto);
int32_t FreeRTOS_setsockopt(Socket_t sock, int32_t level, int32_t opt,
                            const void *value, size_t len);
uint16_t FreeRTOS_htons(uint16_t v);
int32_t FreeRTOS_connect(Socket_t sock,
                         const struct freertos_sockaddr *addr, size_t len);
int32_t FreeRTOS_shutdown(Socket_t sock, int32_t how);
int32_t FreeRTOS_closesocket(Socket_t sock);
int32_t FreeRTOS_recv(Socket_t sock, void *buf, size_t len, int32_t flags);
int32_t FreeRTOS_send(Socket_t sock, const void *buf, size_t len,
                      int32_t flags);

#endif
