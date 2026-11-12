/*
 * mbtcp.c - Modbus TCP cliente sobre FreeRTOS-Plus-TCP
 *
 * Decisoes:
 *  - buffers TX/RX estaticos: o mutex ja serializa, entao nao precisam
 *    ficar na stack de cada task chamadora.
 *  - leitura por tamanho exato (MBAP length), nunca "chuta" tamanho.
 *  - em timeout/erro de frame o socket e' fechado para ressincronizar;
 *    a proxima chamada reconecta sozinha.
 *  - excecao Modbus NAO fecha o socket (a resposta e' valida).
 */
#include "mbtcp.h"
#include "tcpprobe.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* ------------------------------------------------------------------ */
/* estado                                                              */
/* ------------------------------------------------------------------ */

static Socket_t s_sock = FREERTOS_INVALID_SOCKET;
static SemaphoreHandle_t s_mtx = NULL;

static char s_ip[16];
static uint16_t s_port = 502u;
static uint32_t s_timeout_ms = MBTCP_TIMEOUT_MS;
static uint16_t s_txid = 0u;
static int s_last_exc = 0;

/* protegidos pelo mutex */
static uint8_t s_tx[MBTCP_MAX_FRAME];
static uint8_t s_rx[MBTCP_MAX_FRAME];

/* ------------------------------------------------------------------ */
/* utilitarios                                                         */
/* ------------------------------------------------------------------ */

/* "a.b.c.d" -> uint32 em ordem de rede (little-endian: octeto0 no LSB). */
static int parse_ip4(const char *s, uint32_t *out)
{
    uint32_t v = 0u;
    uint32_t octet = 0u;
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
            octet = 0u;
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
/* socket                                                              */
/* ------------------------------------------------------------------ */

static void close_socket(void)
{
    if (s_sock != FREERTOS_INVALID_SOCKET) {
        (void)FreeRTOS_shutdown(s_sock, FREERTOS_SHUT_RDWR);
        (void)FreeRTOS_closesocket(s_sock);
        s_sock = FREERTOS_INVALID_SOCKET;
    }
}

static int open_socket(void)
{
    struct freertos_sockaddr sa;
    uint32_t addr = 0u;
    TickType_t tmo;

    if (!parse_ip4(s_ip, &addr)) {
        tcpprobe_log("mbtcp: ip invalido '%s'", s_ip);
        return MBTCP_EINVAL;
    }

    s_sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM,
                             FREERTOS_IPPROTO_TCP);
    if (s_sock == FREERTOS_INVALID_SOCKET) {
        tcpprobe_log("mbtcp: socket() falhou (heap=%u)",
                     (unsigned)xPortGetFreeHeapSize());
        return MBTCP_ECONN;
    }

    tmo = pdMS_TO_TICKS(s_timeout_ms);
    if (tmo == 0u) {
        tmo = 1u;
    }
    (void)FreeRTOS_setsockopt(s_sock, 0, FREERTOS_SO_RCVTIMEO,
                              &tmo, sizeof(tmo));
    (void)FreeRTOS_setsockopt(s_sock, 0, FREERTOS_SO_SNDTIMEO,
                              &tmo, sizeof(tmo));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = (uint8_t)FREERTOS_AF_INET;
    sa.sin_port = FreeRTOS_htons(s_port);
    sa.sin_addr = addr;

    if (FreeRTOS_connect(s_sock, &sa, sizeof(sa)) < 0) {
        tcpprobe_log("mbtcp: connect %s:%u falhou", s_ip, (unsigned)s_port);
        (void)FreeRTOS_closesocket(s_sock);
        s_sock = FREERTOS_INVALID_SOCKET;
        return MBTCP_ECONN;
    }

    tcpprobe_log("mbtcp: conectado %s:%u", s_ip, (unsigned)s_port);
    return MBTCP_OK;
}

static int ensure_open(void)
{
    if (s_sock != FREERTOS_INVALID_SOCKET) {
        return MBTCP_OK;
    }
    return open_socket();
}

/* Le exatamente need bytes, respeitando um deadline absoluto em ticks. */
static int recv_full(uint8_t *dst, uint16_t need, TickType_t deadline)
{
    uint16_t got = 0u;

    while (got < need) {
        TickType_t now = xTaskGetTickCount();
        int32_t n;

        if ((int32_t)(deadline - now) <= 0) {
            return MBTCP_ETIMEOUT;
        }

        n = FreeRTOS_recv(s_sock, dst + got, (size_t)(need - got), 0);
        if (n > 0) {
            got = (uint16_t)(got + (uint16_t)n);
        } else if (n == 0) {
            return MBTCP_ECONN;      /* peer fechou */
        } else {
            return MBTCP_ETIMEOUT;
        }
    }

    return MBTCP_OK;
}

