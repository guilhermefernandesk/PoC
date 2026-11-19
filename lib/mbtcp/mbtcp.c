/*
 * mbtcp.c - cliente Modbus TCP sobre FreeRTOS-Plus-TCP
 *
 * Decisoes:
 *  - buffers TX/RX estaticos: o mutex serializa, entao nao precisam ficar
 *    na stack de cada task chamadora;
 *  - a resposta e' lida pelo campo "length" do MBAP, nunca por chute;
 *  - timeout/erro de frame fecha o socket para ressincronizar; a proxima
 *    chamada reconecta sozinha;
 *  - excecao Modbus nao fecha o socket, porque a resposta e' valida;
 *  - leitura pode repetir; escrita nunca e' repetida;
 *  - sem task permanente: a aplicacao chama a API de uma task existente.
 */
#include "mbtcp.h"
#include "uhx15a.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* ------------------------------------------------------------------ */
/* Protocolo                                                           */
/* ------------------------------------------------------------------ */

#define MODBUS_TCP_MBAP_PREFIX_SIZE 6u
#define MODBUS_TCP_HEADER_SIZE 7u
#define MODBUS_TCP_LENGTH_OFFSET 4u
#define MODBUS_TCP_UNIT_OFFSET 6u
#define MODBUS_PROTOCOL_ID 0u
#define MODBUS_EXCEPTION_FLAG 0x80u
#define MODBUS_EXCEPTION_PDU_SIZE 2u
#define MODBUS_MAX_READ_REGISTERS 125u
#define MODBUS_MAX_READ_WRITE_WRITE_REGISTERS 121u

/* O campo length do MBAP inclui o unit ID e a PDU. */
#define MODBUS_MIN_LENGTH_FIELD 2u
#define MODBUS_MAX_LENGTH_FIELD (MBTCP_MAX_FRAME - MODBUS_TCP_MBAP_PREFIX_SIZE)

#define MODBUS_MAX_READ_WRITE_PDU_SIZE \
    (10u + (2u * MODBUS_MAX_READ_WRITE_WRITE_REGISTERS))

/* ------------------------------------------------------------------ */
/* Estado                                                              */
/* ------------------------------------------------------------------ */

static Socket_t s_socket = FREERTOS_INVALID_SOCKET;
static SemaphoreHandle_t s_mutex = NULL;

static uint32_t s_ip_address = 0u;
static char s_ip_text[16];
static uint16_t s_port = MBTCP_DEFAULT_PORT;
static uint8_t s_unit = MBTCP_DEFAULT_UNIT;
static uint32_t s_timeout_ms = MBTCP_DEFAULT_TIMEOUT_MS;
static uint8_t s_retries_read = 0u;
static uint16_t s_rx_buffer_size = 0u;
static uint16_t s_tx_buffer_size = 0u;
static uint8_t s_no_delay = 1u;
static uint16_t s_transaction_id = 0u;
static int s_last_exception = 0;
static mbtcp_stats_t s_stats;

/* Protegidos pelo mutex. */
static uint8_t s_tx_frame[MBTCP_MAX_FRAME];
static uint8_t s_rx_frame[MBTCP_MAX_FRAME];

/* ------------------------------------------------------------------ */
/* Utilitarios                                                         */
/* ------------------------------------------------------------------ */

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)(value & 0xFFu);
}

static uint8_t clamp_to_u8(uint8_t value, uint8_t maximum)
{
    return (value > maximum) ? maximum : value;
}

static uint32_t clamp_timeout(uint32_t timeout_ms)
{
    if (timeout_ms == 0u) {
        return MBTCP_DEFAULT_TIMEOUT_MS;
    }
    if (timeout_ms > MBTCP_MAX_TIMEOUT_MS) {
        return MBTCP_MAX_TIMEOUT_MS;
    }
    return timeout_ms;
}

static TickType_t timeout_ticks(void)
{
    TickType_t ticks = pdMS_TO_TICKS(s_timeout_ms);

    return (ticks == 0u) ? 1u : ticks;
}

static TickType_t ticks_until(TickType_t deadline)
{
    TickType_t now = xTaskGetTickCount();

    if ((int32_t)(deadline - now) <= 0) {
        return 0u;
    }
    return (TickType_t)(deadline - now);
}

