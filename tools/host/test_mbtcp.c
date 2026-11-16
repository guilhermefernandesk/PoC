/*
 * test_mbtcp.c - teste host do cliente mbtcp contra tools/sew_sim.py.
 *
 * Roda o transporte e o mapa de verdade (mbtcp.c/mbtcp_map.c), usando
 * os mocks de FreeRTOS em tools/host.
 *
 * Uso:
 *   ./test_mbtcp 1503 [1504]
 *
 * O primeiro porto deve ser um sew_sim --profile uhx65a --unit 0.
 * O segundo (opcional) deve ser --profile generic --unit 1.
 */
#include "mbtcp.h"
#include "mbtcp_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__,         \
                    __LINE__);                                             \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

enum {
    P1_CTRL = 0,
    P1_STATUS,
    P1_SETPOINT,
    P1_ACTUAL,
    P2_CTRL,
    P2_STATUS,
    P2_SETPOINT,
    P2_ACTUAL,
    P_COUNT
};

static const mbpoint_t g_points[P_COUNT] = {
    { "a1.control",   1,  MBT_HOLDING, MBV_U16, MBA_RW, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 65535.0f },
    { "a1.status",    2,  MBT_HOLDING, MBV_U16, MBA_RO, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 0.0f },
    { "a1.setpoint",  3,  MBT_HOLDING, MBV_S32, MBA_RW, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, -1000000.0f, 1000000.0f },
    { "a1.actual",    5,  MBT_HOLDING, MBV_S32, MBA_RO, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 0.0f },
    { "a2.control",   9,  MBT_HOLDING, MBV_U16, MBA_RW, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 65535.0f },
    { "a2.status",   10,  MBT_HOLDING, MBV_U16, MBA_RO, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 0.0f },
    { "a2.setpoint", 11,  MBT_HOLDING, MBV_S32, MBA_RW, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, -1000000.0f, 1000000.0f },
    { "a2.actual",   13,  MBT_HOLDING, MBV_S32, MBA_RO, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 0.0f },
};

static mbgroup_t g_groups[] = {
    { 1, MBT_HOLDING, 1, 16, 20, 0 },
};

static mbval_t g_cache[P_COUNT];
static mbmap_t g_map;

static char g_vendor[64];
static char g_product[64];

static void devinfo_cb(uint8_t obj_id, const uint8_t *data,
                       uint8_t len, void *ctx)
{
    char *dst = NULL;

    (void)ctx;
    if (obj_id == 0x00u) {
        dst = g_vendor;
    } else if (obj_id == 0x01u) {
        dst = g_product;
    }

    if (dst != NULL) {
        size_t n = (len < 63u) ? len : 63u;
        memcpy(dst, data, n);
        dst[n] = '\0';
    }
}

static int mb_open(int port, uint8_t unit)
{
    mbtcp_cfg_t cfg;
    int rc;

    mbtcp_cfg_default(&cfg);
    cfg.ip = "127.0.0.1";
    cfg.port = (uint16_t)port;
    cfg.unit = unit;
    cfg.timeout_ms = 800u;
    cfg.retries_read = 1u;
    cfg.retries_write = 0u;
    cfg.nodelay = 1u;

    rc = mbtcp_init(&cfg);
    if (rc != MBTCP_OK) {
        return rc;
    }
    return mbtcp_connect();
}

