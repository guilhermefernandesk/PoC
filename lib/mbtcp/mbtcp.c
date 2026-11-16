/*
 * mbtcp.c - Modbus TCP cliente sobre FreeRTOS-Plus-TCP
 *
 * Decisoes:
 *  - buffers TX/RX/PDU estaticos: o mutex ja serializa, entao nao precisam
 *    ficar na stack de cada task chamadora.
 *  - leitura por tamanho exato (MBAP length), nunca "chuta" tamanho.
 *  - em timeout/erro de frame o socket e' fechado para ressincronizar;
 *    a proxima chamada reconecta sozinha.
 *  - excecao Modbus NAO fecha o socket (a resposta e' valida).
 *  - retry de leitura configuravel; escrita nao e' repetida por default.
 *  - sem task permanente: a aplicacao chama a API de uma task existente.
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
static uint8_t s_unit = 0u;          /* UHX65A */
static uint32_t s_timeout_ms = MBTCP_TIMEOUT_MS;
static uint8_t s_retries_read = 0u;
static uint8_t s_retries_write = 0u;
static uint16_t s_rx_buf = 0u;
static uint16_t s_tx_buf = 0u;
static uint8_t s_nodelay = 1u;
static uint16_t s_txid = 0u;
static int s_last_exc = 0;
static mbtcp_stats_t s_stats;

/* protegidos pelo mutex */
static uint8_t s_tx[MBTCP_MAX_FRAME];
static uint8_t s_rx[MBTCP_MAX_FRAME];
static uint8_t s_rsp[MBTCP_MAX_PDU];

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