static int set_socket_timeout(uint32_t option, TickType_t ticks)
{
    TickType_t value = (ticks == 0u) ? 1u : ticks;

    if (FreeRTOS_setsockopt(s_socket, 0, (int32_t)option,
                            &value, sizeof(value)) != 0) {
        return MBTCP_ECONN;
    }
    return MBTCP_OK;
}

static int ensure_mutex(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            return MBTCP_ECONN;
        }
    }
    return MBTCP_OK;
}

static int lock_client(void)
{
    int rc = ensure_mutex();

    if (rc != MBTCP_OK) {
        return rc;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(MBTCP_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return MBTCP_EBUSY;
    }
    return MBTCP_OK;
}

static void unlock_client(void)
{
    (void)xSemaphoreGive(s_mutex);
}

/* ------------------------------------------------------------------ */
/* Socket                                                              */
/* ------------------------------------------------------------------ */

static void close_socket(void)
{
    if (s_socket != FREERTOS_INVALID_SOCKET) {
        (void)FreeRTOS_shutdown(s_socket, FREERTOS_SHUT_RDWR);
        (void)FreeRTOS_closesocket(s_socket);
        s_socket = FREERTOS_INVALID_SOCKET;
    }
    s_stats.connected = 0u;
}

static int open_socket(void)
{
    struct freertos_sockaddr address;
    TickType_t ticks = timeout_ticks();

    s_socket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM,
                               FREERTOS_IPPROTO_TCP);
    if (s_socket == FREERTOS_INVALID_SOCKET) {
        s_stats.connect_errors++;
        return MBTCP_ECONN;
    }

    if ((set_socket_timeout(FREERTOS_SO_RCVTIMEO, ticks) != MBTCP_OK) ||
        (set_socket_timeout(FREERTOS_SO_SNDTIMEO, ticks) != MBTCP_OK)) {
        close_socket();
        s_stats.connect_errors++;
        return MBTCP_ECONN;
    }

#ifdef FREERTOS_SO_RCVBUF
    if (s_rx_buffer_size != 0u) {
        uint32_t value = (uint32_t)s_rx_buffer_size;
        (void)FreeRTOS_setsockopt(s_socket, 0, FREERTOS_SO_RCVBUF,
                                  &value, sizeof(value));
    }
#endif
#ifdef FREERTOS_SO_SNDBUF
    if (s_tx_buffer_size != 0u) {
        uint32_t value = (uint32_t)s_tx_buffer_size;
        (void)FreeRTOS_setsockopt(s_socket, 0, FREERTOS_SO_SNDBUF,
                                  &value, sizeof(value));
    }
#endif
#ifdef FREERTOS_SO_TCPNODELAY
    if (s_no_delay != 0u) {
        BaseType_t value = pdTRUE;
        (void)FreeRTOS_setsockopt(s_socket, 0, FREERTOS_SO_TCPNODELAY,
                                  &value, sizeof(value));
    }
#endif

    memset(&address, 0, sizeof(address));
    address.sin_family = (uint8_t)FREERTOS_AF_INET;
    address.sin_port = FreeRTOS_htons(s_port);
    address.sin_addr = s_ip_address;

    if (FreeRTOS_connect(s_socket, &address, sizeof(address)) < 0) {
        s_stats.connect_errors++;
        close_socket();
        return MBTCP_ECONN;
    }

    s_stats.connected = 1u;
    s_stats.connects++;
    return MBTCP_OK;
}

static int ensure_socket_open(void)
{
    if (s_socket != FREERTOS_INVALID_SOCKET) {
        return MBTCP_OK;
    }
    return open_socket();
}

static int send_all(const uint8_t *data, uint16_t length, TickType_t deadline)
{
    uint16_t sent = 0u;

    while (sent < length) {
        TickType_t remaining = ticks_until(deadline);
        int32_t written;
        int rc;

        if (remaining == 0u) {
            return MBTCP_ETIMEOUT;
        }
        rc = set_socket_timeout(FREERTOS_SO_SNDTIMEO, remaining);
        if (rc != MBTCP_OK) {
            return rc;
        }

        written = FreeRTOS_send(s_socket, data + sent,
                                (size_t)(length - sent), 0);
        if (written <= 0) {
            return MBTCP_ESEND;
        }
        sent = (uint16_t)(sent + (uint16_t)written);
    }

    return MBTCP_OK;
}