static void test_uhx65a(int port)
{
    uint16_t regs[16];
    uint16_t setp[2] = { 0u, 1234u };
    uint16_t one = 1u;
    mbval_t v;
    int rc;

    printf("uhx65a: porta %d\n", port);
    rc = mb_open(port, 0u);
    CHECK(rc == MBTCP_OK, "connect uhx65a");

    rc = mbtcp_write_register(1u, 0u);
    CHECK(rc == MBTCP_OK, "FC6 control=0");

    rc = mbtcp_read_holding(1u, 16u, regs);
    CHECK(rc == MBTCP_OK, "FC3 read 16");
    CHECK(regs[0] == 0u, "control 0");
    CHECK((regs[1] & 0x0001u) == 0u, "status disabled");

    rc = mbtcp_write_register(1u, 1u);
    CHECK(rc == MBTCP_OK, "FC6 control=1");

    rc = mbtcp_read_write_registers(1u, 8u, regs, 3u, 2u, setp);
    CHECK(rc == MBTCP_OK, "FC23 read/write");
    CHECK(regs[0] == 1u, "FC23 control echo");
    CHECK((regs[1] & 0x0001u) == 1u, "FC23 status enabled");
    CHECK(regs[3] == 1234u, "FC23 setpoint low");
    CHECK(((regs[4] << 16) | regs[5]) == 1234u, "FC23 actual follows");

    rc = mbtcp_write_registers(1u, 1u, &one);
    CHECK(rc == MBTCP_EEXCEPTION, "FC16 rejected");
    CHECK(mbtcp_last_exception() == 1, "FC16 exception 1");

    rc = mbtcp_read_device_id(1u, NULL, NULL);
    CHECK(rc == MBTCP_EEXCEPTION, "FC43 rejected");
    CHECK(mbtcp_last_exception() == 1, "FC43 exception 1");

    rc = mbmap_init(&g_map, g_points, P_COUNT, g_groups, 1u, g_cache);
    CHECK(rc == MBTCP_OK, "map init");

    rc = mbmap_write(&g_map, P1_CTRL, 1.0f);
    CHECK(rc == MBTCP_OK, "map write control");
    rc = mbmap_write(&g_map, P1_SETPOINT, 4321.0f);
    CHECK(rc == MBTCP_OK, "map write setpoint (FC23)");
    rc = mbmap_tick(&g_map);
    CHECK(rc == MBTCP_OK, "map tick");

    rc = mbmap_read_cached(&g_map, P1_SETPOINT, &v);
    CHECK(rc == MBTCP_OK && v.valid, "cache setpoint valid");
    CHECK(v.raw_i == 4321, "cache setpoint raw");
    rc = mbmap_read_cached(&g_map, P1_ACTUAL, &v);
    CHECK(rc == MBTCP_OK && v.valid, "cache actual valid");
    CHECK(v.raw_i == 4321, "cache actual follows");
    rc = mbmap_read_cached(&g_map, P1_STATUS, &v);
    CHECK(rc == MBTCP_OK && ((v.raw_u & 1u) == 1u), "cache status enabled");

    rc = mbmap_write(&g_map, P1_CTRL, 0.0f);
    CHECK(rc == MBTCP_OK, "map write control=0");
    rc = mbmap_poll_group(&g_map, 1u);
    CHECK(rc == MBTCP_OK, "map poll 2");
    rc = mbmap_read_cached(&g_map, P1_ACTUAL, &v);
    CHECK(v.raw_i == 0, "cache actual zero");

    mbtcp_disconnect();
}

