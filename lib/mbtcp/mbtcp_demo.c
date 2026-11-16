/*
 * mbtcp_demo.c - exemplo de mapa UHX65A (NAO compilado pelo mbtcp.mk).
 *
 * Copie e adapte para a task de processo da aplicacao. O layout abaixo
 * assume 2 eixos em modo Positioning, 8 palavras de process data cada:
 *
 *   eixo 1: 1=control, 2=status, 3..4=position setpoint, 5..6=actual
 *   eixo 2: 9=control, 10=status, 11..12=position setpoint, 13..14=actual
 *
 * ATENCAO: o layout real vem do MOVISUITE (modulos MOVIKIT e mapeamento
 * de process data). Ajuste address/type/scale/word_order na tabela.
 *
 * Este exemplo nunca deve ser chamado do callback do MQTT.
 */
#include "mbtcp.h"
#include "mbtcp_map.h"
#include "tcpprobe.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef DEMO_IP
#define DEMO_IP "10.1.120.15"
#endif
#ifndef DEMO_PORT
#define DEMO_PORT 1502u
#endif
#ifndef DEMO_UNIT
#define DEMO_UNIT 0u
#endif
/* 0 = somente leitura. 1 = habilita escrita de teste de posicao. */
#ifndef DEMO_ALLOW_WRITE
#define DEMO_ALLOW_WRITE 0
#endif

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
        .offset = 0.0f,
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
        .offset = 0.0f,
    },
    [POINT_AXIS1_POSITION_SETPOINT] = {
        .name = "axis1.position_setpoint",
        .address = 3u,
        .type = MBV_S32,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
        .offset = 0.0f,
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
        .offset = 0.0f,
    },
    [POINT_AXIS2_CONTROL] = {
        .name = "axis2.control",
        .address = 9u,
        .type = MBV_U16,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
        .offset = 0.0f,
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
        .offset = 0.0f,
    },
    [POINT_AXIS2_POSITION_SETPOINT] = {
        .name = "axis2.position_setpoint",
        .address = 11u,
        .type = MBV_S32,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1u,
        .scale = 1.0f,
        .offset = 0.0f,
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
        .offset = 0.0f,
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

/* Task de processo: chame de uma task existente, nunca do callback MQTT. */
void mbtcp_example_task(void *argument)
{
    mbtcp_cfg_t cfg;
    int rc;

    (void)argument;

    mbtcp_cfg_default(&cfg);
    cfg.ip = DEMO_IP;
    cfg.port = DEMO_PORT;
    cfg.unit = DEMO_UNIT;
    cfg.timeout_ms = 800u;
    cfg.retries_read = 1u;

    if (mbtcp_init(&cfg) != MBTCP_OK) {
        return;
    }
    if (mbmap_init(&g_map, g_points, POINT_COUNT, g_groups,
                   (uint16_t)(sizeof(g_groups) / sizeof(g_groups[0])),
                   g_cache) != MBTCP_OK) {
        return;
    }

    for (;;) {
        rc = mbmap_tick(&g_map);
        if (rc != MBTCP_OK) {
            tcpprobe_log("modbus: %s", mbtcp_strerror(rc));
        }

        /* Exemplo: so escreva depois de confirmar o mapeamento real. */
#if DEMO_ALLOW_WRITE
        rc = mbmap_write(&g_map, POINT_AXIS1_POSITION_SETPOINT, 1000.0f);
        if (rc != MBTCP_OK) {
            tcpprobe_log("position write: %s", mbtcp_strerror(rc));
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(10u));
    }
}