static int receive_all(uint8_t *data, uint16_t length, TickType_t deadline)
{
    uint16_t received = 0u;

    while (received < length) {
        TickType_t remaining = ticks_until(deadline);
        int32_t read_now;
        int rc;

        if (remaining == 0u) {
            return MBTCP_ETIMEOUT;
        }
        rc = set_socket_timeout(FREERTOS_SO_RCVTIMEO, remaining);
        if (rc != MBTCP_OK) {
            return rc;
        }

        read_now = FreeRTOS_recv(s_socket, data + received,
                                 (size_t)(length - received), 0);
        if (read_now == 0) {
            return MBTCP_ECONN;       /* peer fechou */
        }
        if (read_now < 0) {
            return MBTCP_ETIMEOUT;
        }
        received = (uint16_t)(received + (uint16_t)read_now);
    }

    return MBTCP_OK;
}

/*
 * Le um frame completo: primeiro os 6 bytes do MBAP, depois o restante
 * indicado pelo campo length. O buffer nunca passa de MBTCP_MAX_FRAME.
 */
static int receive_response_frame(uint16_t *frame_length, TickType_t deadline)
{
    uint16_t length_field;
    int rc;

    rc = receive_all(s_rx_frame, MODBUS_TCP_MBAP_PREFIX_SIZE, deadline);
    if (rc != MBTCP_OK) {
        return rc;
    }

    length_field = read_u16(s_rx_frame + MODBUS_TCP_LENGTH_OFFSET);
    if ((length_field < MODBUS_MIN_LENGTH_FIELD) ||
        (length_field > MODBUS_MAX_LENGTH_FIELD)) {
        return MBTCP_EFRAME;
    }

    rc = receive_all(s_rx_frame + MODBUS_TCP_MBAP_PREFIX_SIZE,
                     length_field, deadline);
    if (rc != MBTCP_OK) {
        return rc;
    }

    *frame_length = (uint16_t)(MODBUS_TCP_MBAP_PREFIX_SIZE + length_field);
    return MBTCP_OK;
}

static int bad_frame(void)
{
    s_stats.frame_errors++;
    s_stats.last_error = MBTCP_EFRAME;
    close_socket();
    return MBTCP_EFRAME;
}

static int is_retryable_error(int error)
{
    return (error == MBTCP_ECONN) ||
           (error == MBTCP_ESEND) ||
           (error == MBTCP_ETIMEOUT);
}

/* ------------------------------------------------------------------ */
/* Transacao                                                           */
/* ------------------------------------------------------------------ */

/*
 * Executa uma transacao e devolve um ponteiro para a PDU de resposta
 * dentro de s_rx_frame. O ponteiro vale ate a proxima transacao.
 * Assume o mutex tomado.
 */
