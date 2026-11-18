/*
 * test_faults.c - injecao de falhas no transporte Modbus TCP.
 *
 * O servidor TCP deste arquivo e' proposital e minimo: ele responde
 * frames especificos para testar timeout, MBAP invalido, transacao
 * errada, excecao Modbus, resposta parcial e reconexao.
 *
 * Roda no host com os mocks de FreeRTOS em tools/host.
 */
#define _POSIX_C_SOURCE 200809L

#include "mbtcp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define CHECK(condition, message)                                          \
    do {                                                                   \
        if (!(condition)) {                                                \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", (message), __FILE__,     \
                    __LINE__);                                             \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

typedef enum {
    SCENARIO_OK = 0,
    SCENARIO_BAD_LENGTH,
    SCENARIO_WRONG_TRANSACTION,
    SCENARIO_EXCEPTION,
    SCENARIO_TIMEOUT,
    SCENARIO_PARTIAL,
    SCENARIO_RECONNECT,
    SCENARIO_TIMEOUT_RETRY
} scenario_t;

typedef struct {
    int listen_fd;
    uint16_t port;
    scenario_t scenario;
    pthread_t thread;
} server_ctx_t;

static void sleep_ms(unsigned milliseconds)
{
    struct timespec delay;

    delay.tv_sec = (time_t)(milliseconds / 1000u);
    delay.tv_nsec = (long)((milliseconds % 1000u) * 1000000u);
    (void)nanosleep(&delay, NULL);
}

static int send_all(int fd, const uint8_t *data, size_t length)
{
    size_t sent = 0u;

    while (sent < length) {
        ssize_t written = send(fd, data + sent, length - sent, MSG_NOSIGNAL);

        if (written <= 0) {
            return -1;
        }
        sent += (size_t)written;
    }
    return 0;
}

static int receive_request(int fd, uint8_t *request, size_t length)
{
    size_t received = 0u;

    while (received < length) {
        ssize_t read_now = recv(fd, request + received, length - received, 0);

        if (read_now <= 0) {
            return -1;
        }
        received += (size_t)read_now;
    }
    return 0;
}

static void set_receive_timeout(int fd, unsigned milliseconds)
{
    struct timeval timeout;

    timeout.tv_sec = (time_t)(milliseconds / 1000u);
    timeout.tv_usec = (suseconds_t)((milliseconds % 1000u) * 1000u);
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

static int accept_with_timeout(int listen_fd, int timeout_ms)
{
    struct pollfd descriptor;
    int rc;

    descriptor.fd = listen_fd;
    descriptor.events = POLLIN;
    descriptor.revents = 0;

    rc = poll(&descriptor, 1, timeout_ms);
    if (rc <= 0) {
        return -1;
    }
    return accept(listen_fd, NULL, NULL);
}

static uint16_t request_transaction_id(const uint8_t *request)
{
    return (uint16_t)(((uint16_t)request[0] << 8) | (uint16_t)request[1]);
}

static void build_read_response(uint8_t frame[11], const uint8_t *request,
                                uint16_t transaction_id, uint16_t value)
{
    frame[0] = (uint8_t)(transaction_id >> 8);
    frame[1] = (uint8_t)(transaction_id & 0xFFu);
    frame[2] = 0u;
    frame[3] = 0u;
    frame[4] = 0u;
    frame[5] = 5u;                 /* unit + fc + byte count + 2 data */
    frame[6] = request[6];
    frame[7] = MBTCP_FUNCTION_READ_HOLDING_REGISTERS;
    frame[8] = 2u;
    frame[9] = (uint8_t)(value >> 8);
    frame[10] = (uint8_t)(value & 0xFFu);
}

static void send_read_response(int fd, const uint8_t *request, uint16_t value,
                               int partial)
{
    uint8_t frame[11];
    size_t index;

    build_read_response(frame, request, request_transaction_id(request), value);

    if (!partial) {
        (void)send_all(fd, frame, sizeof(frame));
        return;
    }

    for (index = 0u; index < sizeof(frame); index++) {
        (void)send_all(fd, &frame[index], 1u);
        sleep_ms(1u);
    }
}

static void send_wrong_transaction(int fd, const uint8_t *request,
                                   uint16_t value)
{
    uint8_t frame[11];
    uint16_t transaction_id = request_transaction_id(request);

    build_read_response(frame, request, (uint16_t)(transaction_id + 1u), value);
    (void)send_all(fd, frame, sizeof(frame));
}

static void send_exception(int fd, const uint8_t *request, uint8_t code)
{
    uint8_t frame[9];

    frame[0] = request[0];
    frame[1] = request[1];
    frame[2] = 0u;
    frame[3] = 0u;
    frame[4] = 0u;
    frame[5] = 3u;                 /* unit + function + exception code */
    frame[6] = request[6];
    frame[7] = (uint8_t)(MBTCP_FUNCTION_READ_HOLDING_REGISTERS | 0x80u);
    frame[8] = code;
    (void)send_all(fd, frame, sizeof(frame));
}

static void send_bad_length(int fd, const uint8_t *request)
{
    uint8_t header[6];

    header[0] = request[0];
    header[1] = request[1];
    header[2] = 0u;
    header[3] = 0u;
    header[4] = 0xFFu;             /* length = 0xFFFF: deve ser rejeitado */
    header[5] = 0xFFu;
    (void)send_all(fd, header, sizeof(header));
}

static void handle_connection(server_ctx_t *ctx, int fd)
{
    uint8_t request[12];

    set_receive_timeout(fd, 500u);

    if (receive_request(fd, request, sizeof(request)) != 0) {
        return;
    }

    switch (ctx->scenario) {
    case SCENARIO_BAD_LENGTH:
        send_bad_length(fd, request);
        break;
    case SCENARIO_WRONG_TRANSACTION:
        send_wrong_transaction(fd, request, 0x1234u);
        break;
    case SCENARIO_EXCEPTION:
        send_exception(fd, request, 2u);
        break;
    case SCENARIO_TIMEOUT:
        sleep_ms(300u);
        break;
    case SCENARIO_PARTIAL:
        send_read_response(fd, request, 0x1234u, 1);
        break;
    default:
        send_read_response(fd, request, 0x1234u, 0);
        break;
    }
}

static void *server_thread(void *argument)
{
    server_ctx_t *ctx = (server_ctx_t *)argument;
    int fd;

    if ((ctx->scenario == SCENARIO_RECONNECT) ||
        (ctx->scenario == SCENARIO_TIMEOUT_RETRY)) {
        uint8_t request[12];

        fd = accept_with_timeout(ctx->listen_fd, 500);
        if (fd < 0) {
            return NULL;
        }
        set_receive_timeout(fd, 500u);
        if (receive_request(fd, request, sizeof(request)) == 0) {
            if (ctx->scenario == SCENARIO_RECONNECT) {
                send_bad_length(fd, request);
            } else {
                sleep_ms(300u);
            }
        }
        close(fd);

        fd = accept_with_timeout(ctx->listen_fd, 500);
        if (fd < 0) {
            return NULL;
        }
        set_receive_timeout(fd, 500u);
        if (receive_request(fd, request, sizeof(request)) == 0) {
            send_read_response(fd, request, 0x1234u, 0);
        }
        close(fd);
        return NULL;
    }

    fd = accept_with_timeout(ctx->listen_fd, 1000);
    if (fd < 0) {
        return NULL;
    }
    handle_connection(ctx, fd);
    close(fd);
    return NULL;
}

static int start_server(server_ctx_t *ctx, scenario_t scenario)
{
    struct sockaddr_in address;
    socklen_t address_length = sizeof(address);
    int reuse = 1;

    memset(ctx, 0, sizeof(*ctx));
    ctx->scenario = scenario;
    ctx->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->listen_fd < 0) {
        return -1;
    }

    (void)setsockopt(ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                     &reuse, sizeof(reuse));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(ctx->listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(ctx->listen_fd);
        return -1;
    }
    if (listen(ctx->listen_fd, 2) < 0) {
        close(ctx->listen_fd);
        return -1;
    }
    if (getsockname(ctx->listen_fd, (struct sockaddr *)&address,
                    &address_length) < 0) {
        close(ctx->listen_fd);
        return -1;
    }
    ctx->port = ntohs(address.sin_port);

    if (pthread_create(&ctx->thread, NULL, server_thread, ctx) != 0) {
        close(ctx->listen_fd);
        return -1;
    }
    return 0;
}

static void stop_server(server_ctx_t *ctx)
{
    (void)pthread_join(ctx->thread, NULL);
    close(ctx->listen_fd);
}

static void open_client_ex(uint16_t port, uint8_t retries,
                           uint32_t timeout_ms)
{
    mbtcp_cfg_t cfg;
    int rc;

    mbtcp_cfg_default(&cfg);
    cfg.ip = "127.0.0.1";
    cfg.port = port;
    cfg.timeout_ms = timeout_ms;
    cfg.retries_read = retries;

    rc = mbtcp_init(&cfg);
    CHECK(rc == MBTCP_OK, "init");
    rc = mbtcp_connect();
    CHECK(rc == MBTCP_OK, "connect");
    CHECK(mbtcp_is_connected(), "connected");
}

static void open_client(uint16_t port)
{
    open_client_ex(port, 0u, 150u);
}

static void run_single_case(scenario_t scenario, int expected_error)
{
    server_ctx_t server;
    uint16_t value = 0u;
    int rc;

    CHECK(start_server(&server, scenario) == 0, "start server");
    open_client(server.port);

    rc = mbtcp_read_holding(1u, 1u, &value);
    CHECK(rc == expected_error, "single case result");
    if (expected_error == MBTCP_EEXCEPTION) {
        CHECK(mbtcp_last_exception() == 2, "exception code");
    }
    if (expected_error == MBTCP_OK) {
        CHECK(value == 0x1234u, "read value");
    }

    mbtcp_disconnect();
    CHECK(!mbtcp_is_connected(), "disconnected after single case");
    stop_server(&server);
}

static void test_reconnect(void)
{
    server_ctx_t server;
    uint16_t value = 0u;
    int rc;

    CHECK(start_server(&server, SCENARIO_RECONNECT) == 0, "start reconnect");
    open_client(server.port);

    rc = mbtcp_read_holding(1u, 1u, &value);
    CHECK(rc == MBTCP_EFRAME, "reconnect first read");

    rc = mbtcp_read_holding(1u, 1u, &value);
    CHECK(rc == MBTCP_OK, "reconnect second read");
    CHECK(value == 0x1234u, "reconnect value");

    mbtcp_disconnect();
    CHECK(!mbtcp_is_connected(), "disconnected after reconnect");
    stop_server(&server);
}

static void test_timeout_retry(void)
{
    server_ctx_t server;
    uint16_t value = 0u;
    int rc;

    CHECK(start_server(&server, SCENARIO_TIMEOUT_RETRY) == 0,
          "start timeout retry");
    open_client_ex(server.port, 1u, 250u);

    rc = mbtcp_read_holding(1u, 1u, &value);
    CHECK(rc == MBTCP_OK, "timeout retry read");
    CHECK(value == 0x1234u, "timeout retry value");

    mbtcp_disconnect();
    CHECK(!mbtcp_is_connected(), "disconnected after timeout retry");
    stop_server(&server);
}

static void test_generic_fc(void)
{
    server_ctx_t server;
    uint16_t value = 0u;

    CHECK(start_server(&server, SCENARIO_OK) == 0, "start generic fc");
    open_client(server.port);

    CHECK(mbtcp_fc(MBTCP_FUNCTION_READ_HOLDING_REGISTERS, 1u, 1u, &value) ==
          MBTCP_OK, "mbtcp_fc read");
    CHECK(value == 0x1234u, "mbtcp_fc value");

    value = 0x5678u;
    CHECK(mbtcp_fc(MBTCP_FUNCTION_WRITE_SINGLE_REGISTER, 1u, 2u, &value) ==
          MBTCP_EINVAL, "mbtcp_fc fc06 quantity");
    CHECK(mbtcp_fc(MBTCP_FUNCTION_READ_HOLDING_REGISTERS, 1u, 0u, &value) ==
          MBTCP_EINVAL, "mbtcp_fc fc03 quantity");
    CHECK(mbtcp_fc(MBTCP_FUNCTION_READ_HOLDING_REGISTERS, 1u, 1u, NULL) ==
          MBTCP_EINVAL, "mbtcp_fc fc03 null");
    CHECK(mbtcp_fc(MBTCP_FUNCTION_WRITE_SINGLE_REGISTER, 1u, 1u, NULL) ==
          MBTCP_EINVAL, "mbtcp_fc fc06 null");
    CHECK(mbtcp_fc(0x10u, 1u, 1u, &value) == MBTCP_EINVAL,
          "mbtcp_fc rejects fc16");

    mbtcp_disconnect();
    CHECK(!mbtcp_is_connected(), "disconnected after generic fc");
    stop_server(&server);
}

int main(void)
{
    run_single_case(SCENARIO_BAD_LENGTH, MBTCP_EFRAME);
    run_single_case(SCENARIO_WRONG_TRANSACTION, MBTCP_EFRAME);
    run_single_case(SCENARIO_EXCEPTION, MBTCP_EEXCEPTION);
    run_single_case(SCENARIO_TIMEOUT, MBTCP_ETIMEOUT);
    run_single_case(SCENARIO_PARTIAL, MBTCP_OK);
    test_reconnect();
    test_timeout_retry();
    test_generic_fc();

    puts("test_faults: ok");
    return 0;
}
