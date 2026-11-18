/*
 * test_uhx15a.c - teste host do driver UHX15A contra tools/sew_sim.py.
 *
 * Roda o transporte e o driver de verdade, usando os mocks de FreeRTOS em
 * tools/host. O IP/porta vem de uhx15a_config.h; o Makefile sobrescreve
 * com -DUHX15A_IP/-DUHX15A_PORT para apontar para o simulador local.
 *
 * Uso:
 *   make -C tools/host
 *   ./tools/host/test_uhx15a
 *
 * O simulador deve estar rodando com:
 *   python3 tools/sew_sim.py --port 1503 --profile uhx15a --unit 0 \
 *       --fieldbus-timeout 0
 */
#include "uhx15a.h"
#include "uhx15a_config.h"

#include "mbtcp.h"

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

static bool close_enough(float actual, float expected)
{
    float difference = actual - expected;

    return (difference > -0.01f) && (difference < 0.01f);
}

static void test_read_and_targets(void)
{
    uhx15a_result_t rc;
    uint16_t enable = 1u;

    rc = uhx15a_init();
    CHECK(rc == UHX15A_OK, "init");
    CHECK(!uhx15a_online(), "offline before first poll");

    /*
     * Normaliza o simulador: o teste ASan roda depois do teste normal e
     * nao pode depender do ultimo setpoint deixado pelo processo anterior.
     */
    CHECK(mbtcp_fc(MBTCP_FUNCTION_WRITE_SINGLE_REGISTER,
                   UHX15A_AZ_CONTROL, 1u, &enable) == MBTCP_OK,
          "enable azimuth control");
    CHECK(mbtcp_fc(MBTCP_FUNCTION_WRITE_SINGLE_REGISTER,
                   UHX15A_EL_CONTROL, 1u, &enable) == MBTCP_OK,
          "enable elevation control");
    CHECK(uhx15a_set_target(UHX15A_AXIS_AZIMUTH, 0.0f) == UHX15A_OK,
          "reset azimuth target");
    CHECK(uhx15a_set_target(UHX15A_AXIS_ELEVATION, 0.0f) == UHX15A_OK,
          "reset elevation target");

    rc = uhx15a_poll();
    CHECK(rc == UHX15A_OK, "first poll");
    CHECK(uhx15a_online(), "online after poll");
    CHECK(mbtcp_is_connected(), "transport connected");
    CHECK(uhx15a_enabled(UHX15A_AXIS_AZIMUTH), "azimuth enabled");
    CHECK(uhx15a_enabled(UHX15A_AXIS_ELEVATION), "elevation enabled");
    CHECK(close_enough(uhx15a_position_deg(UHX15A_AXIS_AZIMUTH), 0.0f),
          "azimuth initial position");
    CHECK(close_enough(uhx15a_position_deg(UHX15A_AXIS_ELEVATION), 0.0f),
          "elevation initial position");
    CHECK((uhx15a_status_word(UHX15A_AXIS_AZIMUTH) &
           UHX15A_STATUS_ENABLED) != 0u, "azimuth status enabled bit");
    CHECK(!uhx15a_fieldbus_timeout(UHX15A_AXIS_AZIMUTH),
          "azimuth fieldbus timeout");

    rc = uhx15a_set_target(UHX15A_AXIS_AZIMUTH, 45.0f);
    CHECK(rc == UHX15A_OK, "set azimuth target");
    rc = uhx15a_poll();
    CHECK(rc == UHX15A_OK, "poll after azimuth target");
    CHECK(close_enough(uhx15a_position_deg(UHX15A_AXIS_AZIMUTH), 45.0f),
          "azimuth target mirrored");

    rc = uhx15a_set_target(UHX15A_AXIS_AZIMUTH, -30.0f);
    CHECK(rc == UHX15A_OK, "set negative azimuth target");
    rc = uhx15a_poll();
    CHECK(rc == UHX15A_OK, "poll after negative azimuth target");
    CHECK(close_enough(uhx15a_position_deg(UHX15A_AXIS_AZIMUTH), -30.0f),
          "negative azimuth target mirrored");

    rc = uhx15a_set_target(UHX15A_AXIS_ELEVATION, 10.0f);
    CHECK(rc == UHX15A_OK, "set elevation target");
    rc = uhx15a_poll();
    CHECK(rc == UHX15A_OK, "poll after elevation target");
    CHECK(close_enough(uhx15a_position_deg(UHX15A_AXIS_ELEVATION), 10.0f),
          "elevation target mirrored");
    CHECK(close_enough(uhx15a_position_deg(UHX15A_AXIS_AZIMUTH), -30.0f),
          "azimuth unchanged by elevation target");
}

