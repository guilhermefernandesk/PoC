/*
 * tcpprobe.c - implementacao sobre FreeRTOS-Plus-TCP
 *
 * Sem lwIP, sem HAL, sem netif global, sem task permanente.
 */
#include "tcpprobe.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* ------------------------------------------------------------------ */
/* logging                                                             */
/* ------------------------------------------------------------------ */

static void (*s_logger)(const char *) = NULL;

void tcpprobe_set_logger(void (*fn)(const char *line))
{
    s_logger = fn;
}

void tcpprobe_log(const char *fmt, ...)
{
#if TCPPROBE_LOG_ENABLE
    char buf[TCPPROBE_LOG_BUF];
    va_list ap;
    int n;

    if (fmt == NULL) {
        return;
    }

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n < 0) {
        return;
    }

    if (s_logger != NULL) {
        s_logger(buf);
    } else {
        printf("[tcpprobe] %s\r\n", buf);
    }
#else
    (void)fmt;
#endif
}

void tcpprobe_log_hex(const char *tag, const void *buf, unsigned len)
{
#if TCPPROBE_LOG_ENABLE
    const uint8_t *p = (const uint8_t *)buf;
    char line[TCPPROBE_LOG_BUF];
    unsigned i;
    unsigned off = 0;

    if (tag == NULL) {
        tag = "hex";
    }
    if (p == NULL) {
        return;
    }

    line[0] = '\0';

    for (i = 0; i < len; i++) {
        int n = snprintf(line + off, sizeof(line) - off, "%02X ", p[i]);

        if ((n <= 0) || ((unsigned)n >= (sizeof(line) - off))) {
            tcpprobe_log("%s: ... (%u bytes)", tag, len);
            return;
        }
        off += (unsigned)n;

        if (((i & 15u) == 15u) && ((i + 1u) < len)) {
            tcpprobe_log("%s: %s", tag, line);
            line[0] = '\0';
            off = 0;
        }
    }

    if (off > 0u) {
        tcpprobe_log("%s: %s", tag, line);
    }
#else
    (void)tag;
    (void)buf;
    (void)len;
#endif
}

/* ------------------------------------------------------------------ */
/* utilitarios                                                         */
/* ------------------------------------------------------------------ */

/* "a.b.c.d" -> uint32 em ordem de rede (byte 0 no LSB, little-endian). */
static int parse_ip4(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    uint32_t octet = 0;
    int parts = 0;
    int digits = 0;

    if ((s == NULL) || (out == NULL)) {
        return 0;
    }

    for (;;) {
        char c = *s++;

        if ((c >= '0') && (c <= '9')) {
            octet = (octet * 10u) + (uint32_t)(c - '0');
            if ((++digits > 3) || (octet > 255u)) {
                return 0;
            }
        } else if ((c == '.') || (c == '\0')) {
            if ((digits == 0) || (parts >= 4)) {
                return 0;
            }
            v |= octet << (8 * parts);
            parts++;
            octet = 0;
            digits = 0;

            if (c == '\0') {
                break;
            }
        } else {
            return 0;
        }
    }

    if (parts != 4) {
        return 0;
    }

    *out = v;
    return 1;
}

/* ------------------------------------------------------------------ */
/* env dump                                                            */
/* ------------------------------------------------------------------ */

