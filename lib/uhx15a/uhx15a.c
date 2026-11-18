/*
 * uhx15a.c - driver do controlador UHX15A
 *
 * Codigo apenas. Toda configuracao do equipamento esta em
 * uhx15a_config.h.
 */
#include "uhx15a.h"
#include "uhx15a_config.h"

#include "mbtcp.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    bool valid;
    uint16_t status_word;
    int32_t raw_position;
    float position_deg;
} uhx15a_axis_state_t;

static uhx15a_axis_state_t s_axis[UHX15A_AXIS_COUNT];
static bool s_initialized = false;
static bool s_online = false;
static const char *s_last_error = "ok";

static bool axis_valid(uhx15a_axis_t axis)
{
    return ((unsigned)axis < (unsigned)UHX15A_AXIS_COUNT);
}

static uhx15a_result_t fail(uhx15a_result_t result, const char *message)
{
    s_last_error = message;
    return result;
}

static uhx15a_result_t map_error(int rc)
{
    switch (rc) {
    case MBTCP_OK:          return UHX15A_OK;
    case MBTCP_ECONN:       return UHX15A_ERR_LINK;
    case MBTCP_ESEND:       return UHX15A_ERR_LINK;
    case MBTCP_ETIMEOUT:    return UHX15A_ERR_TIMEOUT;
    case MBTCP_EFRAME:      return UHX15A_ERR_PROTOCOL;
    case MBTCP_EEXCEPTION:  return UHX15A_ERR_MODBUS;
    case MBTCP_EBUSY:       return UHX15A_ERR_BUSY;
    case MBTCP_EINVAL:      return UHX15A_ERR_ARG;
    default:                return UHX15A_ERR_LINK;
    }
}

static void invalidate_axes(void)
{
    unsigned index;

    for (index = 0u; index < UHX15A_AXIS_COUNT; index++) {
        s_axis[index].valid = false;
    }
}

static int32_t registers_to_s32(uint16_t high, uint16_t low)
{
    uint32_t raw = ((uint32_t)high << 16) | (uint32_t)low;

    return (int32_t)raw;
}

static void s32_to_registers(int32_t value, uint16_t registers[2])
{
    uint32_t raw = (uint32_t)value;

    registers[0] = (uint16_t)(raw >> 16);
    registers[1] = (uint16_t)(raw & 0xFFFFu);
}