static int perform_transaction(const uint8_t *request_pdu,
                               uint16_t request_pdu_length,
                               const uint8_t **response_pdu,
                               uint16_t *response_pdu_length,
                               uint8_t retries)
{
    uint16_t attempt;
    int rc = MBTCP_ECONN;

    if ((request_pdu == NULL) || (request_pdu_length == 0u) ||
        (request_pdu_length > MBTCP_MAX_PDU) ||
        (response_pdu == NULL) || (response_pdu_length == NULL)) {
        return MBTCP_EINVAL;
    }

    s_last_exception = 0;

    for (attempt = 0u; attempt <= retries; attempt++) {
        uint16_t transaction_id;
        uint16_t response_frame_length = 0u;
        uint16_t request_frame_length =
            (uint16_t)(MODBUS_TCP_HEADER_SIZE + request_pdu_length);
        uint16_t response_length;
        uint16_t response_transaction_id;
        uint16_t response_protocol_id;
        uint16_t response_length_field;
        uint8_t response_unit;
        TickType_t started;
        TickType_t deadline;

        if (attempt != 0u) {
            s_stats.retries++;
        }

        rc = ensure_socket_open();
        if (rc != MBTCP_OK) {
            s_stats.last_error = rc;
            if (attempt < retries) {
                continue;
            }
            break;
        }

        transaction_id = ++s_transaction_id;
        write_u16(s_tx_frame + 0u, transaction_id);
        write_u16(s_tx_frame + 2u, MODBUS_PROTOCOL_ID);
        write_u16(s_tx_frame + MODBUS_TCP_LENGTH_OFFSET,
                  (uint16_t)(1u + request_pdu_length));
        s_tx_frame[MODBUS_TCP_UNIT_OFFSET] = s_unit;
        memcpy(s_tx_frame + MODBUS_TCP_HEADER_SIZE,
               request_pdu, request_pdu_length);

        s_stats.tx_count++;

        started = xTaskGetTickCount();
        deadline = started + timeout_ticks();

        rc = send_all(s_tx_frame, request_frame_length, deadline);
        if (rc == MBTCP_OK) {
            rc = receive_response_frame(&response_frame_length, deadline);
        }

        s_stats.latency_ms = (uint32_t)(xTaskGetTickCount() - started) *
                             (uint32_t)portTICK_PERIOD_MS;

        if (rc != MBTCP_OK) {
            s_stats.last_error = rc;
            if (rc == MBTCP_ETIMEOUT) {
                s_stats.timeouts++;
            } else if (rc == MBTCP_EFRAME) {
                s_stats.frame_errors++;
            }
            close_socket();
            if ((attempt < retries) && is_retryable_error(rc)) {
                continue;
            }
            break;
        }

        if (response_frame_length < (MODBUS_TCP_HEADER_SIZE + 1u)) {
            return bad_frame();
        }

        response_transaction_id = read_u16(s_rx_frame + 0u);
        response_protocol_id = read_u16(s_rx_frame + 2u);
        response_length_field = read_u16(s_rx_frame + MODBUS_TCP_LENGTH_OFFSET);
        response_unit = s_rx_frame[MODBUS_TCP_UNIT_OFFSET];

        if ((response_transaction_id != transaction_id) ||
            (response_protocol_id != MODBUS_PROTOCOL_ID) ||
            (response_unit != s_unit) ||
            (response_length_field !=
             (uint16_t)(response_frame_length - MODBUS_TCP_MBAP_PREFIX_SIZE))) {
            return bad_frame();
        }

        response_length = (uint16_t)(response_frame_length -
                                      MODBUS_TCP_HEADER_SIZE);
        *response_pdu = s_rx_frame + MODBUS_TCP_HEADER_SIZE;
        *response_pdu_length = response_length;

        if ((*response_pdu)[0] ==
            (uint8_t)(request_pdu[0] | MODBUS_EXCEPTION_FLAG)) {
            if (response_length != MODBUS_EXCEPTION_PDU_SIZE) {
                return bad_frame();
            }
            s_last_exception = (int)(*response_pdu)[1];
            s_stats.rx_count++;
            s_stats.exceptions++;
            s_stats.last_error = MBTCP_EEXCEPTION;
            rc = MBTCP_EEXCEPTION;
            break;
        }

        if ((*response_pdu)[0] != request_pdu[0]) {
            return bad_frame();
        }

        s_stats.rx_count++;
        s_stats.last_error = MBTCP_OK;
        rc = MBTCP_OK;
        break;
    }

    return rc;
}

/* ------------------------------------------------------------------ */
/* Operacoes internas (assumem o mutex tomado)                         */
/* ------------------------------------------------------------------ */

static int read_holding_registers(uint16_t address, uint16_t quantity,
                                  uint16_t *registers, uint8_t retries)
{
    uint8_t request[5];
    const uint8_t *response;
    uint16_t response_length;
    uint16_t byte_count = (uint16_t)(2u * quantity);
    uint16_t index;
    int rc;

    request[0] = MBTCP_FUNCTION_READ_HOLDING_REGISTERS;
    write_u16(request + 1u, address);
    write_u16(request + 3u, quantity);

    rc = perform_transaction(request, sizeof(request), &response,
                             &response_length, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }

    if ((response_length != (uint16_t)(2u + byte_count)) ||
        (response[1] != (uint8_t)byte_count)) {
        return bad_frame();
    }

    for (index = 0u; index < quantity; index++) {
        registers[index] = read_u16(response + 2u + (2u * index));
    }
    return MBTCP_OK;
}

