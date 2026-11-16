/*
 * test_mbtcp.c - teste host do cliente mbtcp contra tools/sew_sim.py.
 *
 * Roda o transporte e o mapa de verdade (mbtcp.c/mbtcp_map.c), usando
 * os mocks de FreeRTOS em tools/host.
 *
 * Uso:
 *   ./test_mbtcp [porta]
 *
 * A porta deve ser um sew_sim.py --profile uhx65a --unit 0.
 */
#include "mbtcp.h"
#include "mbtcp_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message)                                          \
    do {                                                                   \
        if (!(condition)) {                                                \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", (message), __FILE__,     \
                    __LINE__);                                             \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

enum {
    POINT_AXIS1_CONTROL = 0,
    POINT_AXIS1_STATUS,
    POINT_AXIS1_POSITION_SETPOINT,
    POINT_AXIS1_POSITION_ACTUAL,
    POINT_AXIS2_CONTROL,
    POINT_AXIS2_STATUS,
    POINT_AXIS2_POSITION_SETPOINT,
    POINT_AXIS2_POSITION_ACTUAL,
    POINT_COUNT
};

static const mbpoint_t g_points[POINT_COUNT] = {
    [POINT_AXIS1_CONTROL] = {
        .name = "axis1.control",
        .address = 1u,
        .type = MBV_U16,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
        .minimum = 0.0f,
        .maximum = 65535.0f,
    },
    [POINT_AXIS1_STATUS] = {
        .name = "axis1.status",
        .address = 2u,
        .type = MBV_U16,
        .access = MBA_RO,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
    },
    [POINT_AXIS1_POSITION_SETPOINT] = {
        .name = "axis1.position_setpoint",
        .address = 3u,
        .type = MBV_S32,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
        .minimum = -1000000.0f,
        .maximum = 1000000.0f,
    },
    [POINT_AXIS1_POSITION_ACTUAL] = {
        .name = "axis1.position_actual",
        .address = 5u,
        .type = MBV_S32,
        .access = MBA_RO,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
    },
    [POINT_AXIS2_CONTROL] = {
        .name = "axis2.control",
        .address = 9u,
        .type = MBV_U16,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
        .minimum = 0.0f,
        .maximum = 65535.0f,
    },
    [POINT_AXIS2_STATUS] = {
        .name = "axis2.status",
        .address = 10u,
        .type = MBV_U16,
        .access = MBA_RO,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
    },
    [POINT_AXIS2_POSITION_SETPOINT] = {
        .name = "axis2.position_setpoint",
        .address = 11u,
        .type = MBV_S32,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
        .minimum = -1000000.0f,
        .maximum = 1000000.0f,
    },
    [POINT_AXIS2_POSITION_ACTUAL] = {
        .name = "axis2.position_actual",
        .address = 13u,
        .type = MBV_S32,
        .access = MBA_RO,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
    },
};

static mbgroup_t g_groups[] = {
    {
        .id = 1u,
        .address = 1u,
        .quantity = 16u,
        .period_ms = 20u,
    },
};

static mbval_t g_cache[POINT_COUNT];
static mbmap_t g_map;

static int open_client(int port)
{
    mbtcp_cfg_t cfg;
    int rc;

    mbtcp_cfg_default(&cfg);
    cfg.ip = "127.0.0.1";
    cfg.port = (uint16_t)port;
    cfg.timeout_ms = 800u;
    cfg.retries_read = 1u;

    rc = mbtcp_init(&cfg);
    if (rc != MBTCP_OK) {
        return rc;
    }
    return mbtcp_connect();
}

static void test_transport(void)
{
    uint16_t registers[16];
    uint16_t setpoint[2] = { 0u, 1234u };
    uint16_t one = 1u;
    uint16_t read_one = 0u;
    uint8_t raw_request[5] = {
        MBTCP_FUNCTION_READ_HOLDING_REGISTERS, 0u, 1u, 0u, 1u
    };
    uint8_t raw_response[8];
    uint16_t raw_response_length = 0u;
    int rc;

    rc = mbtcp_write_register(1u, 0u);
    CHECK(rc == MBTCP_OK, "FC6 control=0");

    rc = mbtcp_read_holding(1u, 16u, registers);
    CHECK(rc == MBTCP_OK, "FC3 read 16");
    CHECK(registers[0] == 0u, "control 0");
    CHECK((registers[1] & 0x0001u) == 0u, "status disabled");

    rc = mbtcp_write_register(1u, 1u);
    CHECK(rc == MBTCP_OK, "FC6 control=1");

    rc = mbtcp_read_write_registers(1u, 8u, registers, 3u, 2u, setpoint);
    CHECK(rc == MBTCP_OK, "FC23 read/write");
    CHECK(registers[0] == 1u, "FC23 control echo");
    CHECK((registers[1] & 0x0001u) == 1u, "FC23 status enabled");
    CHECK(registers[2] == 0u && registers[3] == 1234u,
          "FC23 setpoint registers");
    CHECK(registers[4] == 0u && registers[5] == 1234u,
          "FC23 actual follows");

    rc = mbtcp_write_multi(0u, 1u, 1u, &one);
    CHECK(rc == MBTCP_EEXCEPTION, "FC16 rejected");
    CHECK(mbtcp_last_exception() == 1, "FC16 exception 1");

    rc = mbtcp_raw_pdu(raw_request, sizeof(raw_request),
                       raw_response, sizeof(raw_response),
                       &raw_response_length);
    CHECK(rc == MBTCP_OK, "raw PDU FC3");
    CHECK(raw_response_length == 4u, "raw PDU length");
    CHECK(raw_response[0] == MBTCP_FUNCTION_READ_HOLDING_REGISTERS,
          "raw PDU function");
    CHECK(raw_response[1] == 2u, "raw PDU byte count");
    CHECK(raw_response[2] == 0u && raw_response[3] == 1u,
          "raw PDU control value");

    rc = mbtcp_read(0u, MBTCP_FUNCTION_READ_HOLDING_REGISTERS,
                    1u, 1u, &read_one);
    CHECK(rc == MBTCP_OK && read_one == 1u, "legacy mbtcp_read");

    rc = mbtcp_write(0u, 1u, 0u);
    CHECK(rc == MBTCP_OK, "legacy mbtcp_write");
}

static void test_map(void)
{
    mbval_t value;
    int rc;

    rc = mbmap_init(&g_map, g_points, POINT_COUNT, g_groups,
                    (uint16_t)(sizeof(g_groups) / sizeof(g_groups[0])),
                    g_cache);
    CHECK(rc == MBTCP_OK, "map init");

    rc = mbmap_write(&g_map, POINT_AXIS1_CONTROL, 1.0f);
    CHECK(rc == MBTCP_OK, "map write control");

    rc = mbmap_write(&g_map, POINT_AXIS1_POSITION_SETPOINT, 4321.0f);
    CHECK(rc == MBTCP_OK, "map write position setpoint (FC23)");

    rc = mbmap_tick(&g_map);
    CHECK(rc == MBTCP_OK, "map tick");

    rc = mbmap_read_cached(&g_map, POINT_AXIS1_POSITION_SETPOINT, &value);
    CHECK(rc == MBTCP_OK && value.valid, "cache setpoint valid");
    CHECK(value.raw_signed == 4321, "cache setpoint raw");
    CHECK(value.value > 4320.9f && value.value < 4321.1f,
          "cache setpoint value");

    rc = mbmap_read_cached(&g_map, POINT_AXIS1_POSITION_ACTUAL, &value);
    CHECK(rc == MBTCP_OK && value.valid, "cache actual valid");
    CHECK(value.raw_signed == 4321, "cache actual follows");

    rc = mbmap_read_cached(&g_map, POINT_AXIS1_STATUS, &value);
    CHECK(rc == MBTCP_OK && (value.raw_unsigned & 1u) == 1u,
          "cache status enabled");

    rc = mbmap_write(&g_map, POINT_AXIS1_CONTROL, 0.0f);
    CHECK(rc == MBTCP_OK, "map write control=0");

    rc = mbmap_poll_group(&g_map, 1u);
    CHECK(rc == MBTCP_OK, "map poll");

    rc = mbmap_read_cached(&g_map, POINT_AXIS1_POSITION_ACTUAL, &value);
    CHECK(value.raw_signed == 0, "cache actual zero");

    rc = mbmap_write(&g_map, POINT_AXIS1_POSITION_SETPOINT, 2000000.0f);
    CHECK(rc == MBTCP_EINVAL, "map rejects out-of-range value");
}

int main(int argc, char **argv)
{
    int port = (argc > 1) ? atoi(argv[1]) : 1503;
    int rc;

    printf("uhx65a: porta %d\n", port);

    rc = open_client(port);
    CHECK(rc == MBTCP_OK, "connect uhx65a");

    test_transport();
    test_map();

    mbtcp_close();
    rc = mbtcp_open("127.0.0.1", (uint16_t)port, 800u);
    CHECK(rc == MBTCP_OK, "legacy mbtcp_open");
    mbtcp_close();

    puts("test_mbtcp: ok");
    return 0;
}
