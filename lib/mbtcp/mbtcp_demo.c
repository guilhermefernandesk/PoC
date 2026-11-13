/*
 * mbtcp_demo.c - cola para uma thread que ja existe
 *
 * Nao cria task, nao tem loop e nao tem delay. Chame demo_init() dentro
 * do for(;;) da sua thread:
 *
 *     for (;;) {
 *         demo_init();          // 1a vez: setup; depois: 1 leitura
 *         vTaskDelay(pdMS_TO_TICKS(1000));
 *     }
 *
 * A primeira chamada imprime o ambiente e abre o socket. As seguintes
 * fazem uma leitura FC03 e logam o resultado.
 *
 * Para usar: copie esta funcao para o .c da sua thread OU adicione este
 * arquivo ao C_SOURCES do projeto (o mbtcp.mk nao o compila).
 */
#include "mbtcp.h"
#include "tcpprobe.h"

/* ---- ajuste para a bancada ---- */
#ifndef DEMO_IP
#define DEMO_IP "192.168.1.100"
#endif
#ifndef DEMO_PORT
#define DEMO_PORT 1502u
#endif
#ifndef DEMO_UNIT
#define DEMO_UNIT 1u
#endif
#ifndef DEMO_ADDR
#define DEMO_ADDR 0u
#endif
#ifndef DEMO_QTY
#define DEMO_QTY 4u
#endif

#if (DEMO_QTY < 1u) || (DEMO_QTY > 125u)
#error "DEMO_QTY deve ficar entre 1 e 125"
#endif

static int s_ready = 0;

int demo_init(void)
{
    uint16_t v[DEMO_QTY];
    unsigned i;
    int rc;

    if (!s_ready) {
        tcpprobe_env_dump();

        rc = mbtcp_open(DEMO_IP, DEMO_PORT, 1000u);
        tcpprobe_log("demo: open %s:%u -> %d (%s)",
                     DEMO_IP, (unsigned)DEMO_PORT,
                     rc, mbtcp_strerror(rc));

        s_ready = 1;   /* mesmo se falhar: mbtcp_read reconecta */
    }

    rc = mbtcp_read((uint8_t)DEMO_UNIT, 3u, (uint16_t)DEMO_ADDR,
                    (uint16_t)DEMO_QTY, v);

    if (rc == MBTCP_OK) {
        for (i = 0u; i < DEMO_QTY; i++) {
            tcpprobe_log("demo: reg[%u] = %04X (%u)",
                         (unsigned)(DEMO_ADDR + i),
                         (unsigned)v[i], (unsigned)v[i]);
        }
    } else {
        tcpprobe_log("demo: read -> %d (%s) exc=%d",
                     rc, mbtcp_strerror(rc), mbtcp_last_exception());
    }

    return rc;
}