static int write_single_register(uint16_t address, uint16_t value,
                                 uint8_t retries)
{
    uint8_t request[5];
    const uint8_t *response;
    uint16_t response_length;
    int rc;

    request[0] = MBTCP_FUNCTION_WRITE_SINGLE_REGISTER;
    write_u16(request + 1u, address);
    write_u16(request + 3u, value);

    rc = perform_transaction(request, sizeof(request), &response,
                             &response_length, retries);
    if (rc != MBTCP_OK) {
        return rc;
    }

    if ((response_length != (uint16_t)sizeof(request)) ||
        (memcmp(response, request, sizeof(request)) != 0)) {
        return bad_frame();
    }
    return MBTCP_OK;
}

static int read_write_registers(uint16_t read_address, uint16_t read_quantity,
                                uint16_t *read_registers,
                                uint16_t write_address, uint16_t write_quantity,
                                const uint16_t *write_registers)
{
    uint8_t request[MODBUS_MAX_READ_WRITE_PDU_SIZE];
    const uint8_t *response;
    uint16_t response_length;
    uint16_t request_length =
        (uint16_t)(10u + (2u * write_quantity));
    uint16_t byte_count = (uint16_t)(2u * read_quantity);
    uint16_t index;
    int rc;

    request[0] = MBTCP_FUNCTION_READ_WRITE_MULTIPLE_REGISTERS;
    write_u16(request + 1u, read_address);
    write_u16(request + 3u, read_quantity);
    write_u16(request + 5u, write_address);
    write_u16(request + 7u, write_quantity);
    request[9] = (uint8_t)(2u * write_quantity);

    for (index = 0u; index < write_quantity; index++) {
        write_u16(request + 10u + (2u * index), write_registers[index]);
    }

    /*
     * FC23 contem uma escrita. Mesmo que a leitura falhe, repetir a
     * transacao pode repetir a escrita, entao retries fica sempre em 0.
     */
    rc = perform_transaction(request, request_length, &response,
                             &response_length, 0u);
    if (rc != MBTCP_OK) {
        return rc;
    }

    if ((response_length != (uint16_t)(2u + byte_count)) ||
        (response[1] != (uint8_t)byte_count)) {
        return bad_frame();
    }

    for (index = 0u; index < read_quantity; index++) {
        read_registers[index] = read_u16(response + 2u + (2u * index));
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
    cfg->port = MBTCP_DEFAULT_PORT;
    cfg->unit = MBTCP_DEFAULT_UNIT;
    cfg->timeout_ms = MBTCP_DEFAULT_TIMEOUT_MS;
    cfg->retries_read = 0u;
    cfg->nodelay = 1u;
}

int mbtcp_init(const mbtcp_cfg_t *cfg)
{
    uint32_t ip_address;
    int rc;

    if ((cfg == NULL) || (cfg->ip == NULL)) {
        return MBTCP_EINVAL;
    }

    ip_address = FreeRTOS_inet_addr(cfg->ip);
    if (ip_address == 0u) {
        return MBTCP_EINVAL;
    }

    rc = ensure_mutex();
    if (rc != MBTCP_OK) {
        return rc;
    }

    rc = lock_client();
    if (rc != MBTCP_OK) {
        return rc;
    }

    close_socket();

    memset(s_ip_text, 0, sizeof(s_ip_text));
    strncpy(s_ip_text, cfg->ip, sizeof(s_ip_text) - 1u);
    s_ip_address = ip_address;
    s_port = (cfg->port != 0u) ? cfg->port : MBTCP_DEFAULT_PORT;
    s_unit = cfg->unit;
    s_timeout_ms = clamp_timeout(cfg->timeout_ms);
    s_retries_read = clamp_to_u8(cfg->retries_read, MBTCP_MAX_RETRIES);
    s_rx_buffer_size = cfg->rx_buf;
    s_tx_buffer_size = cfg->tx_buf;
    s_no_delay = cfg->nodelay;
    s_transaction_id = 0u;
    s_last_exception = 0;
    memset(&s_stats, 0, sizeof(s_stats));

    unlock_client();
    return MBTCP_OK;
}

int mbtcp_connect(void)
{
    int rc = lock_client();

    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = ensure_socket_open();
    unlock_client();
    return rc;
}

void mbtcp_disconnect(void)
{
    if (s_mutex == NULL) {
        close_socket();
        return;
    }
    if (lock_client() == MBTCP_OK) {
        close_socket();
        unlock_client();
    }
}

int mbtcp_set_unit(uint8_t unit)
{
    int rc = lock_client();

    if (rc != MBTCP_OK) {
        return rc;
    }
    s_unit = unit;
    unlock_client();
    return MBTCP_OK;
}

const mbtcp_stats_t *mbtcp_stats(void)
{
    return &s_stats;
}

bool mbtcp_is_connected(void)
{
    return (s_stats.connected != 0u);
}

int mbtcp_read_holding(uint16_t address, uint16_t quantity,
                       uint16_t *registers)
{
    int rc;

    if ((registers == NULL) || (quantity == 0u) ||
        (quantity > MODBUS_MAX_READ_REGISTERS)) {
        return MBTCP_EINVAL;
    }

    rc = lock_client();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_holding_registers(address, quantity, registers, s_retries_read);
    unlock_client();
    return rc;
}

int mbtcp_write_register(uint16_t address, uint16_t value)
{
    int rc = lock_client();

    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = write_single_register(address, value, 0u);
    unlock_client();
    return rc;
}

int mbtcp_read_write_registers(uint16_t read_address, uint16_t read_quantity,
                               uint16_t *read_registers,
                               uint16_t write_address, uint16_t write_quantity,
                               const uint16_t *write_registers)
{
    int rc;

    if ((read_registers == NULL) || (read_quantity == 0u) ||
        (read_quantity > MODBUS_MAX_READ_REGISTERS) ||
        (write_registers == NULL) || (write_quantity == 0u) ||
        (write_quantity > MODBUS_MAX_READ_WRITE_WRITE_REGISTERS)) {
        return MBTCP_EINVAL;
    }

    rc = lock_client();
    if (rc != MBTCP_OK) {
        return rc;
    }
    rc = read_write_registers(read_address, read_quantity, read_registers,
                              write_address, write_quantity, write_registers);
    unlock_client();
    return rc;
}

int mbtcp_fc(uint8_t fc, uint16_t address, uint16_t quantity,
             uint16_t *registers)
{
    switch (fc) {
    case MBTCP_FUNCTION_READ_HOLDING_REGISTERS:
        return mbtcp_read_holding(address, quantity, registers);
    case MBTCP_FUNCTION_WRITE_SINGLE_REGISTER:
        if ((registers == NULL) || (quantity != 1u)) {
            return MBTCP_EINVAL;
        }
        return mbtcp_write_register(address, registers[0]);
    default:
        return MBTCP_EINVAL;
    }
}

int mbtcp_raw_pdu(const uint8_t *request_pdu, uint16_t request_pdu_length,
                  uint8_t *response_pdu, uint16_t response_capacity,
                  uint16_t *response_pdu_length)
{
    const uint8_t *response;
    uint16_t response_length = 0u;
    int rc;

    if ((request_pdu == NULL) || (request_pdu_length == 0u) ||
        (request_pdu_length > MBTCP_MAX_PDU) ||
        (response_pdu == NULL) || (response_capacity == 0u) ||
        (response_pdu_length == NULL)) {
        return MBTCP_EINVAL;
    }
    *response_pdu_length = 0u;

    rc = lock_client();
    if (rc != MBTCP_OK) {
        return rc;
    }

    rc = perform_transaction(request_pdu, request_pdu_length, &response,
                             &response_length, 0u);
    if (rc == MBTCP_OK) {
        if (response_length > response_capacity) {
            s_stats.last_error = MBTCP_EINVAL;
            rc = MBTCP_EINVAL;
        } else {
            memcpy(response_pdu, response, response_length);
            *response_pdu_length = response_length;
        }
    }

    unlock_client();
    return rc;
}

int mbtcp_last_exception(void)
{
    return s_last_exception;
}

const char *mbtcp_strerror(int error)
{
    switch (error) {
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

const char *mbtcp_exception_str(int exception_code)
{
    switch (exception_code) {
    case 1:  return "illegal function";
    case 2:  return "illegal data address";
    case 3:  return "illegal data value";
    case 4:  return "server device failure";
    case 5:  return "acknowledge";
    case 6:  return "server device busy";
    case 7:  return "negative acknowledge";
    case 8:  return "memory parity error";
    case 10: return "gateway path unavailable";
    case 11: return "gateway target failed";
    default: return "unknown exception";
    }
}