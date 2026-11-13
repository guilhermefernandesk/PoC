/*
 * mbtcp.h - cliente Modbus TCP minimo para FreeRTOS-Plus-TCP
 *
 * Um socket, um mutex, sem task permanente. O socket e' criado no
 * primeiro uso e reconectado automaticamente se cair.
 *
 * Depende de tcpprobe.h apenas para logging (tcpprobe_log/_hex).
 *
 * Thread-safety: as funcoes publicas podem ser chamadas de qualquer task;
 * o acesso ao socket e' serializado por um mutex interno.
 */
#ifndef MBTCP_H
#define MBTCP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MBTCP_VERSION "1.0.0"

/* ------------------------------------------------------------------ */
/* Configuracao                                                        */
/* ------------------------------------------------------------------ */

/* Maior frame Modbus TCP (MBAP 6 + unit 1 + PDU 253) */
#ifndef MBTCP_MAX_FRAME
#define MBTCP_MAX_FRAME 260
#endif

/* Timeout padrao de connect/send/recv, em ms */
#ifndef MBTCP_TIMEOUT_MS
#define MBTCP_TIMEOUT_MS 1000u
#endif

/* Tempo maximo esperando o mutex do socket, em ms */
#ifndef MBTCP_MUTEX_TIMEOUT_MS
#define MBTCP_MUTEX_TIMEOUT_MS 3000u
#endif

/* 1 = loga TX/RX em hex via tcpprobe */
#ifndef MBTCP_TRACE
#define MBTCP_TRACE 1
#endif

/* ------------------------------------------------------------------ */
/* Codigos de retorno                                                  */
/* ------------------------------------------------------------------ */
#define MBTCP_OK          0
#define MBTCP_ECONN      -1   /* socket()/connect falhou, ou peer fechou */
#define MBTCP_ESEND      -2   /* FreeRTOS_send falhou                    */
#define MBTCP_ETIMEOUT   -3   /* timeout de recv                         */
#define MBTCP_EFRAME     -4   /* resposta invalida/fora de sincronia     */
#define MBTCP_EEXCEPTION -5   /* slave respondeu excecao Modbus          */
#define MBTCP_EBUSY      -6   /* mutex ocupado                           */
#define MBTCP_EINVAL     -7   /* argumento invalido                      */

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

/*
 * Abre (ou reabre) a conexao. timeout_ms == 0 usa MBTCP_TIMEOUT_MS.
 * Pode ser chamada mais de uma vez; fecha a anterior.
 * Retorna MBTCP_OK ou erro.
 */
int mbtcp_open(const char *ip, uint16_t port, uint32_t timeout_ms);

/* Fecha o socket. Seguro chamar sem ter aberto. */
void mbtcp_close(void);

/*
 * FC03/FC04: le qty registradores a partir de addr para out[].
 * qty: 1..125. out precisa ter qty posicoes.
 */
int mbtcp_read(uint8_t unit, uint8_t fc, uint16_t addr,
               uint16_t qty, uint16_t *out);

/* FC06: escreve um registrador. */
int mbtcp_write(uint8_t unit, uint16_t addr, uint16_t value);

/* FC16: escreve qty registradores (1..123). */
int mbtcp_write_multi(uint8_t unit, uint16_t addr,
                      uint16_t qty, const uint16_t *values);

/* Codigo da ultima excecao Modbus (1..11), 0 se nenhuma. */
int mbtcp_last_exception(void);

/* Texto do erro. */
const char *mbtcp_strerror(int err);

/* ------------------------------------------------------------------ */
/* Demo (opcional; implementada em mbtcp_demo.c, nao compilada pelo   */
/* mbtcp.mk). Chame demo_init() dentro do for(;;) da sua thread:      */
/* a primeira chamada faz setup; as seguintes fazem uma leitura FC03. */
/* ------------------------------------------------------------------ */
int demo_init(void);

#ifdef __cplusplus
}
#endif
#endif /* MBTCP_H */
