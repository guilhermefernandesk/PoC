/*
 * freertos_mock.c - FreeRTOS-Plus-TCP sobre sockets POSIX, so para testes.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "FreeRTOS_Sockets.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    pthread_mutex_t m;
} mock_mutex_t;

size_t xPortGetFreeHeapSize(void)
{
    return 0u;
}

TickType_t xTaskGetTickCount(void)
{
    struct timespec ts;

    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (TickType_t)((uint64_t)ts.tv_sec * 1000u +
                        (uint64_t)ts.tv_nsec / 1000000u);
}

void vTaskDelay(TickType_t ticks)
{
    struct timespec ts;

    ts.tv_sec = (time_t)(ticks / 1000u);
    ts.tv_nsec = (long)((ticks % 1000u) * 1000000u);
    (void)nanosleep(&ts, NULL);
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    mock_mutex_t *m = (mock_mutex_t *)malloc(sizeof(*m));

    if (m == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&m->m, NULL) != 0) {
        free(m);
        return NULL;
    }
    return (SemaphoreHandle_t)m;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout)
{
    mock_mutex_t *m = (mock_mutex_t *)sem;

    (void)timeout;
    if (m == NULL) {
        return pdFALSE;
    }
    return (pthread_mutex_lock(&m->m) == 0) ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem)
{
    mock_mutex_t *m = (mock_mutex_t *)sem;

    if (m == NULL) {
        return pdFALSE;
    }
    return (pthread_mutex_unlock(&m->m) == 0) ? pdTRUE : pdFALSE;
}

Socket_t FreeRTOS_socket(BaseType_t family, BaseType_t type, BaseType_t proto)
{
    (void)family;
    (void)type;
    (void)proto;
    return socket(AF_INET, SOCK_STREAM, 0);
}

int32_t FreeRTOS_setsockopt(Socket_t sock, int32_t level, int32_t opt,
                            const void *value, size_t len)
{
    (void)level;
    (void)len;

    if ((sock < 0) || (value == NULL)) {
        return -1;
    }

    switch (opt) {
    case FREERTOS_SO_RCVTIMEO:
    case FREERTOS_SO_SNDTIMEO: {
        TickType_t ms = *(const TickType_t *)value;
        struct timeval tv;

        tv.tv_sec = (time_t)(ms / 1000u);
        tv.tv_usec = (suseconds_t)((ms % 1000u) * 1000u);
        return setsockopt(sock, SOL_SOCKET,
                          (opt == FREERTOS_SO_RCVTIMEO) ? SO_RCVTIMEO
                                                        : SO_SNDTIMEO,
                          &tv, sizeof(tv));
    }
    case FREERTOS_SO_RCVBUF:
    case FREERTOS_SO_SNDBUF: {
        int v = (int)*(const uint32_t *)value;
        return setsockopt(sock, SOL_SOCKET,
                          (opt == FREERTOS_SO_RCVBUF) ? SO_RCVBUF : SO_SNDBUF,
                          &v, sizeof(v));
    }
    case FREERTOS_SO_TCPNODELAY: {
        int v = (*(const BaseType_t *)value != 0) ? 1 : 0;
        return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
    }
    default:
        return -1;
    }
}

uint32_t FreeRTOS_inet_addr(const char *ip)
{
    struct in_addr address;

    if ((ip == NULL) || (inet_pton(AF_INET, ip, &address) != 1)) {
        return 0u;
    }
    return (uint32_t)address.s_addr;
}

uint16_t FreeRTOS_htons(uint16_t v)
{
    return htons(v);
}

int32_t FreeRTOS_connect(Socket_t sock,
                         const struct freertos_sockaddr *addr, size_t len)
{
    struct sockaddr_in sa;

    (void)len;
    if ((sock < 0) || (addr == NULL)) {
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = addr->sin_port;       /* ja em ordem de rede */
    sa.sin_addr.s_addr = addr->sin_addr; /* FreeRTOS usa ordem de rede */
    return connect(sock, (struct sockaddr *)&sa, sizeof(sa));
}

int32_t FreeRTOS_shutdown(Socket_t sock, int32_t how)
{
    (void)how;
    return shutdown(sock, SHUT_RDWR);
}

int32_t FreeRTOS_closesocket(Socket_t sock)
{
    return close(sock);
}

int32_t FreeRTOS_recv(Socket_t sock, void *buf, size_t len, int32_t flags)
{
    (void)flags;
    return (int32_t)recv(sock, buf, len, 0);
}

int32_t FreeRTOS_send(Socket_t sock, const void *buf, size_t len,
                      int32_t flags)
{
    (void)flags;
    return (int32_t)send(sock, buf, len, MSG_NOSIGNAL);
}