static void test_limits_and_invalid_axis(void)
{
    uhx15a_result_t rc;

    rc = uhx15a_set_target(UHX15A_AXIS_AZIMUTH, 999.0f);
    CHECK(rc == UHX15A_ERR_RANGE, "azimuth range rejected");
    CHECK(strcmp(uhx15a_last_error(), "position out of range") == 0,
          "range error text");

    rc = uhx15a_set_target((uhx15a_axis_t)99, 0.0f);
    CHECK(rc == UHX15A_ERR_ARG, "invalid axis rejected");
    CHECK(strcmp(uhx15a_last_error(), "invalid axis") == 0,
          "invalid axis error text");

    CHECK(!uhx15a_enabled((uhx15a_axis_t)99), "invalid axis not enabled");
    CHECK(uhx15a_position_deg((uhx15a_axis_t)99) == 0.0f,
          "invalid axis position");
    CHECK(uhx15a_status_word((uhx15a_axis_t)99) == 0u,
          "invalid axis status");
}

static void test_generic_fc_and_enable(void)
{
    uint16_t value;
    uhx15a_result_t rc;

    value = 0u;
    CHECK(mbtcp_fc(MBTCP_FUNCTION_READ_HOLDING_REGISTERS,
                   UHX15A_AZ_STATUS, 1u, &value) == MBTCP_OK,
          "fc03 status");
    CHECK((value & UHX15A_STATUS_ENABLED) != 0u, "fc03 status enabled");

    value = 0u;
    CHECK(mbtcp_fc(MBTCP_FUNCTION_WRITE_SINGLE_REGISTER,
                   UHX15A_AZ_CONTROL, 1u, &value) == MBTCP_OK,
          "fc06 disable control");
    rc = uhx15a_poll();
    CHECK(rc == UHX15A_OK, "poll after disable");
    CHECK(!uhx15a_enabled(UHX15A_AXIS_AZIMUTH), "azimuth disabled");
    CHECK(close_enough(uhx15a_position_deg(UHX15A_AXIS_AZIMUTH), 0.0f),
          "azimuth actual zero when disabled");

    value = 1u;
    CHECK(mbtcp_fc(MBTCP_FUNCTION_WRITE_SINGLE_REGISTER,
                   UHX15A_AZ_CONTROL, 1u, &value) == MBTCP_OK,
          "fc06 enable control");
    rc = uhx15a_poll();
    CHECK(rc == UHX15A_OK, "poll after enable");
    CHECK(uhx15a_enabled(UHX15A_AXIS_AZIMUTH), "azimuth enabled again");
    CHECK(close_enough(uhx15a_position_deg(UHX15A_AXIS_AZIMUTH), -30.0f),
          "azimuth resumes last target");

    value = 0u;
    CHECK(mbtcp_fc(0x10u, 1u, 1u, &value) == MBTCP_EINVAL,
          "fc16 rejected by generic fc");
}

static void test_read_only_registers(void)
{
    uint16_t value = 5u;
    uint16_t readback = 0u;

    CHECK(mbtcp_fc(MBTCP_FUNCTION_WRITE_SINGLE_REGISTER,
                   UHX15A_AZ_STATUS, 1u, &value) == MBTCP_EEXCEPTION,
          "fc06 status rejected");
    CHECK(mbtcp_last_exception() == 2, "fc06 status exception 2");

    CHECK(mbtcp_read_write_registers(UHX15A_AZ_STATUS, 1u, &readback,
                                     UHX15A_AZ_STATUS, 1u, &value) ==
          MBTCP_EEXCEPTION, "fc23 status rejected");
    CHECK(mbtcp_last_exception() == 2, "fc23 status exception 2");
}

static void test_close_and_reinit(void)
{
    uhx15a_result_t rc;

    uhx15a_close();
    CHECK(!uhx15a_online(), "offline after close");
    CHECK(!mbtcp_is_connected(), "transport disconnected after close");
    CHECK(uhx15a_poll() == UHX15A_ERR_ARG, "poll before init");
    CHECK(uhx15a_set_target(UHX15A_AXIS_AZIMUTH, 0.0f) == UHX15A_ERR_ARG,
          "set target before init");

    rc = uhx15a_init();
    CHECK(rc == UHX15A_OK, "reinit");
    rc = uhx15a_poll();
    CHECK(rc == UHX15A_OK, "poll after reinit");
    CHECK(uhx15a_online(), "online after reinit");
}

int main(void)
{
    test_read_and_targets();
    test_limits_and_invalid_axis();
    test_generic_fc_and_enable();
    test_read_only_registers();
    test_close_and_reinit();

    puts("test_uhx15a: ok");
    return 0;
}
