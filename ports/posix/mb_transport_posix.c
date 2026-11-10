#define _POSIX_C_SOURCE 200809L

#include "mb_transport_posix.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct mb_conn {
    int fd;
};

static int g_listen_fd = -1;

static int wait_for_fd(int fd, short events, uint32_t timeout_ms)
{
    struct pollfd pfd;
    int timeout = -1;

    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;

    if (timeout_ms != UINT32_MAX) {
        timeout = (timeout_ms > (uint32_t)INT_MAX) ? INT_MAX : (int)timeout_ms;
    }

    for (;;) {
        const int result = poll(&pfd, 1, timeout);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return MB_ERR;
        }
        if (result == 0) {
            return MB_TIMEOUT;
        }
        if ((pfd.revents & POLLERR) || (pfd.revents & POLLNVAL)) {
            return MB_ERR;
        }
        if ((pfd.revents & events) || (pfd.revents & POLLHUP)) {
            return MB_OK;
        }
    }
}

static int posix_listen(void *ctx, uint16_t port)
{
    (void)ctx;

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return MB_ERR;
    }

    const int one = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(fd, (const struct sockaddr *)&address, sizeof(address)) < 0) {
        close(fd);
        return MB_ERR;
    }
    if (listen(fd, 4) < 0) {
        close(fd);
        return MB_ERR;
    }

    if (g_listen_fd >= 0) {
        close(g_listen_fd);
    }
    g_listen_fd = fd;
    return MB_OK;
}

static int posix_accept(void *ctx, mb_conn_t **conn, uint32_t timeout_ms)
{
    (void)ctx;

    if ((conn == NULL) || (g_listen_fd < 0)) {
        return MB_ERR;
    }

    int result = wait_for_fd(g_listen_fd, POLLIN, timeout_ms);
    if (result != MB_OK) {
        return result;
    }

    const int fd = accept(g_listen_fd, NULL, NULL);
    if (fd < 0) {
        if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return MB_TIMEOUT;
        }
        return MB_ERR;
    }

    mb_conn_t *new_conn = calloc(1, sizeof(*new_conn));
    if (new_conn == NULL) {
        close(fd);
        return MB_ERR;
    }

    new_conn->fd = fd;
    *conn = new_conn;
    return MB_OK;
}

static int posix_recv(mb_conn_t *conn, void *buffer, size_t length, uint32_t timeout_ms)
{
    if ((conn == NULL) || (buffer == NULL) || (length == 0u)) {
        return MB_ERR;
    }

    const int ready = wait_for_fd(conn->fd, POLLIN, timeout_ms);
    if (ready != MB_OK) {
        return ready;
    }

    const ssize_t received = recv(conn->fd, buffer, length, 0);
    if (received == 0) {
        return MB_CLOSED;
    }
    if (received < 0) {
        if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return MB_TIMEOUT;
        }
        return MB_ERR;
    }

    return (int)received;
}

static int posix_send(mb_conn_t *conn, const void *buffer, size_t length)
{
    if ((conn == NULL) || ((buffer == NULL) && (length != 0u))) {
        return MB_ERR;
    }

    const uint8_t *bytes = (const uint8_t *)buffer;
    size_t sent = 0u;

    while (sent < length) {
#ifdef MSG_NOSIGNAL
        const ssize_t result = send(conn->fd, &bytes[sent], length - sent, MSG_NOSIGNAL);
#else
        const ssize_t result = send(conn->fd, &bytes[sent], length - sent, 0);
#endif
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return MB_ERR;
        }
        if (result == 0) {
            return MB_ERR;
        }
        sent += (size_t)result;
    }

    return MB_OK;
}

static void posix_close(mb_conn_t *conn)
{
    if (conn == NULL) {
        return;
    }

    close(conn->fd);
    free(conn);
}

static const mb_transport_t g_posix_transport = {
    .ctx = NULL,
    .listen = posix_listen,
    .accept = posix_accept,
    .recv = posix_recv,
    .send = posix_send,
    .close = posix_close,
};

const mb_transport_t *mb_transport_posix_get(void)
{
    return &g_posix_transport;
}