void tcpprobe_env_dump(void)
{
    tcpprobe_log("tcpprobe %s", TCPPROBE_VERSION);

    tcpprobe_log("heap free=%u tasks=%u",
                 (unsigned)xPortGetFreeHeapSize(),
                 (unsigned)uxTaskGetNumberOfTasks());

#if TCPPROBE_HAS_MIN_HEAP
    tcpprobe_log("heap min=%u", (unsigned)xPortGetMinimumEverFreeHeapSize());
#endif

#ifdef configTOTAL_HEAP_SIZE
    tcpprobe_log("configTOTAL_HEAP_SIZE=%u", (unsigned)configTOTAL_HEAP_SIZE);
#endif

#if TCPPROBE_HAS_ISNETWORKUP
    tcpprobe_log("net up=%d", (int)FreeRTOS_IsNetworkUp());
#endif

#if TCPPROBE_HAS_ENDPOINT_CFG
    {
        uint32_t ip = 0;
        uint32_t mask = 0;
        uint32_t gw = 0;
        uint32_t dns = 0;

        FreeRTOS_GetEndPointConfiguration(&ip, &mask, &gw, &dns);

        tcpprobe_log("ip=%u.%u.%u.%u mask=%u.%u.%u.%u gw=%u.%u.%u.%u",
                     (unsigned)(ip & 0xFFu),
                     (unsigned)((ip >> 8) & 0xFFu),
                     (unsigned)((ip >> 16) & 0xFFu),
                     (unsigned)((ip >> 24) & 0xFFu),
                     (unsigned)(mask & 0xFFu),
                     (unsigned)((mask >> 8) & 0xFFu),
                     (unsigned)((mask >> 16) & 0xFFu),
                     (unsigned)((mask >> 24) & 0xFFu),
                     (unsigned)(gw & 0xFFu),
                     (unsigned)((gw >> 8) & 0xFFu),
                     (unsigned)((gw >> 16) & 0xFFu),
                     (unsigned)((gw >> 24) & 0xFFu));
    }
#endif

#ifdef ipconfigTCP_RX_BUFFER_LENGTH
    tcpprobe_log("tcp rx_buf=%u tx_buf=%u",
                 (unsigned)ipconfigTCP_RX_BUFFER_LENGTH,
                 (unsigned)ipconfigTCP_TX_BUFFER_LENGTH);
#endif

#ifdef ipconfigTCP_WIN_SEG_COUNT
    tcpprobe_log("tcp win_seg=%u", (unsigned)ipconfigTCP_WIN_SEG_COUNT);
#endif

#ifdef ipconfigNETWORK_MTU
    tcpprobe_log("net mtu=%u", (unsigned)ipconfigNETWORK_MTU);
#endif
}

/* ------------------------------------------------------------------ */
/* cliente one-shot                                                    */
/* ------------------------------------------------------------------ */

int tcpprobe_tcp_client(const char *ip, uint16_t port,
                        const void *tx, uint16_t txlen,
                        void *rx, uint16_t rxcap,
                        uint32_t timeout_ms)
{
    Socket_t sock;
    struct freertos_sockaddr sa;
    uint32_t addr = 0;
    TickType_t tmo;
    uint32_t off;
    int32_t n;

    if ((ip == NULL) || (rx == NULL) || (rxcap == 0u)) {
        return TCPPROBE_EINVAL;
    }
    if (!parse_ip4(ip, &addr)) {
        tcpprobe_log("tcp: ip invalido '%s'", ip);
        return TCPPROBE_EINVAL;
    }
    if (timeout_ms == 0u) {
        timeout_ms = 1000u;
    }

    sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM,
                           FREERTOS_IPPROTO_TCP);
    if (sock == FREERTOS_INVALID_SOCKET) {
        tcpprobe_log("tcp: socket() falhou (heap=%u)",
                     (unsigned)xPortGetFreeHeapSize());
        return TCPPROBE_ECONN;
    }

    tmo = pdMS_TO_TICKS(timeout_ms);
    if (tmo == 0u) {
        tmo = 1u;
    }
    (void)FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_RCVTIMEO, &tmo, sizeof(tmo));
    (void)FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_SNDTIMEO, &tmo, sizeof(tmo));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = (uint8_t)FREERTOS_AF_INET;
    sa.sin_port = FreeRTOS_htons(port);
    sa.sin_addr = addr;

    tcpprobe_log("tcp: connect %s:%u ...", ip, (unsigned)port);

    if (FreeRTOS_connect(sock, &sa, sizeof(sa)) < 0) {
        tcpprobe_log("tcp: connect falhou");
        (void)FreeRTOS_closesocket(sock);
        return TCPPROBE_ECONN;
    }

    if ((tx != NULL) && (txlen > 0u)) {
        off = 0;
        while (off < (uint32_t)txlen) {
            n = FreeRTOS_send(sock, (const uint8_t *)tx + off,
                              (size_t)(txlen - off), 0);
            if (n <= 0) {
                tcpprobe_log("tcp: send falhou");
                (void)FreeRTOS_closesocket(sock);
                return TCPPROBE_ESEND;
            }
            off += (uint32_t)n;
        }
        tcpprobe_log_hex("tx", tx, txlen);
    }

    n = FreeRTOS_recv(sock, rx, (size_t)rxcap, 0);

    if (n > 0) {
        tcpprobe_log_hex("rx", rx, (unsigned)n);
    } else if (n == 0) {
        tcpprobe_log("tcp: peer fechou sem dados");
    } else {
        tcpprobe_log("tcp: recv timeout/erro (%d)", (int)n);
    }

    (void)FreeRTOS_shutdown(sock, FREERTOS_SHUT_RDWR);
    (void)FreeRTOS_closesocket(sock);

    if (n > 0) {
        return (int)n;
    }
    if (n == 0) {
        return TCPPROBE_ERECV;
    }
    return TCPPROBE_ETIMEOUT;
}