static void test_generic(int port)
{
    uint8_t bits[16];
    uint8_t wbits[4] = { 1u, 0u, 1u, 1u };
    uint16_t wr[3] = { 1u, 2u, 3u };
    uint16_t rd[4] = { 0u, 0u, 0u, 0u };
    uint16_t ir[2] = { 0u, 0u };
    uint16_t lw[2] = { 0x00AAu, 0x00BBu };
    uint16_t w2[2] = { 7u, 8u };
    uint16_t r2[2] = { 0u, 0u };
    uint16_t fifo[8];
    uint16_t count = 0u;
    uint16_t fr[4] = { 0u, 0u, 0u, 0u };
    uint8_t raw_pdu[5] = { 3u, 0u, 20u, 0u, 1u };
    uint8_t raw_rsp[8];
    uint16_t raw_len = 0u;
    int rc;

    printf("generic: porta %d\n", port);
    rc = mb_open(port, 1u);
    CHECK(rc == MBTCP_OK, "connect generic");

    rc = mbtcp_read_coils(0u, 16u, bits);
    CHECK(rc == MBTCP_OK, "FC1");
    CHECK(bits[0] == 0u && bits[1] == 1u && bits[2] == 0u && bits[3] == 1u,
          "FC1 padrao");

    rc = mbtcp_read_discrete_inputs(0u, 16u, bits);
    CHECK(rc == MBTCP_OK, "FC2");
    CHECK(bits[0] == 0u && bits[1] == 0u && bits[2] == 1u && bits[3] == 1u,
          "FC2 padrao");

    rc = mbtcp_read_input(0u, 2u, ir);
    CHECK(rc == MBTCP_OK && ir[0] == 0u && ir[1] == 3u, "FC4 padrao");

    rc = mbtcp_write_coil(10u, 1);
    CHECK(rc == MBTCP_OK, "FC5");
    rc = mbtcp_read_coils(10u, 1u, bits);
    CHECK(rc == MBTCP_OK && bits[0] == 1u, "FC5 readback");

    rc = mbtcp_write_coils(10u, 4u, wbits);
    CHECK(rc == MBTCP_OK, "FC15");
    rc = mbtcp_read_coils(10u, 4u, bits);
    CHECK(rc == MBTCP_OK && bits[0] == 1u && bits[1] == 0u &&
          bits[2] == 1u && bits[3] == 1u, "FC15 readback");

    rc = mbtcp_write_registers(20u, 3u, wr);
    CHECK(rc == MBTCP_OK, "FC16");
    rc = mbtcp_read_holding(20u, 2u, rd);
    CHECK(rc == MBTCP_OK && rd[0] == 1u && rd[1] == 2u, "FC16 readback");

    rc = mbtcp_read(1u, 3u, 20u, 1u, rd);
    CHECK(rc == MBTCP_OK && rd[0] == 1u, "legacy mbtcp_read");
    rc = mbtcp_write(1u, 31u, 0x55AAu);
    CHECK(rc == MBTCP_OK, "legacy mbtcp_write");
    rc = mbtcp_read_holding(31u, 1u, rd);
    CHECK(rc == MBTCP_OK && rd[0] == 0x55AAu, "legacy mbtcp_write readback");
    rc = mbtcp_write_multi(1u, 32u, 2u, lw);
    CHECK(rc == MBTCP_OK, "legacy mbtcp_write_multi");
    rc = mbtcp_read_holding(32u, 2u, rd);
    CHECK(rc == MBTCP_OK && rd[0] == lw[0] && rd[1] == lw[1],
          "legacy mbtcp_write_multi readback");

    rc = mbtcp_write_register(20u, 0x1234u);
    CHECK(rc == MBTCP_OK, "FC6 0x1234");
    rc = mbtcp_mask_write_register(20u, 0xFF00u, 0x0001u);
    CHECK(rc == MBTCP_OK, "FC22");
    rc = mbtcp_read_holding(20u, 1u, rd);
    CHECK(rc == MBTCP_OK && rd[0] == 0x1201u, "FC22 result");

    rc = mbtcp_read_write_registers(20u, 2u, r2, 20u, 2u, w2);
    CHECK(rc == MBTCP_OK && r2[0] == 7u && r2[1] == 8u, "FC23");

    rc = mbtcp_raw_pdu(raw_pdu, sizeof(raw_pdu),
                       raw_rsp, sizeof(raw_rsp), &raw_len);
    CHECK(rc == MBTCP_OK && raw_len == 4u && raw_rsp[0] == 3u &&
          raw_rsp[1] == 2u && raw_rsp[2] == 0u && raw_rsp[3] == 7u,
          "raw PDU FC3");

    rc = mbtcp_read_fifo(0x3000u, fifo, 8u, &count);
    CHECK(rc == MBTCP_OK && count == 3u, "FC24 count");
    CHECK(fifo[0] == 0x1111u && fifo[1] == 0x2222u && fifo[2] == 0x3333u,
          "FC24 values");

    rc = mbtcp_write_file_record(1u, 0u, 3u, wr);
    CHECK(rc == MBTCP_OK, "FC21");
    rc = mbtcp_read_file_record(1u, 0u, 4u, fr);
    CHECK(rc == MBTCP_OK, "FC20");
    CHECK(fr[0] == 1u && fr[1] == 2u && fr[2] == 3u && fr[3] == 0u,
          "FC20 values");

    memset(g_vendor, 0, sizeof(g_vendor));
    memset(g_product, 0, sizeof(g_product));
    rc = mbtcp_read_device_id(2u, devinfo_cb, NULL);
    CHECK(rc == MBTCP_OK, "FC43");
    CHECK(strcmp(g_vendor, "SEW-EURODRIVE") == 0, "FC43 vendor");
    CHECK(strcmp(g_product, "UHX65A-SIM") == 0, "FC43 product");

    memset(g_vendor, 0, sizeof(g_vendor));
    memset(g_product, 0, sizeof(g_product));
    rc = mbtcp_read_device_id_obj(4u, 1u, devinfo_cb, NULL);
    CHECK(rc == MBTCP_OK, "FC43/14 objeto individual");
    CHECK(g_vendor[0] == '\0', "FC43/14 nao le vendor");
    CHECK(strcmp(g_product, "UHX65A-SIM") == 0, "FC43/14 product");

    mbtcp_disconnect();
}

int main(int argc, char **argv)
{
    int uhx_port = (argc > 1) ? atoi(argv[1]) : 1503;
    int generic_port = (argc > 2) ? atoi(argv[2]) : 0;

    test_uhx65a(uhx_port);
    if (generic_port != 0) {
        test_generic(generic_port);
    }

    puts("test_mbtcp: ok");
    return 0;
}
