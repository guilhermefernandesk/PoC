/*
 * example.c - S0: prova TCP cru (nao compilado pelo tcpprobe.mk)
 *
 * Copie/adapte para o seu projeto apenas durante a validacao.
 * Cria uma task one-shot que espera 5 s, imprime o ambiente, conecta
 * em ip:port, envia "hello" e imprime o que voltar. Depois se deleta.
 *
 * Chamada (uma vez, depois da rede subir):
 *     tcpprobe_example_start("192.168.1.100", 1502);
 */
#include "tcpprobe.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef TCPPROBE_EXAMPLE_STACK_WORDS
#define TCPPROBE_EXAMPLE_STACK_WORDS 384
#endif

#ifndef TCPPROBE_EXAMPLE_PRIO
#define TCPPROBE_EXAMPLE_PRIO (tskIDLE_PRIORITY + 2)
#endif

typedef struct {
    const char *ip;
    uint16_t port;
} example_ctx_t;

static example_ctx_t s_ctx;

static void example_task(void *arg)
{
    example_ctx_t *c = (example_ctx_t *)arg;
    uint8_t rx[64];
    int n;

    vTaskDelay(pdMS_TO_TICKS(5000));

    tcpprobe_env_dump();

    n = tcpprobe_tcp_client(c->ip, c->port, "hello", 5,
                            rx, (uint16_t)sizeof(rx), 2000u);

    if (n > 0) {
        tcpprobe_log("S0 OK: %d bytes recebidos", n);
    } else {
        tcpprobe_log("S0 FALHOU: %d (%s)", n,
                     (n == TCPPROBE_ECONN)    ? "connect" :
                     (n == TCPPROBE_ESEND)    ? "send"    :
                     (n == TCPPROBE_ETIMEOUT) ? "timeout" :
                     (n == TCPPROBE_ERECV)    ? "peer fechou" : "arg");
    }

    vTaskDelete(NULL);
}

void tcpprobe_example_start(const char *ip, uint16_t port)
{
    s_ctx.ip = ip;
    s_ctx.port = port;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    {
        static StackType_t stk[TCPPROBE_EXAMPLE_STACK_WORDS];
        static StaticTask_t tcb;

        (void)xTaskCreateStatic(example_task, "tcp_s0",
                                TCPPROBE_EXAMPLE_STACK_WORDS,
                                &s_ctx, TCPPROBE_EXAMPLE_PRIO,
                                stk, &tcb);
    }
#else
    (void)xTaskCreate(example_task, "tcp_s0",
                      TCPPROBE_EXAMPLE_STACK_WORDS,
                      &s_ctx, TCPPROBE_EXAMPLE_PRIO, NULL);
#endif
}
