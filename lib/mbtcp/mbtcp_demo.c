/*
 * mbtcp_demo.c - exemplo de mapa UHX65A + autoteste (NAO compilado pelo
 * mbtcp.mk).
 *
 * Este arquivo e' um exemplo para a aplicacao copiar/adaptar. O layout
 * abaixo assume 2 eixos em modo Positioning, 8 palavras de process data
 * cada:
 *
 *   eixo 1: 1=control, 2=status, 3..4=setpoint S32, 5..6=actual S32,
 *           7..8=reservado
 *   eixo 2: 9=control, 10=status, 11..12=setpoint S32, 13..14=actual S32,
 *           15..16=reservado
 *
 * ATENCAO: o layout real vem do MOVISUITE (modulos MOVIKIT e mapeamento
 * de process data). Ajuste addr/type/scale/word_order na tabela abaixo.
 * O simulador tools/sew_sim.py usa este mesmo layout por default.
 *
 * demo_init() nao cria task e nao faz vTaskDelay. Chame de uma task de
 * aplicacao (nunca do callback do MQTT) ou use mbtcp_selftest().
 */
#include "mbtcp.h"
#include "mbtcp_map.h"
#include "tcpprobe.h"

#include <stdio.h>

/* Ajuste para o IP do PC que roda tools/sew_sim.py, ou para o UHX65A. */
#ifndef DEMO_IP
#define DEMO_IP "10.1.120.15"
#endif
#ifndef DEMO_PORT
#define DEMO_PORT 1502u
#endif
#ifndef DEMO_UNIT
#define DEMO_UNIT 0u
#endif

/* 0 = somente leitura. 1 = habilita escrita de teste no control word. */
#ifndef DEMO_ALLOW_WRITE
#define DEMO_ALLOW_WRITE 0
#endif

enum {
    P_A1_CTRL = 0,
    P_A1_STATUS,
    P_A1_SETPOINT,
    P_A1_ACTUAL,
    P_A2_CTRL,
    P_A2_STATUS,
    P_A2_SETPOINT,
    P_A2_ACTUAL,
    P_COUNT
};

static const mbpoint_t g_points[P_COUNT] = {
    /* nome          addr table       type     access   word      bit grp per  scale off  min      max */
    { "a1.control",   1,  MBT_HOLDING, MBV_U16, MBA_RW, MBW_HI_LO, 0, 1,  20, 1.0f, 0.0f, 0.0f, 65535.0f },
    { "a1.status",    2,  MBT_HOLDING, MBV_U16, MBA_RO, MBW_HI_LO, 0, 1,  20, 1.0f, 0.0f, 0.0f, 0.0f },
    { "a1.setpoint",  3,  MBT_HOLDING, MBV_S32, MBA_RW, MBW_HI_LO, 0, 1,  20, 1.0f, 0.0f, -1000000.0f, 1000000.0f },
    { "a1.actual",    5,  MBT_HOLDING, MBV_S32, MBA_RO, MBW_HI_LO, 0, 1,  20, 1.0f, 0.0f, 0.0f, 0.0f },
    { "a2.control",   9,  MBT_HOLDING, MBV_U16, MBA_RW, MBW_HI_LO, 0, 1,  20, 1.0f, 0.0f, 0.0f, 65535.0f },
    { "a2.status",   10,  MBT_HOLDING, MBV_U16, MBA_RO, MBW_HI_LO, 0, 1,  20, 1.0f, 0.0f, 0.0f, 0.0f },
    { "a2.setpoint", 11,  MBT_HOLDING, MBV_S32, MBA_RW, MBW_HI_LO, 0, 1,  20, 1.0f, 0.0f, -1000000.0f, 1000000.0f },
    { "a2.actual",   13,  MBT_HOLDING, MBV_S32, MBA_RO, MBW_HI_LO, 0, 1,  20, 1.0f, 0.0f, 0.0f, 0.0f },
};

static mbgroup_t g_groups[] = {
    { 1, MBT_HOLDING, 1, 16, 20, 0 },
};

static mbval_t g_cache[P_COUNT];
static mbmap_t g_map;

static int map_setup(void)
{
    return mbmap_init(&g_map, g_points, P_COUNT,
                      g_groups, (uint16_t)(sizeof(g_groups) / sizeof(g_groups[0])),
                      g_cache);
}

int mbtcp_selftest(const char *ip, uint16_t port, uint8_t unit)
{
    mbtcp_cfg_t cfg;
    uint16_t regs[16];
    int rc;

    tcpprobe_env_dump();

    mbtcp_cfg_default(&cfg);
    cfg.ip = (ip != NULL) ? ip : DEMO_IP;
    cfg.port = (port != 0u) ? port : DEMO_PORT;
    cfg.unit = unit;
    cfg.timeout_ms = 800u;
    cfg.retries_read = 1u;
    cfg.retries_write = 0u;
    cfg.nodelay = 1u;

    rc = mbtcp_init(&cfg);
    if (rc != MBTCP_OK) {
        tcpprobe_log("selftest: init: %s", mbtcp_strerror(rc));
        return rc;
    }

    rc = mbtcp_connect();
    if (rc != MBTCP_OK) {
        tcpprobe_log("selftest: connect: %s", mbtcp_strerror(rc));
        return rc;
    }

    rc = mbtcp_read_holding(1u, 16u, regs);
    if (rc == MBTCP_OK) {
        tcpprobe_log("selftest: PD1..16 = %04X %04X %04X %04X %04X %04X %04X %04X "
                     "%04X %04X %04X %04X %04X %04X %04X %04X",
                     regs[0], regs[1], regs[2], regs[3],
                     regs[4], regs[5], regs[6], regs[7],
                     regs[8], regs[9], regs[10], regs[11],
                     regs[12], regs[13], regs[14], regs[15]);
    } else if (rc == MBTCP_EEXCEPTION) {
        tcpprobe_log("selftest: FC3 excecao %d (%s)",
                     mbtcp_last_exception(),
                     mbtcp_exception_str(mbtcp_last_exception()));
    } else {
        tcpprobe_log("selftest: FC3: %s", mbtcp_strerror(rc));
    }

    if (map_setup() == MBTCP_OK) {
        rc = mbmap_poll_group(&g_map, 1u);
        if (rc == MBTCP_OK) {
            mbmap_dump(&g_map);
        } else {
            tcpprobe_log("selftest: poll: %s", mbtcp_strerror(rc));
        }
    }

#if DEMO_ALLOW_WRITE
    rc = mbmap_write(&g_map, P_A1_SETPOINT, 1000.0f);
    tcpprobe_log("selftest: write setpoint: %s", mbtcp_strerror(rc));
#endif

    mbtcp_disconnect();
    return rc;
}

int demo_init(void)
{
    static int done = 0;
    static int last = MBTCP_OK;

    if (done == 0) {
        last = mbtcp_selftest(DEMO_IP, DEMO_PORT, DEMO_UNIT);
        done = 1;
    }
    return last;
}