/*
 * Envia tx e recebe um frame Modbus TCP completo em s_rx.
 * Retorna o tamanho total do frame (> 0) ou erro (< 0).
 */
static int xfer(const uint8_t *tx, uint16_t txlen, TickType_t deadline)
{
    uint32_t off = 0u;
    uint16_t mlen;
    int rc;

    while (off < (uint32_t)txlen) {
        int32_t n = FreeRTOS_send(s_sock, tx + off,
                                  (size_t)(txlen - off), 0);
        if (n <= 0) {
            return MBTCP_ESEND;
        }
        off += (uint32_t)n;
    }

    rc = recv_full(s_rx, 6u, deadline);
    if (rc != MBTCP_OK) {
        return rc;
    }

    mlen = (uint16_t)(((uint16_t)s_rx[4] << 8) | (uint16_t)s_rx[5]);
    if ((mlen < 2u) || ((uint16_t)(6u + mlen) > (uint16_t)sizeof(s_rx))) {
        return MBTCP_EFRAME;
    }

    rc = recv_full(s_rx + 6, mlen, deadline);
    if (rc != MBTCP_OK) {
        return rc;
    }

    return (int)(6u + mlen);
}

/*
 * Valida MBAP + unit + fc. Retorna MBTCP_OK, MBTCP_EEXCEPTION ou
 * MBTCP_EFRAME. flen e' o tamanho total recebido.
 */
static int check_header(uint16_t txid, uint8_t unit, uint8_t fc, int flen)
{
    if (flen < 8) {
        return MBTCP_EFRAME;
    }
    if ((s_rx[0] != (uint8_t)(txid >> 8)) ||
        (s_rx[1] != (uint8_t)(txid & 0xFFu))) {
        return MBTCP_EFRAME;
    }
    if ((s_rx[2] != 0u) || (s_rx[3] != 0u)) {
        return MBTCP_EFRAME;
    }
    if (s_rx[6] != unit) {
        return MBTCP_EFRAME;
    }
    if (s_rx[7] == (uint8_t)(fc | 0x80u)) {
        if (flen < 9) {
            return MBTCP_EFRAME;
        }
        s_last_exc = (int)s_rx[8];
        return MBTCP_EEXCEPTION;
    }
    if (s_rx[7] != fc) {
        return MBTCP_EFRAME;
    }
    return MBTCP_OK;
}

/* ------------------------------------------------------------------ */
/* API publica                                                         */
/* ------------------------------------------------------------------ */

