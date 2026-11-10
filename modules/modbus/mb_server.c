#include "mb_server.h"

#include "mb_tcp.h"

#include <string.h>

static int recv_exact(const mb_transport_t *transport,
                      mb_conn_t *conn,
                      uint8_t *buffer,
                      size_t length,
                      uint32_t timeout_ms)
{
    size_t received = 0u;

    while (received < length) {
        const int result =
            transport->recv(conn, &buffer[received], length - received, timeout_ms);
        if (result == MB_TIMEOUT) {
            return MB_TIMEOUT;
        }
        if ((result == MB_CLOSED) || (result == 0)) {
            return MB_CLOSED;
        }
        if (result < 0) {
            return MB_ERR;
        }
        received += (size_t)result;
    }

    return MB_OK;
}

static void handle_connection(const mb_server_cfg_t *cfg, mb_conn_t *conn)
{
    uint8_t header[MB_MBAP_SIZE];
    uint8_t adu[MB_MAX_ADU];
    uint8_t response[MB_MAX_ADU];

    for (;;) {
        int result = recv_exact(cfg->transport, conn, header, sizeof(header),
                                cfg->recv_timeout_ms);
        if (result != MB_OK) {
            return;
        }

        const uint16_t length =
            (uint16_t)(((uint16_t)header[4] << 8) | (uint16_t)header[5]);
        if ((length < 2u) || (length > 254u)) {
            return;
        }

        const size_t pdu_len = (size_t)length - 1u;
        if ((MB_MBAP_SIZE + pdu_len) > sizeof(adu)) {
            return;
        }

        memcpy(adu, header, sizeof(header));
        result = recv_exact(cfg->transport, conn, &adu[MB_MBAP_SIZE], pdu_len,
                            cfg->recv_timeout_ms);
        if (result != MB_OK) {
            return;
        }

        size_t response_len = 0u;
        result = mb_tcp_process(cfg->map, adu, MB_MBAP_SIZE + pdu_len,
                                response, sizeof(response), &response_len);
        if (result < 0) {
            return;
        }
        if (result == 0) {
            continue;
        }

        if (cfg->transport->send(conn, response, response_len) != MB_OK) {
            return;
        }
    }
}

int mb_server_run(const mb_server_cfg_t *cfg)
{
    if ((cfg == NULL) || (cfg->transport == NULL) || (cfg->map == NULL)) {
        return MB_ERR;
    }

    if ((cfg->transport->listen == NULL) || (cfg->transport->accept == NULL) ||
        (cfg->transport->recv == NULL) || (cfg->transport->send == NULL) ||
        (cfg->transport->close == NULL)) {
        return MB_ERR;
    }

    if (cfg->transport->listen(cfg->transport->ctx, cfg->port) != MB_OK) {
        return MB_ERR;
    }

    for (;;) {
        mb_conn_t *conn = NULL;
        const int result = cfg->transport->accept(cfg->transport->ctx, &conn,
                                                  cfg->accept_timeout_ms);

        if (result == MB_TIMEOUT) {
            continue;
        }
        if ((result != MB_OK) || (conn == NULL)) {
            continue;
        }

        handle_connection(cfg, conn);
        cfg->transport->close(conn);
    }
}
