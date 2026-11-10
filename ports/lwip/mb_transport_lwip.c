#include "mb_transport_lwip.h"

#include "lwip/api.h"
#include "lwip/opt.h"
#include "lwip/pbuf.h"

struct mb_conn {
    struct netconn *netconn;
    struct pbuf *pbuf;
    u16_t offset;
};

static struct netconn *g_listener;
static struct mb_conn g_conn;
static bool g_conn_in_use;

static void set_recv_timeout(struct netconn *conn, uint32_t timeout_ms)
{
#if LWIP_SO_RCVTIMEO
    netconn_set_recvtimeout(conn, (u32_t)timeout_ms);
#else
    (void)conn;
    (void)timeout_ms;
#endif
}

static int lwip_listen(void *ctx, uint16_t port)
{
    (void)ctx;

    struct netconn *listener = netconn_new(NETCONN_TCP);
    if (listener == NULL) {
        return MB_ERR;
    }

    err_t err = netconn_bind(listener, IP_ADDR_ANY, port);
    if (err == ERR_OK) {
        err = netconn_listen(listener);
    }

    if (err != ERR_OK) {
        (void)netconn_delete(listener);
        return MB_ERR;
    }

    if (g_listener != NULL) {
        (void)netconn_delete(g_listener);
    }
    g_listener = listener;
    return MB_OK;
}

static int lwip_accept(void *ctx, mb_conn_t **conn, uint32_t timeout_ms)
{
    (void)ctx;

    if ((conn == NULL) || (g_listener == NULL) || g_conn_in_use) {
        return MB_ERR;
    }

    set_recv_timeout(g_listener, timeout_ms);

    struct netconn *accepted = NULL;
    const err_t err = netconn_accept(g_listener, &accepted);
    if (err == ERR_TIMEOUT) {
        return MB_TIMEOUT;
    }
    if ((err == ERR_CLSD) || (err == ERR_RST) || (err == ERR_ABRT)) {
        return MB_CLOSED;
    }
    if (err != ERR_OK) {
        return MB_ERR;
    }

    g_conn.netconn = accepted;
    g_conn.pbuf = NULL;
    g_conn.offset = 0u;
    g_conn_in_use = true;

    *conn = &g_conn;
    return MB_OK;
}

static int lwip_recv(mb_conn_t *conn, void *buffer, size_t length, uint32_t timeout_ms)
{
    if ((conn == NULL) || (buffer == NULL) || (length == 0u)) {
        return MB_ERR;
    }

    for (;;) {
        if (conn->pbuf == NULL) {
            set_recv_timeout(conn->netconn, timeout_ms);

            struct pbuf *received = NULL;
            const err_t err = netconn_recv_tcp_pbuf(conn->netconn, &received);
            if (err == ERR_TIMEOUT) {
                return MB_TIMEOUT;
            }
            if ((err == ERR_CLSD) || (err == ERR_RST) || (err == ERR_ABRT)) {
                return MB_CLOSED;
            }
            if (err != ERR_OK) {
                return MB_ERR;
            }
            if (received == NULL) {
                return MB_CLOSED;
            }

            conn->pbuf = received;
            conn->offset = 0u;
        }

        if (conn->offset >= conn->pbuf->tot_len) {
            (void)pbuf_free(conn->pbuf);
            conn->pbuf = NULL;
            conn->offset = 0u;
            continue;
        }

        u16_t available = (u16_t)(conn->pbuf->tot_len - conn->offset);
        if (length < (size_t)available) {
            available = (u16_t)length;
        }

        const u16_t copied =
            pbuf_copy_partial(conn->pbuf, buffer, available, conn->offset);
        conn->offset = (u16_t)(conn->offset + copied);

        if (conn->offset >= conn->pbuf->tot_len) {
            (void)pbuf_free(conn->pbuf);
            conn->pbuf = NULL;
            conn->offset = 0u;
        }

        if (copied == 0u) {
            return MB_ERR;
        }

        return (int)copied;
    }
}

static int lwip_send(mb_conn_t *conn, const void *buffer, size_t length)
{
    if ((conn == NULL) || ((buffer == NULL) && (length != 0u))) {
        return MB_ERR;
    }

    const err_t err = netconn_write(conn->netconn, buffer, length, NETCONN_COPY);
    return (err == ERR_OK) ? MB_OK : MB_ERR;
}

static void lwip_close(mb_conn_t *conn)
{
    if (conn == NULL) {
        return;
    }

    if (conn->pbuf != NULL) {
        (void)pbuf_free(conn->pbuf);
        conn->pbuf = NULL;
    }
    if (conn->netconn != NULL) {
        (void)netconn_close(conn->netconn);
        (void)netconn_delete(conn->netconn);
        conn->netconn = NULL;
    }
    conn->offset = 0u;
    g_conn_in_use = false;
}

static const mb_transport_t g_lwip_transport = {
    .ctx = NULL,
    .listen = lwip_listen,
    .accept = lwip_accept,
    .recv = lwip_recv,
    .send = lwip_send,
    .close = lwip_close,
};

const mb_transport_t *mb_transport_lwip_get(void)
{
    return &g_lwip_transport;
}