int mbtcp_open(const char *ip, uint16_t port, uint32_t timeout_ms)
{
    uint32_t tmp = 0u;
    int rc;

    if ((ip == NULL) || !parse_ip4(ip, &tmp)) {
        return MBTCP_EINVAL;
    }

    if (s_mtx == NULL) {
        s_mtx = xSemaphoreCreateMutex();
        if (s_mtx == NULL) {
            return MBTCP_ECONN;
        }
    }

    if (xSemaphoreTake(s_mtx,
                       pdMS_TO_TICKS(MBTCP_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return MBTCP_EBUSY;
    }

    close_socket();

    strncpy(s_ip, ip, sizeof(s_ip) - 1u);
    s_ip[sizeof(s_ip) - 1u] = '\0';
    s_port = port;
    s_timeout_ms = (timeout_ms != 0u) ? timeout_ms : MBTCP_TIMEOUT_MS;
    s_txid = 0u;
    s_last_exc = 0;

    rc = open_socket();

    (void)xSemaphoreGive(s_mtx);
    return rc;
}

void mbtcp_close(void)
{
    if (s_mtx == NULL) {
        close_socket();
        return;
    }

    if (xSemaphoreTake(s_mtx,
                       pdMS_TO_TICKS(MBTCP_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        close_socket();
        (void)xSemaphoreGive(s_mtx);
    } else {
        /* forcado: nao deixa o socket preso */
        close_socket();
    }
}

int mbtcp_read(uint8_t unit, uint8_t fc, uint16_t addr,
               uint16_t qty, uint16_t *out)
{
    uint16_t txid;
    uint16_t i;
    TickType_t deadline;
    int flen;
    int rc;

    if ((out == NULL) || (qty == 0u) || (qty > 125u)) {
        return MBTCP_EINVAL;
    }
    if ((fc != 3u) && (fc != 4u)) {
        return MBTCP_EINVAL;
    }
    if (s_mtx == NULL) {
        return MBTCP_ECONN;
    }

    if (xSemaphoreTake(s_mtx,
                       pdMS_TO_TICKS(MBTCP_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return MBTCP_EBUSY;
    }

    rc = ensure_open();
    if (rc != MBTCP_OK) {
        (void)xSemaphoreGive(s_mtx);
        return rc;
    }

    txid = ++s_txid;
    s_tx[0] = (uint8_t)(txid >> 8);
    s_tx[1] = (uint8_t)(txid & 0xFFu);
    s_tx[2] = 0u;
    s_tx[3] = 0u;
    s_tx[4] = 0u;
    s_tx[5] = 6u;
    s_tx[6] = unit;
    s_tx[7] = fc;
    s_tx[8] = (uint8_t)(addr >> 8);
    s_tx[9] = (uint8_t)(addr & 0xFFu);
    s_tx[10] = (uint8_t)(qty >> 8);
    s_tx[11] = (uint8_t)(qty & 0xFFu);

#if MBTCP_TRACE
    tcpprobe_log_hex("mb tx", s_tx, 12u);
#endif

    deadline = xTaskGetTickCount() + pdMS_TO_TICKS(s_timeout_ms);
    flen = xfer(s_tx, 12u, deadline);

#if MBTCP_TRACE
    if (flen > 0) {
        tcpprobe_log_hex("mb rx", s_rx, (unsigned)flen);
    }
#endif

    if (flen < 0) {
        close_socket();
        (void)xSemaphoreGive(s_mtx);
        return flen;
    }

    rc = check_header(txid, unit, fc, flen);
    if (rc != MBTCP_OK) {
        if (rc == MBTCP_EFRAME) {
            close_socket();
        }
        (void)xSemaphoreGive(s_mtx);
        return rc;
    }

    if ((flen != (int)(9u + (2u * qty))) ||
        (s_rx[8] != (uint8_t)(2u * qty))) {
        close_socket();
        (void)xSemaphoreGive(s_mtx);
        return MBTCP_EFRAME;
    }

    for (i = 0u; i < qty; i++) {
        out[i] = (uint16_t)(((uint16_t)s_rx[9u + (2u * i)] << 8) |
                            (uint16_t)s_rx[10u + (2u * i)]);
    }

    (void)xSemaphoreGive(s_mtx);
    return MBTCP_OK;
}

int mbtcp_write(uint8_t unit, uint16_t addr, uint16_t value)
{
    uint16_t txid;
    TickType_t deadline;
    int flen;
    int rc;

    if (s_mtx == NULL) {
        return MBTCP_ECONN;
    }

    if (xSemaphoreTake(s_mtx,
                       pdMS_TO_TICKS(MBTCP_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return MBTCP_EBUSY;
    }

    rc = ensure_open();
    if (rc != MBTCP_OK) {
        (void)xSemaphoreGive(s_mtx);
        return rc;
    }

    txid = ++s_txid;
    s_tx[0] = (uint8_t)(txid >> 8);
    s_tx[1] = (uint8_t)(txid & 0xFFu);
    s_tx[2] = 0u;
    s_tx[3] = 0u;
    s_tx[4] = 0u;
    s_tx[5] = 6u;
    s_tx[6] = unit;
    s_tx[7] = 6u;
    s_tx[8] = (uint8_t)(addr >> 8);
    s_tx[9] = (uint8_t)(addr & 0xFFu);
    s_tx[10] = (uint8_t)(value >> 8);
    s_tx[11] = (uint8_t)(value & 0xFFu);

#if MBTCP_TRACE
    tcpprobe_log_hex("mb tx", s_tx, 12u);
#endif

    deadline = xTaskGetTickCount() + pdMS_TO_TICKS(s_timeout_ms);
    flen = xfer(s_tx, 12u, deadline);

#if MBTCP_TRACE
    if (flen > 0) {
        tcpprobe_log_hex("mb rx", s_rx, (unsigned)flen);
    }
#endif

    if (flen < 0) {
        close_socket();
        (void)xSemaphoreGive(s_mtx);
        return flen;
    }

    rc = check_header(txid, unit, 6u, flen);
    if (rc != MBTCP_OK) {
        if (rc == MBTCP_EFRAME) {
            close_socket();
        }
        (void)xSemaphoreGive(s_mtx);
        return rc;
    }

    if ((flen != 12) ||
        (s_rx[8] != s_tx[8]) || (s_rx[9] != s_tx[9]) ||
        (s_rx[10] != s_tx[10]) || (s_rx[11] != s_tx[11])) {
        close_socket();
        (void)xSemaphoreGive(s_mtx);
        return MBTCP_EFRAME;
    }

    (void)xSemaphoreGive(s_mtx);
    return MBTCP_OK;
}

int mbtcp_write_multi(uint8_t unit, uint16_t addr,
                      uint16_t qty, const uint16_t *values)
{
    uint16_t txid;
    uint16_t len;
    uint16_t i;
    TickType_t deadline;
    int flen;
    int rc;

    if ((values == NULL) || (qty == 0u) || (qty > 123u)) {
        return MBTCP_EINVAL;
    }
    if (s_mtx == NULL) {
        return MBTCP_ECONN;
    }

    if (xSemaphoreTake(s_mtx,
                       pdMS_TO_TICKS(MBTCP_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return MBTCP_EBUSY;
    }

    rc = ensure_open();
    if (rc != MBTCP_OK) {
        (void)xSemaphoreGive(s_mtx);
        return rc;
    }

    txid = ++s_txid;
    len = (uint16_t)(7u + (2u * qty));

    s_tx[0] = (uint8_t)(txid >> 8);
    s_tx[1] = (uint8_t)(txid & 0xFFu);
    s_tx[2] = 0u;
    s_tx[3] = 0u;
    s_tx[4] = (uint8_t)(len >> 8);
    s_tx[5] = (uint8_t)(len & 0xFFu);
    s_tx[6] = unit;
    s_tx[7] = 16u;
    s_tx[8] = (uint8_t)(addr >> 8);
    s_tx[9] = (uint8_t)(addr & 0xFFu);
    s_tx[10] = (uint8_t)(qty >> 8);
    s_tx[11] = (uint8_t)(qty & 0xFFu);
    s_tx[12] = (uint8_t)(2u * qty);

    for (i = 0u; i < qty; i++) {
        s_tx[13u + (2u * i)] = (uint8_t)(values[i] >> 8);
        s_tx[14u + (2u * i)] = (uint8_t)(values[i] & 0xFFu);
    }

#if MBTCP_TRACE
    tcpprobe_log_hex("mb tx", s_tx, (unsigned)(13u + (2u * qty)));
#endif

    deadline = xTaskGetTickCount() + pdMS_TO_TICKS(s_timeout_ms);
    flen = xfer(s_tx, (uint16_t)(13u + (2u * qty)), deadline);

#if MBTCP_TRACE
    if (flen > 0) {
        tcpprobe_log_hex("mb rx", s_rx, (unsigned)flen);
    }
#endif

    if (flen < 0) {
        close_socket();
        (void)xSemaphoreGive(s_mtx);
        return flen;
    }

    rc = check_header(txid, unit, 16u, flen);
    if (rc != MBTCP_OK) {
        if (rc == MBTCP_EFRAME) {
            close_socket();
        }
        (void)xSemaphoreGive(s_mtx);
        return rc;
    }

    if ((flen != 12) ||
        (s_rx[8] != s_tx[8]) || (s_rx[9] != s_tx[9]) ||
        (s_rx[10] != s_tx[10]) || (s_rx[11] != s_tx[11])) {
        close_socket();
        (void)xSemaphoreGive(s_mtx);
        return MBTCP_EFRAME;
    }

    (void)xSemaphoreGive(s_mtx);
    return MBTCP_OK;
}

int mbtcp_last_exception(void)
{
    return s_last_exc;
}

const char *mbtcp_strerror(int err)
{
    switch (err) {
    case MBTCP_OK:          return "ok";
    case MBTCP_ECONN:       return "connect/socket";
    case MBTCP_ESEND:       return "send";
    case MBTCP_ETIMEOUT:    return "timeout";
    case MBTCP_EFRAME:      return "frame";
    case MBTCP_EEXCEPTION:  return "modbus exception";
    case MBTCP_EBUSY:       return "busy";
    case MBTCP_EINVAL:      return "invalid arg";
    default:                return "?";
    }
}
