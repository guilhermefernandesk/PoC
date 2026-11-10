#ifndef MB_TRANSPORT_H
#define MB_TRANSPORT_H

#include "mb_types.h"

typedef struct mb_conn mb_conn_t;

/*
 * Transport contract
 * ------------------
 * listen()  : prepare the listening endpoint. Returns MB_OK or MB_ERR.
 * accept()  : wait for a client. Returns MB_OK with *conn set, MB_TIMEOUT,
 *             MB_ERR, or MB_CLOSED.
 * recv()    : return >0 bytes read, 0/MB_CLOSED on orderly close,
 *             MB_TIMEOUT on timeout, MB_ERR on error.
 * send()    : return MB_OK when all bytes were sent, MB_ERR otherwise.
 * close()   : release the connection.
 *
 * The Modbus core never sees LwIP, HAL, FreeRTOS or POSIX headers.
 */
typedef struct {
    void *ctx;
    int (*listen)(void *ctx, uint16_t port);
    int (*accept)(void *ctx, mb_conn_t **conn, uint32_t timeout_ms);
    int (*recv)(mb_conn_t *conn, void *buf, size_t len, uint32_t timeout_ms);
    int (*send)(mb_conn_t *conn, const void *buf, size_t len);
    void (*close)(mb_conn_t *conn);
} mb_transport_t;

#endif
