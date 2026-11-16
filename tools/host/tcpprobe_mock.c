/*
 * tcpprobe_mock.c - implementacao minima para o teste host.
 * Ativa o log com MBTCP_TEST_VERBOSE=1.
 */
#include "tcpprobe.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int verbose(void)
{
    const char *v = getenv("MBTCP_TEST_VERBOSE");
    return (v != NULL) && (v[0] != '\0') && (v[0] != '0');
}

void tcpprobe_set_logger(void (*fn)(const char *line))
{
    (void)fn;
}

void tcpprobe_log(const char *fmt, ...)
{
    va_list ap;
    int n;
    char buf[TCPPROBE_LOG_BUF];

    if (!verbose() || (fmt == NULL)) {
        return;
    }

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0) {
        if ((size_t)n >= sizeof(buf)) {
            n = (int)sizeof(buf) - 1;
        }
        if ((n > 0) && (buf[n - 1] == '\n')) {
            buf[n - 1] = '\0';
        }
        fprintf(stderr, "[mbtcp] %s\n", buf);
    }
}

void tcpprobe_log_hex(const char *tag, const void *bufp, unsigned len)
{
    const unsigned char *p = (const unsigned char *)bufp;
    unsigned i;

    if (!verbose() || (bufp == NULL)) {
        return;
    }

    fprintf(stderr, "[mbtcp] %s (%u):", (tag != NULL) ? tag : "?", len);
    for (i = 0u; i < len; i++) {
        fprintf(stderr, " %02X", p[i]);
    }
    fputc('\n', stderr);
}

void tcpprobe_env_dump(void)
{
    if (verbose()) {
        fprintf(stderr, "[mbtcp] host test build\n");
    }
}

int tcpprobe_tcp_client(const char *ip, uint16_t port,
                        const void *tx, uint16_t txlen,
                        void *rx, uint16_t rxcap,
                        uint32_t timeout_ms)
{
    (void)ip;
    (void)port;
    (void)tx;
    (void)txlen;
    (void)rx;
    (void)rxcap;
    (void)timeout_ms;
    return TCPPROBE_EINVAL;
}
