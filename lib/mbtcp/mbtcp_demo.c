/*
 * mbtcp_demo.c - S3: le 4 registradores em loop (nao compilado pelo .mk)
 *
 * Serve para validar contra o simulador no PC. Cria UMA task temporaria
 * (ou permanente, se MBTCP_DEMO_ITERATIONS == 0). Remova a chamada quando
 * integrar no handler real.
 *
 * Chamada (uma vez, depois da rede subir):
 *     mbtcp_demo_start("192.168.1.100", 1502, 1);
 *
 * Compile junto adicionando ao C_SOURCES do projeto, ou copie o corpo
 * para a task que ja existe.
 */
#include "mbtcp.h"
#include "tcpprobe.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#ifndef MBTCP_DEMO_STACK_WORDS
#define MBTCP_DEMO_STACK_WORDS 512
#endif

#ifndef MBTCP_DEMO_PRIO
#define MBTCP_DEMO_PRIO (tskIDLE_PRIORITY + 2)
#endif

/* 0 = infinito; N > 0 = para depois de N leituras e se deleta */
#ifndef MBTCP_DEMO_ITERATIONS
#define MBTCP_DEMO_ITERATIONS 0
#endif

#ifndef MBTCP_DEMO_PERIOD_MS
#define MBTCP_DEMO_PERIOD_MS 5000u
#endif

typedef struct {
    char ip[16];
    uint16_t port;
    uint8_t unit;
} demo_ctx_t;

static demo_ctx_t s_ctx;

static void demo_task(void *arg)
{
    demo_ctx_t *c = (demo_ctx_t *)arg;
    uint16_t v[4];
    unsigned iter = 0u;
    int rc;

    vTaskDelay(pdMS_TO_TICKS(5000));

    tcpprobe_env_dump();

    rc = mbtcp_open(c->ip, c->port, MBTCP_TIMEOUT_MS);
    tcpprobe_log("mbtcp: open %s:%u unit=%u -> %d (%s)",
                 c->ip, (unsigned)c->port, (unsigned)c->unit,
                 rc, mbtcp_strerror(rc));

    for (;;) {
        rc = mbtcp_read(c->unit, 3u, 0u, 4u, v);

        if (rc == MBTCP_OK) {
            tcpprobe_log("mbtcp: regs[0..3] = %04X %04X %04X %04X",
                         (unsigned)v[0], (unsigned)v[1],
                         (unsigned)v[2], (unsigned)v[3]);
        } else {
            tcpprobe_log("mbtcp: read -> %d (%s) exc=%d",
                         rc, mbtcp_strerror(rc), mbtcp_last_exception());
        }

        iter++;

#if (MBTCP_DEMO_ITERATIONS > 0)
        if (iter >= (unsigned)MBTCP_DEMO_ITERATIONS) {
            tcpprobe_log("mbtcp: demo concluido (%u iteracoes)", iter);
            mbtcp_close();
            vTaskDelete(NULL);
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(MBTCP_DEMO_PERIOD_MS));
    }
}

void mbtcp_demo_start(const char *ip, uint16_t port, uint8_t unit)
{
    if (ip == NULL) {
        return;
    }

    strncpy(s_ctx.ip, ip, sizeof(s_ctx.ip) - 1u);
    s_ctx.ip[sizeof(s_ctx.ip) - 1u] = '\0';
    s_ctx.port = port;
    s_ctx.unit = unit;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    {
        static StackType_t stk[MBTCP_DEMO_STACK_WORDS];
        static StaticTask_t tcb;

        (void)xTaskCreateStatic(demo_task, "mbtcp_demo",
                                MBTCP_DEMO_STACK_WORDS,
                                &s_ctx, MBTCP_DEMO_PRIO,
                                stk, &tcb);
    }
#else
    (void)xTaskCreate(demo_task, "mbtcp_demo",
                      MBTCP_DEMO_STACK_WORDS,
                      &s_ctx, MBTCP_DEMO_PRIO, NULL);
#endif
}