static int32_t position_to_raw(float position_deg)
{
    float scaled = (position_deg - UHX15A_POSITION_OFFSET) /
                   UHX15A_POSITION_SCALE;

    /* Arredonda para o inteiro mais proximo sem depender de libm. */
    return (int32_t)((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
}

static void decode_axis(uhx15a_axis_t axis, uint16_t status_address,
                        uint16_t position_address,
                        const uint16_t *registers)
{
    uint16_t status = registers[status_address - UHX15A_POLL_START];
    uint16_t high = registers[position_address - UHX15A_POLL_START];
    uint16_t low = registers[position_address + 1u - UHX15A_POLL_START];
    int32_t raw = registers_to_s32(high, low);

    s_axis[axis].status_word = status;
    s_axis[axis].raw_position = raw;
    s_axis[axis].position_deg =
        ((float)raw * UHX15A_POSITION_SCALE) + UHX15A_POSITION_OFFSET;
    s_axis[axis].valid = true;
}

uhx15a_result_t uhx15a_init(void)
{
    mbtcp_cfg_t cfg;
    int rc;

    mbtcp_cfg_default(&cfg);
    cfg.ip = UHX15A_IP;
    cfg.port = UHX15A_PORT;
    cfg.unit = UHX15A_UNIT_ID;
    cfg.timeout_ms = UHX15A_TIMEOUT_MS;
    cfg.retries_read = UHX15A_RETRIES_READ;
    cfg.rx_buf = UHX15A_RX_BUF;
    cfg.tx_buf = UHX15A_TX_BUF;
    cfg.nodelay = UHX15A_NODELAY;

    rc = mbtcp_init(&cfg);
    if (rc != MBTCP_OK) {
        s_initialized = false;
        s_online = false;
        memset(s_axis, 0, sizeof(s_axis));
        return fail(map_error(rc), mbtcp_strerror(rc));
    }

    s_initialized = true;
    s_online = false;
    memset(s_axis, 0, sizeof(s_axis));
    s_last_error = "ok";
    return UHX15A_OK;
}

uhx15a_result_t uhx15a_poll(void)
{
    uint16_t registers[UHX15A_POLL_COUNT];
    int rc;

    if (!s_initialized) {
        return fail(UHX15A_ERR_ARG, "not initialized");
    }

    rc = mbtcp_read_holding(UHX15A_POLL_START, UHX15A_POLL_COUNT, registers);
    if (rc != MBTCP_OK) {
        s_online = false;
        invalidate_axes();
        return fail(map_error(rc), mbtcp_strerror(rc));
    }

    decode_axis(UHX15A_AXIS_AZIMUTH, UHX15A_AZ_STATUS,
                UHX15A_AZ_ACTUAL, registers);
    decode_axis(UHX15A_AXIS_ELEVATION, UHX15A_EL_STATUS,
                UHX15A_EL_ACTUAL, registers);

    s_online = true;
    s_last_error = "ok";
    return UHX15A_OK;
}

uhx15a_result_t uhx15a_set_target(uhx15a_axis_t axis, float position_deg)
{
    uint16_t address;
    uint16_t echo;
    uint16_t registers[2];
    float minimum;
    float maximum;
    int rc;

    if (!s_initialized) {
        return fail(UHX15A_ERR_ARG, "not initialized");
    }

    switch (axis) {
    case UHX15A_AXIS_AZIMUTH:
        address = UHX15A_AZ_TARGET;
        minimum = UHX15A_AZ_MIN_DEG;
        maximum = UHX15A_AZ_MAX_DEG;
        break;
    case UHX15A_AXIS_ELEVATION:
        address = UHX15A_EL_TARGET;
        minimum = UHX15A_EL_MIN_DEG;
        maximum = UHX15A_EL_MAX_DEG;
        break;
    default:
        return fail(UHX15A_ERR_ARG, "invalid axis");
    }

    if (!(position_deg >= minimum && position_deg <= maximum)) {
        return fail(UHX15A_ERR_RANGE, "position out of range");
    }

    s32_to_registers(position_to_raw(position_deg), registers);

    /*
     * FC23 le 1 register e escreve 2 no mesmo endereco. A leitura serve
     * apenas para manter a transacao valida; o valor lido e' descartado.
     */
    rc = mbtcp_read_write_registers(address, 1u, &echo,
                                    address, 2u, registers);
    if (rc != MBTCP_OK) {
        return fail(map_error(rc), mbtcp_strerror(rc));
    }

    s_last_error = "ok";
    return UHX15A_OK;
}

static const uhx15a_axis_state_t *axis_state(uhx15a_axis_t axis)
{
    if (!axis_valid(axis) || !s_axis[axis].valid) {
        return NULL;
    }
    return &s_axis[axis];
}

bool uhx15a_online(void)
{
    return s_online;
}

bool uhx15a_enabled(uhx15a_axis_t axis)
{
    const uhx15a_axis_state_t *state = axis_state(axis);

    return (state != NULL) &&
           ((state->status_word & UHX15A_STATUS_ENABLED) != 0u);
}

bool uhx15a_fieldbus_timeout(uhx15a_axis_t axis)
{
    const uhx15a_axis_state_t *state = axis_state(axis);

    return (state != NULL) &&
           ((state->status_word & UHX15A_STATUS_FIELDBUS_TIMEOUT) != 0u);
}

float uhx15a_position_deg(uhx15a_axis_t axis)
{
    const uhx15a_axis_state_t *state = axis_state(axis);

    return (state != NULL) ? state->position_deg : 0.0f;
}

uint16_t uhx15a_status_word(uhx15a_axis_t axis)
{
    const uhx15a_axis_state_t *state = axis_state(axis);

    return (state != NULL) ? state->status_word : 0u;
}

const char *uhx15a_last_error(void)
{
    return s_last_error;
}

void uhx15a_close(void)
{
    if (s_initialized) {
        mbtcp_disconnect();
    }

    s_initialized = false;
    s_online = false;
    memset(s_axis, 0, sizeof(s_axis));
    s_last_error = "closed";
}