static int lock_mbtcp(void)
{
    if (s_mtx == NULL) {
        return MBTCP_ECONN;
    }
    if (xSemaphoreTake(s_mtx,
                       pdMS_TO_TICKS(MBTCP_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return MBTCP_EBUSY;
    }
    return MBTCP_OK;
}

static void unlock_mbtcp(void)
{
    (void)xSemaphoreGive(s_mtx);
}

static int ensure_mutex(void)
{
    if (s_mtx == NULL) {
        s_mtx = xSemaphoreCreateMutex();
        if (s_mtx == NULL) {
            return MBTCP_ECONN;
        }
    }
    return MBTCP_OK;
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
    s_stats.connected = 0u;
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
        s_stats.connect_errors++;
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

#ifdef FREERTOS_SO_RCVBUF
    if (s_rx_buf != 0u) {
        uint32_t v = (uint32_t)s_rx_buf;
        (void)FreeRTOS_setsockopt(s_sock, 0, FREERTOS_SO_RCVBUF,
                                  &v, sizeof(v));
    }
#endif
#ifdef FREERTOS_SO_SNDBUF
    if (s_tx_buf != 0u) {
        uint32_t v = (uint32_t)s_tx_buf;
        (void)FreeRTOS_setsockopt(s_sock, 0, FREERTOS_SO_SNDBUF,
                                  &v, sizeof(v));
    }
#endif
#ifdef FREERTOS_SO_TCPNODELAY
    if (s_nodelay != 0u) {
        BaseType_t v = pdTRUE;
        (void)FreeRTOS_setsockopt(s_sock, 0, FREERTOS_SO_TCPNODELAY,
                                  &v, sizeof(v));
    }
#endif

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = (uint8_t)FREERTOS_AF_INET;
    sa.sin_port = FreeRTOS_htons(s_port);
    sa.sin_addr = addr;

    if (FreeRTOS_connect(s_sock, &sa, sizeof(sa)) < 0) {
        tcpprobe_log("mbtcp: connect %s:%u falhou", s_ip, (unsigned)s_port);
        s_stats.connect_errors++;
        close_socket();
        return MBTCP_ECONN;
    }

    s_stats.connected = 1u;
    s_stats.connects++;
    tcpprobe_log("mbtcp: conectado %s:%u unit=%u",
                 s_ip, (unsigned)s_port, (unsigned)s_unit);
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

static int bad_frame(void)
{
    s_stats.frame_errors++;
    s_stats.last_error = MBTCP_EFRAME;
    close_socket();
    return MBTCP_EFRAME;
}

/*
 * Executa uma transacao (PDU -> PDU de resposta em s_rsp).
 * Assume o mutex tomado. *rsp_len recebe o tamanho da PDU de resposta
 * (inclui o function code).
 */
static int transact_locked(const uint8_t *pdu, uint16_t pdu_len,
                           uint16_t *rsp_len, uint8_t retries)
{
    uint16_t attempt;
    int rc = MBTCP_ECONN;
    uint16_t txlen;

    if ((pdu == NULL) || (pdu_len == 0u) || (pdu_len > MBTCP_MAX_PDU) ||
        (rsp_len == NULL)) {
        return MBTCP_EINVAL;
    }

    txlen = (uint16_t)(7u + pdu_len);
    s_last_exc = 0;

    for (attempt = 0u; attempt <= retries; attempt++) {
        uint16_t txid;
        TickType_t t0;
        TickType_t t1;
        TickType_t deadline;
        int flen;

        if (attempt != 0u) {
            s_stats.retries++;
        }

        rc = ensure_open();
        if (rc != MBTCP_OK) {
            s_stats.last_error = rc;
            if (attempt < retries) {
                continue;
            }
            break;
        }

        txid = ++s_txid;
        s_tx[0] = (uint8_t)(txid >> 8);
        s_tx[1] = (uint8_t)(txid & 0xFFu);
        s_tx[2] = 0u;
        s_tx[3] = 0u;
        s_tx[4] = (uint8_t)((1u + pdu_len) >> 8);
        s_tx[5] = (uint8_t)((1u + pdu_len) & 0xFFu);
        s_tx[6] = s_unit;
        memcpy(s_tx + 7, pdu, pdu_len);

#if MBTCP_TRACE
        tcpprobe_log_hex("mb tx", s_tx, (unsigned)txlen);
#endif
        s_stats.tx_count++;

        t0 = xTaskGetTickCount();
        deadline = t0 + pdMS_TO_TICKS(s_timeout_ms);
        if (deadline == t0) {
            deadline = t0 + 1u;         /* timeout menor que 1 tick */
        }
        flen = xfer(s_tx, txlen, deadline);
        t1 = xTaskGetTickCount();
        s_stats.latency_ms = (uint32_t)((t1 - t0) *
                                        (TickType_t)portTICK_PERIOD_MS);

#if MBTCP_TRACE
        if (flen > 0) {
            tcpprobe_log_hex("mb rx", s_rx, (unsigned)flen);
        }
#endif

        if (flen < 0) {
            rc = flen;
            s_stats.last_error = rc;
            if (rc == MBTCP_ETIMEOUT) {
                s_stats.timeouts++;
            } else if (rc == MBTCP_EFRAME) {
                s_stats.frame_errors++;
            }
            close_socket();
            if (attempt < retries) {
                continue;
            }
            break;
        }

        if (flen < 8) {
            rc = bad_frame();
            if (attempt < retries) {
                continue;
            }
            break;
        }
        if ((s_rx[0] != (uint8_t)(txid >> 8)) ||
            (s_rx[1] != (uint8_t)(txid & 0xFFu)) ||
            (s_rx[2] != 0u) || (s_rx[3] != 0u) ||
            (s_rx[6] != s_unit)) {
            rc = bad_frame();
            if (attempt < retries) {
                continue;
            }
            break;
        }

        if (s_rx[7] == (uint8_t)(pdu[0] | 0x80u)) {
            if (flen != 9) {
                rc = bad_frame();
                if (attempt < retries) {
                    continue;
                }
                break;
            }
            s_last_exc = (int)s_rx[8];
            s_stats.rx_count++;
            s_stats.exceptions++;
            s_stats.last_error = MBTCP_EEXCEPTION;
            rc = MBTCP_EEXCEPTION;
            break;
        }

        if (s_rx[7] != pdu[0]) {
            rc = bad_frame();
            if (attempt < retries) {
                continue;
            }
            break;
        }

        {
            uint16_t rlen = (uint16_t)((uint16_t)flen - 7u);
            if (rlen > (uint16_t)sizeof(s_rsp)) {
                rc = bad_frame();
                if (attempt < retries) {
                    continue;
                }
                break;
            }
            memcpy(s_rsp, s_rx + 7, rlen);
            *rsp_len = rlen;
        }

        s_stats.rx_count++;
        s_stats.last_error = MBTCP_OK;
        rc = MBTCP_OK;
        break;
    }

    return rc;
}

/* ------------------------------------------------------------------ */
/* blocos internos (assumem mutex tomado)                              */
/* ------------------------------------------------------------------ */

static int read_regs_locked(uint8_t fc, uint16_t addr, uint16_t qty,
                            uint16_t *out, uint8_t retries)
{
    uint8_t pdu[5];
    uint16_t rlen;
    uint16_t i;
    int rc;

    pdu[0] = fc;
    pdu[1] = (uint8_t)(addr >> 8);
    pdu[2] = (uint8_t)(addr & 0xFFu);
    pdu[3] = (uint8_t)(qty >> 8);
    pdu[4] = (uint8_t)(qty & 0xFFu);

    rc = transact_locked(pdu, 5u, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }

    if ((rlen != (uint16_t)(2u + (2u * qty))) ||
        (s_rsp[1] != (uint8_t)(2u * qty))) {
        return bad_frame();
    }

    for (i = 0u; i < qty; i++) {
        out[i] = (uint16_t)(((uint16_t)s_rsp[2u + (2u * i)] << 8) |
                            (uint16_t)s_rsp[3u + (2u * i)]);
    }
    return MBTCP_OK;
}

static int read_bits_locked(uint8_t fc, uint16_t addr, uint16_t qty,
                            uint8_t *bits, uint8_t retries)
{
    uint8_t pdu[5];
    uint16_t rlen;
    uint16_t bc;
    uint16_t i;
    int rc;

    bc = (uint16_t)((qty + 7u) / 8u);

    pdu[0] = fc;
    pdu[1] = (uint8_t)(addr >> 8);
    pdu[2] = (uint8_t)(addr & 0xFFu);
    pdu[3] = (uint8_t)(qty >> 8);
    pdu[4] = (uint8_t)(qty & 0xFFu);

    rc = transact_locked(pdu, 5u, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }

    if ((rlen != (uint16_t)(2u + bc)) || (s_rsp[1] != (uint8_t)bc)) {
        return bad_frame();
    }

    for (i = 0u; i < qty; i++) {
        bits[i] = (uint8_t)((s_rsp[2u + (i / 8u)] >> (i % 8u)) & 1u);
    }
    return MBTCP_OK;
}

static int write_coil_locked(uint16_t addr, int on, uint8_t retries)
{
    uint8_t pdu[5];
    uint16_t value = (on != 0) ? 0xFF00u : 0x0000u;
    uint16_t rlen;
    int rc;

    pdu[0] = 5u;
    pdu[1] = (uint8_t)(addr >> 8);
    pdu[2] = (uint8_t)(addr & 0xFFu);
    pdu[3] = (uint8_t)(value >> 8);
    pdu[4] = (uint8_t)(value & 0xFFu);

    rc = transact_locked(pdu, 5u, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if ((rlen != 5u) || (s_rsp[1] != pdu[1]) || (s_rsp[2] != pdu[2]) ||
        (s_rsp[3] != pdu[3]) || (s_rsp[4] != pdu[4])) {
        return bad_frame();
    }
    return MBTCP_OK;
}

static int write_register_locked(uint16_t addr, uint16_t value,
                                 uint8_t retries)
{
    uint8_t pdu[5];
    uint16_t rlen;
    int rc;

    pdu[0] = 6u;
    pdu[1] = (uint8_t)(addr >> 8);
    pdu[2] = (uint8_t)(addr & 0xFFu);
    pdu[3] = (uint8_t)(value >> 8);
    pdu[4] = (uint8_t)(value & 0xFFu);

    rc = transact_locked(pdu, 5u, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if ((rlen != 5u) || (s_rsp[1] != pdu[1]) || (s_rsp[2] != pdu[2]) ||
        (s_rsp[3] != pdu[3]) || (s_rsp[4] != pdu[4])) {
        return bad_frame();
    }
    return MBTCP_OK;
}

static int write_coils_locked(uint16_t addr, uint16_t qty,
                              const uint8_t *bits, uint8_t retries)
{
    uint8_t pdu[6u + 250u];
    uint16_t bc;
    uint16_t rlen;
    uint16_t i;
    uint16_t len;
    int rc;

    bc = (uint16_t)((qty + 7u) / 8u);
    len = (uint16_t)(6u + bc);

    pdu[0] = 15u;
    pdu[1] = (uint8_t)(addr >> 8);
    pdu[2] = (uint8_t)(addr & 0xFFu);
    pdu[3] = (uint8_t)(qty >> 8);
    pdu[4] = (uint8_t)(qty & 0xFFu);
    pdu[5] = (uint8_t)bc;
    memset(pdu + 6, 0, bc);

    for (i = 0u; i < qty; i++) {
        if (bits[i] != 0u) {
            pdu[6u + (i / 8u)] |= (uint8_t)(1u << (i % 8u));
        }
    }

    rc = transact_locked(pdu, len, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if ((rlen != 5u) || (s_rsp[1] != pdu[1]) || (s_rsp[2] != pdu[2]) ||
        (s_rsp[3] != pdu[3]) || (s_rsp[4] != pdu[4])) {
        return bad_frame();
    }
    return MBTCP_OK;
}

static int write_registers_locked(uint16_t addr, uint16_t qty,
                                  const uint16_t *values, uint8_t retries)
{
    uint8_t pdu[6u + 246u];
    uint16_t len;
    uint16_t rlen;
    uint16_t i;
    int rc;

    len = (uint16_t)(6u + (2u * qty));

    pdu[0] = 16u;
    pdu[1] = (uint8_t)(addr >> 8);
    pdu[2] = (uint8_t)(addr & 0xFFu);
    pdu[3] = (uint8_t)(qty >> 8);
    pdu[4] = (uint8_t)(qty & 0xFFu);
    pdu[5] = (uint8_t)(2u * qty);

    for (i = 0u; i < qty; i++) {
        pdu[6u + (2u * i)] = (uint8_t)(values[i] >> 8);
        pdu[7u + (2u * i)] = (uint8_t)(values[i] & 0xFFu);
    }

    rc = transact_locked(pdu, len, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if ((rlen != 5u) || (s_rsp[1] != pdu[1]) || (s_rsp[2] != pdu[2]) ||
        (s_rsp[3] != pdu[3]) || (s_rsp[4] != pdu[4])) {
        return bad_frame();
    }
    return MBTCP_OK;
}

static int mask_write_locked(uint16_t addr, uint16_t and_mask,
                             uint16_t or_mask, uint8_t retries)
{
    uint8_t pdu[7];
    uint16_t rlen;
    int rc;

    pdu[0] = 22u;
    pdu[1] = (uint8_t)(addr >> 8);
    pdu[2] = (uint8_t)(addr & 0xFFu);
    pdu[3] = (uint8_t)(and_mask >> 8);
    pdu[4] = (uint8_t)(and_mask & 0xFFu);
    pdu[5] = (uint8_t)(or_mask >> 8);
    pdu[6] = (uint8_t)(or_mask & 0xFFu);

    rc = transact_locked(pdu, 7u, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if ((rlen != 7u) || (memcmp(s_rsp + 1, pdu + 1, 6u) != 0)) {
        return bad_frame();
    }
    return MBTCP_OK;
}

static int read_write_locked(uint16_t raddr, uint16_t rqty, uint16_t *rout,
                             uint16_t waddr, uint16_t wqty,
                             const uint16_t *win, uint8_t retries)
{
    uint8_t pdu[10u + 242u];
    uint16_t len;
    uint16_t rlen;
    uint16_t i;
    int rc;

    len = (uint16_t)(10u + (2u * wqty));

    pdu[0] = 23u;
    pdu[1] = (uint8_t)(raddr >> 8);
    pdu[2] = (uint8_t)(raddr & 0xFFu);
    pdu[3] = (uint8_t)(rqty >> 8);
    pdu[4] = (uint8_t)(rqty & 0xFFu);
    pdu[5] = (uint8_t)(waddr >> 8);
    pdu[6] = (uint8_t)(waddr & 0xFFu);
    pdu[7] = (uint8_t)(wqty >> 8);
    pdu[8] = (uint8_t)(wqty & 0xFFu);
    pdu[9] = (uint8_t)(2u * wqty);

    for (i = 0u; i < wqty; i++) {
        pdu[10u + (2u * i)] = (uint8_t)(win[i] >> 8);
        pdu[11u + (2u * i)] = (uint8_t)(win[i] & 0xFFu);
    }

    rc = transact_locked(pdu, len, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if ((rlen != (uint16_t)(2u + (2u * rqty))) ||
        (s_rsp[1] != (uint8_t)(2u * rqty))) {
        return bad_frame();
    }

    for (i = 0u; i < rqty; i++) {
        rout[i] = (uint16_t)(((uint16_t)s_rsp[2u + (2u * i)] << 8) |
                             (uint16_t)s_rsp[3u + (2u * i)]);
    }
    return MBTCP_OK;
}

static int read_fifo_locked(uint16_t addr, uint16_t *out, uint16_t cap,
                            uint16_t *count, uint8_t retries)
{
    uint8_t pdu[3];
    uint16_t rlen;
    uint16_t bc;
    uint16_t n;
    uint16_t i;
    int rc;

    pdu[0] = 24u;
    pdu[1] = (uint8_t)(addr >> 8);
    pdu[2] = (uint8_t)(addr & 0xFFu);

    rc = transact_locked(pdu, 3u, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if (rlen < 4u) {
        return bad_frame();
    }

    bc = s_rsp[1];
    n = (uint16_t)(((uint16_t)s_rsp[2] << 8) | (uint16_t)s_rsp[3]);

    if ((bc != (uint16_t)(2u + (2u * n))) ||
        (rlen != (uint16_t)(2u + bc)) || (n > 31u)) {
        return bad_frame();
    }
    if (n > cap) {
        return MBTCP_EINVAL;
    }

    for (i = 0u; i < n; i++) {
        out[i] = (uint16_t)(((uint16_t)s_rsp[4u + (2u * i)] << 8) |
                            (uint16_t)s_rsp[5u + (2u * i)]);
    }
    *count = n;
    return MBTCP_OK;
}

static int read_file_locked(uint16_t file, uint16_t record, uint16_t qty,
                            uint16_t *out, uint8_t retries)
{
    uint8_t pdu[9];
    uint16_t rlen;
    uint16_t bc;
    uint16_t sub_len;
    uint16_t i;
    int rc;

    pdu[0] = 20u;
    pdu[1] = 7u;                 /* byte count */
    pdu[2] = 6u;                 /* reference type */
    pdu[3] = (uint8_t)(file >> 8);
    pdu[4] = (uint8_t)(file & 0xFFu);
    pdu[5] = (uint8_t)(record >> 8);
    pdu[6] = (uint8_t)(record & 0xFFu);
    pdu[7] = (uint8_t)(qty >> 8);
    pdu[8] = (uint8_t)(qty & 0xFFu);

    rc = transact_locked(pdu, 9u, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if (rlen < 4u) {
        return bad_frame();
    }

    bc = s_rsp[1];
    sub_len = s_rsp[2];

    if ((bc != (uint16_t)(2u + (2u * qty))) ||
        (sub_len != (uint16_t)(1u + (2u * qty))) ||
        (s_rsp[3] != 6u) ||
        (rlen != (uint16_t)(2u + bc))) {
        return bad_frame();
    }

    for (i = 0u; i < qty; i++) {
        out[i] = (uint16_t)(((uint16_t)s_rsp[4u + (2u * i)] << 8) |
                            (uint16_t)s_rsp[5u + (2u * i)]);
    }
    return MBTCP_OK;
}

static int write_file_locked(uint16_t file, uint16_t record, uint16_t qty,
                             const uint16_t *in, uint8_t retries)
{
    uint8_t pdu[9u + 244u];
    uint16_t len;
    uint16_t rlen;
    uint16_t i;
    int rc;

    len = (uint16_t)(9u + (2u * qty));

    pdu[0] = 21u;
    pdu[1] = (uint8_t)(7u + (2u * qty));
    pdu[2] = 6u;
    pdu[3] = (uint8_t)(file >> 8);
    pdu[4] = (uint8_t)(file & 0xFFu);
    pdu[5] = (uint8_t)(record >> 8);
    pdu[6] = (uint8_t)(record & 0xFFu);
    pdu[7] = (uint8_t)(qty >> 8);
    pdu[8] = (uint8_t)(qty & 0xFFu);

    for (i = 0u; i < qty; i++) {
        pdu[9u + (2u * i)] = (uint8_t)(in[i] >> 8);
        pdu[10u + (2u * i)] = (uint8_t)(in[i] & 0xFFu);
    }

    rc = transact_locked(pdu, len, &rlen, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }
    if ((rlen != len) || (memcmp(s_rsp, pdu, len) != 0)) {
        return bad_frame();
    }
    return MBTCP_OK;
}

static int device_id_locked(uint8_t category, uint8_t start_obj,
                            mbtcp_devinfo_cb cb, void *ctx)
{
    uint8_t obj_id = start_obj;
    uint8_t page;

    for (page = 0u; page < 32u; page++) {
        uint8_t pdu[4];
        uint16_t rlen;
        uint8_t n;
        uint8_t i;
        uint16_t off;
        int rc;

        pdu[0] = 0x2Bu;
        pdu[1] = 0x0Eu;
        pdu[2] = category;
        pdu[3] = obj_id;

        rc = transact_locked(pdu, 4u, &rlen, s_retries_read);
        if (rc != MBTCP_OK) {
            return rc;
        }

        if ((rlen < 7u) || (s_rsp[0] != 0x2Bu) ||
            (s_rsp[1] != 0x0Eu) || (s_rsp[2] != category)) {
            return bad_frame();
        }

        n = s_rsp[6];
        off = 7u;

        for (i = 0u; i < n; i++) {
            uint8_t id;
            uint8_t len;

            if ((uint16_t)(off + 2u) > rlen) {
                return bad_frame();
            }
            id = s_rsp[off];
            len = s_rsp[off + 1u];
            off = (uint16_t)(off + 2u);

            if ((uint16_t)(off + len) > rlen) {
                return bad_frame();
            }
            if (cb != NULL) {
                cb(id, s_rsp + off, len, ctx);
            }
            off = (uint16_t)(off + len);
        }

        if ((s_rsp[4] == 0u) || (s_rsp[5] == obj_id)) {
            break;
        }
        obj_id = s_rsp[5];
    }

    return MBTCP_OK;
}

/* ------------------------------------------------------------------ */
/* API publica                                                         */
/* ------------------------------------------------------------------ */

void mbtcp_cfg_default(mbtcp_cfg_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = 502u;
    cfg->unit = 0u;
    cfg->timeout_ms = MBTCP_TIMEOUT_MS;
    cfg->retries_read = 0u;
    cfg->retries_write = 0u;
    cfg->nodelay = 1u;
}

int mbtcp_init(const mbtcp_cfg_t *cfg)
{
    uint32_t tmp = 0u;
    int rc;

    if ((cfg == NULL) || (cfg->ip == NULL) || !parse_ip4(cfg->ip, &tmp)) {
        return MBTCP_EINVAL;
    }

    rc = ensure_mutex();
    if (rc != MBTCP_OK) {
        return rc;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }

    close_socket();

    memset(s_ip, 0, sizeof(s_ip));
    strncpy(s_ip, cfg->ip, sizeof(s_ip) - 1u);
    s_port = (cfg->port != 0u) ? cfg->port : 502u;
    s_unit = cfg->unit;
    s_timeout_ms = (cfg->timeout_ms != 0u) ? cfg->timeout_ms
                                           : MBTCP_TIMEOUT_MS;
    s_retries_read = (cfg->retries_read > 5u) ? 5u : cfg->retries_read;
    s_retries_write = (cfg->retries_write > 5u) ? 5u : cfg->retries_write;
    s_rx_buf = cfg->rx_buf;
    s_tx_buf = cfg->tx_buf;
    s_nodelay = cfg->nodelay;
    s_txid = 0u;
    s_last_exc = 0;
    memset(&s_stats, 0, sizeof(s_stats));

    unlock_mbtcp();
    return MBTCP_OK;
}

int mbtcp_connect(void)
{
    int rc;

    rc = ensure_mutex();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }

    rc = ensure_open();
    unlock_mbtcp();
    return rc;
}

void mbtcp_disconnect(void)
{
    if (s_mtx == NULL) {
        close_socket();
        return;
    }
    if (lock_mbtcp() == MBTCP_OK) {
        close_socket();
        unlock_mbtcp();
    }
}

int mbtcp_set_unit(uint8_t unit)
{
    int rc;

    rc = ensure_mutex();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    s_unit = unit;
    unlock_mbtcp();
    return MBTCP_OK;
}

const mbtcp_stats_t *mbtcp_stats(void)
{
    return &s_stats;
}

int mbtcp_open(const char *ip, uint16_t port, uint32_t timeout_ms)
{
    mbtcp_cfg_t cfg;
    int rc;

    mbtcp_cfg_default(&cfg);
    cfg.ip = ip;
    cfg.port = port;
    cfg.timeout_ms = timeout_ms;
    cfg.unit = 0u;

    rc = mbtcp_init(&cfg);
    if (rc != MBTCP_OK) {
        return rc;
    }
    return mbtcp_connect();
}

void mbtcp_close(void)
{
    mbtcp_disconnect();
}

int mbtcp_read_holding(uint16_t addr, uint16_t qty, uint16_t *out)
{
    int rc;

    if ((out == NULL) || (qty == 0u) || (qty > 125u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_regs_locked(3u, addr, qty, out, s_retries_read);
    unlock_mbtcp();
    return rc;
}

int mbtcp_read_input(uint16_t addr, uint16_t qty, uint16_t *out)
{
    int rc;

    if ((out == NULL) || (qty == 0u) || (qty > 125u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_regs_locked(4u, addr, qty, out, s_retries_read);
    unlock_mbtcp();
    return rc;
}

int mbtcp_read_coils(uint16_t addr, uint16_t qty, uint8_t *bits)
{
    int rc;

    if ((bits == NULL) || (qty == 0u) || (qty > 2000u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_bits_locked(1u, addr, qty, bits, s_retries_read);
    unlock_mbtcp();
    return rc;
}

int mbtcp_read_discrete_inputs(uint16_t addr, uint16_t qty, uint8_t *bits)
{
    int rc;

    if ((bits == NULL) || (qty == 0u) || (qty > 2000u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_bits_locked(2u, addr, qty, bits, s_retries_read);
    unlock_mbtcp();
    return rc;
}

int mbtcp_write_coil(uint16_t addr, int on)
{
    int rc;

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = write_coil_locked(addr, on, s_retries_write);
    unlock_mbtcp();
    return rc;
}

int mbtcp_write_register(uint16_t addr, uint16_t value)
{
    int rc;

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = write_register_locked(addr, value, s_retries_write);
    unlock_mbtcp();
    return rc;
}

int mbtcp_write_coils(uint16_t addr, uint16_t qty, const uint8_t *bits)
{
    int rc;

    if ((bits == NULL) || (qty == 0u) || (qty > 1968u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = write_coils_locked(addr, qty, bits, s_retries_write);
    unlock_mbtcp();
    return rc;
}

int mbtcp_write_registers(uint16_t addr, uint16_t qty,
                          const uint16_t *values)
{
    int rc;

    if ((values == NULL) || (qty == 0u) || (qty > 123u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = write_registers_locked(addr, qty, values, s_retries_write);
    unlock_mbtcp();
    return rc;
}

int mbtcp_mask_write_register(uint16_t addr,
                              uint16_t and_mask, uint16_t or_mask)
{
    int rc;

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = mask_write_locked(addr, and_mask, or_mask, s_retries_write);
    unlock_mbtcp();
    return rc;
}

int mbtcp_read_write_registers(uint16_t raddr, uint16_t rqty, uint16_t *rout,
                               uint16_t waddr, uint16_t wqty,
                               const uint16_t *win)
{
    int rc;

    if ((rout == NULL) || (rqty == 0u) || (rqty > 125u) ||
        (win == NULL) || (wqty == 0u) || (wqty > 121u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_write_locked(raddr, rqty, rout, waddr, wqty, win,
                           s_retries_write);
    unlock_mbtcp();
    return rc;
}

int mbtcp_read_fifo(uint16_t addr, uint16_t *out, uint16_t cap,
                    uint16_t *count)
{
    int rc;

    if ((out == NULL) || (cap == 0u) || (count == NULL)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_fifo_locked(addr, out, cap, count, s_retries_read);
    unlock_mbtcp();
    return rc;
}

int mbtcp_read_file_record(uint16_t file, uint16_t record,
                           uint16_t qty, uint16_t *out)
{
    int rc;

    if ((out == NULL) || (qty == 0u) || (qty > 124u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_file_locked(file, record, qty, out, s_retries_read);
    unlock_mbtcp();
    return rc;
}

int mbtcp_write_file_record(uint16_t file, uint16_t record,
                            uint16_t qty, const uint16_t *in)
{
    int rc;

    if ((in == NULL) || (qty == 0u) || (qty > 122u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = write_file_locked(file, record, qty, in, s_retries_write);
    unlock_mbtcp();
    return rc;
}

int mbtcp_read_device_id(uint8_t category,
                         mbtcp_devinfo_cb cb, void *ctx)
{
    return mbtcp_read_device_id_obj(category, 0u, cb, ctx);
}

int mbtcp_read_device_id_obj(uint8_t category, uint8_t obj_id,
                             mbtcp_devinfo_cb cb, void *ctx)
{
    int rc;

    if ((category < 1u) || (category > 4u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = device_id_locked(category, obj_id, cb, ctx);
    unlock_mbtcp();
    return rc;
}

int mbtcp_raw_pdu(const uint8_t *pdu, uint16_t pdu_len,
                  uint8_t *rsp, uint16_t rsp_cap, uint16_t *rsp_len)
{
    uint16_t rlen = 0u;
    int rc;

    if ((pdu == NULL) || (pdu_len == 0u) || (pdu_len > MBTCP_MAX_PDU) ||
        (rsp == NULL) || (rsp_cap < 2u) || (rsp_len == NULL)) {
        return MBTCP_EINVAL;
    }
    *rsp_len = 0u;

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }

    rc = transact_locked(pdu, pdu_len, &rlen, 0u);
    if (rc == MBTCP_OK) {
        if (rlen > rsp_cap) {
            rc = MBTCP_EINVAL;
        } else {
            memcpy(rsp, s_rsp, rlen);
            *rsp_len = rlen;
        }
    }

    unlock_mbtcp();
    return rc;
}

int mbtcp_read(uint8_t unit, uint8_t fc, uint16_t addr,
               uint16_t qty, uint16_t *out)
{
    int rc;
    uint8_t saved;

    if ((out == NULL) || (qty == 0u) || (qty > 125u) ||
        ((fc != 3u) && (fc != 4u))) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    saved = s_unit;
    s_unit = unit;
    rc = read_regs_locked(fc, addr, qty, out, s_retries_read);
    s_unit = saved;
    unlock_mbtcp();
    return rc;
}

int mbtcp_write(uint8_t unit, uint16_t addr, uint16_t value)
{
    int rc;
    uint8_t saved;

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    saved = s_unit;
    s_unit = unit;
    rc = write_register_locked(addr, value, s_retries_write);
    s_unit = saved;
    unlock_mbtcp();
    return rc;
}

int mbtcp_write_multi(uint8_t unit, uint16_t addr,
                      uint16_t qty, const uint16_t *values)
{
    int rc;
    uint8_t saved;

    if ((values == NULL) || (qty == 0u) || (qty > 123u)) {
        return MBTCP_EINVAL;
    }

    rc = lock_mbtcp();
    if (rc != MBTCP_OK) {
        return rc;
    }
    saved = s_unit;
    s_unit = unit;
    rc = write_registers_locked(addr, qty, values, s_retries_write);
    s_unit = saved;
    unlock_mbtcp();
    return rc;
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

const char *mbtcp_exception_str(int code)
{
    switch (code) {
    case 1:  return "illegal function";
    case 2:  return "illegal data address";
    case 3:  return "illegal data value";
    case 4:  return "server device failure";
    case 5:  return "acknowledge";
    case 6:  return "server device busy";
    case 8:  return "memory parity error";
    case 10: return "gateway path unavailable";
    case 11: return "gateway target failed";
    default: return "unknown exception";
    }
}
