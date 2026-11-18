/*
 * mbtcp.h - cliente Modbus TCP para FreeRTOS-Plus-TCP
 *
 * Escopo deste cliente:
 *   - um socket TCP, um mutex, sem task permanente;
 *   - FC03 (ler holding registers);
 *   - FC06 (escrever um register);
 *   - FC23 (ler e escrever registers na mesma transacao);
 *   - mbtcp_fc() para leitura/escrita pontual por function code;
 *   - PDU cru para qualquer outra function code.
 *
 * Perfil alvo: SEW UHX15A (unit 0, offset 0, FC03/FC06/FC23).
 *
 * Thread-safety: as funcoes publicas podem ser chamadas de qualquer task;
 * o acesso ao socket e' serializado por um mutex interno.
 */
#ifndef MBTCP_H
#define MBTCP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MBTCP_VERSION "4.0.0"

/* ------------------------------------------------------------------ */
/* Configuracao                                                        */
/* ------------------------------------------------------------------ */

/* Maior frame Modbus TCP: MBAP (7) + PDU (253). */
#define MBTCP_MAX_FRAME 260u

/* Maior PDU: function code + dados. */
#define MBTCP_MAX_PDU 253u

/* Valores usados quando a configuracao deixa o campo em zero. */
#define MBTCP_DEFAULT_PORT 502u
#define MBTCP_DEFAULT_UNIT 0u
#define MBTCP_DEFAULT_TIMEOUT_MS 1000u

/* Limites de seguranca. */
#define MBTCP_MAX_TIMEOUT_MS 60000u
#define MBTCP_MAX_RETRIES 5u
#define MBTCP_MUTEX_TIMEOUT_MS 3000u

/* 1 = loga TX/RX em hexadecimal via tcpprobe. */
#ifndef MBTCP_TRACE
#define MBTCP_TRACE 0
#endif

/* ------------------------------------------------------------------ */
/* Function codes usadas pelo cliente                                  */
/* ------------------------------------------------------------------ */

enum {
    MBTCP_FUNCTION_READ_HOLDING_REGISTERS = 0x03,
    MBTCP_FUNCTION_WRITE_SINGLE_REGISTER = 0x06,
    MBTCP_FUNCTION_READ_WRITE_MULTIPLE_REGISTERS = 0x17
};

/* ------------------------------------------------------------------ */
/* Codigos de retorno                                                  */
/* ------------------------------------------------------------------ */

#define MBTCP_OK          0
#define MBTCP_ECONN      -1   /* socket/connect falhou, ou peer fechou  */
#define MBTCP_ESEND      -2   /* envio falhou                           */
#define MBTCP_ETIMEOUT   -3   /* timeout de envio/recepcao              */
#define MBTCP_EFRAME     -4   /* resposta invalida/fora de sincronia    */
#define MBTCP_EEXCEPTION -5   /* slave respondeu uma excecao Modbus     */
#define MBTCP_EBUSY      -6   /* mutex ocupado                          */
#define MBTCP_EINVAL     -7   /* argumento invalido                     */

/* ------------------------------------------------------------------ */
/* Sessao                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *ip;          /* "a.b.c.d"                                  */
    uint16_t port;           /* 502 no UHX15A; 1502/1503 no simulador      */
    uint8_t unit;            /* UHX15A: 0                                  */
    uint32_t timeout_ms;     /* 0 = MBTCP_DEFAULT_TIMEOUT_MS               */
    uint8_t retries_read;    /* tentativas extras em leitura (0..5)        */
    uint16_t rx_buf;         /* 0 = default do stack                       */
    uint16_t tx_buf;         /* 0 = default do stack                       */
    uint8_t nodelay;         /* 1 = TCP_NODELAY                            */
} mbtcp_cfg_t;

typedef struct {
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t timeouts;
    uint32_t exceptions;
    uint32_t frame_errors;
    uint32_t connect_errors;
    uint32_t connects;
    uint32_t retries;
    uint32_t latency_ms;
    int last_error;
    uint8_t connected;
} mbtcp_stats_t;

/* Preenche a configuracao com os defaults do UHX15A. */
void mbtcp_cfg_default(mbtcp_cfg_t *cfg);

/*
 * Configura a sessao sem abrir socket. A primeira transacao conecta.
 * Chame antes de iniciar as tasks que usam o cliente.
 */
int mbtcp_init(const mbtcp_cfg_t *cfg);

/* Abre (ou reabre) a conexao agora. */
int mbtcp_connect(void);

/* Fecha o socket. Seguro chamar sem ter aberto. */
void mbtcp_disconnect(void);

/* Estado atual do socket. */
bool mbtcp_is_connected(void);

/* Troca o unit ID da sessao. */
int mbtcp_set_unit(uint8_t unit);

/* Estatisticas internas. O ponteiro pertence ao cliente. */
const mbtcp_stats_t *mbtcp_stats(void);

/* ------------------------------------------------------------------ */
/* API principal (usa o unit configurado em mbtcp_init)                */
/* ------------------------------------------------------------------ */

/* FC03: le 1..125 holding registers. */
int mbtcp_read_holding(uint16_t address, uint16_t quantity,
                       uint16_t *registers);

/* FC06: escreve um holding register. */
int mbtcp_write_register(uint16_t address, uint16_t value);

/*
 * FC23: le e escreve registers na mesma transacao.
 * read_quantity: 1..125; write_quantity: 1..121.
 */
int mbtcp_read_write_registers(uint16_t read_address, uint16_t read_quantity,
                               uint16_t *read_registers,
                               uint16_t write_address, uint16_t write_quantity,
                               const uint16_t *write_registers);

/*
 * Leitura/escrita pontual por function code.
 *
 *   fc == 0x03: le quantity registers de address para registers.
 *   fc == 0x06: escreve registers[0] em address; quantity deve ser 1.
 *
 * Retorna MBTCP_OK ou erro. Outras FCs retornam MBTCP_EINVAL; use
 * mbtcp_read_write_registers() para FC23 ou mbtcp_raw_pdu() para o resto.
 */
int mbtcp_fc(uint8_t fc, uint16_t address, uint16_t quantity,
             uint16_t *registers);

/*
 * PDU cru: qualquer function code. A resposta inclui o function code.
 * Retorna MBTCP_EEXCEPTION se o slave responder uma excecao.
 */
int mbtcp_raw_pdu(const uint8_t *request_pdu, uint16_t request_pdu_length,
                  uint8_t *response_pdu, uint16_t response_capacity,
                  uint16_t *response_pdu_length);

/* ------------------------------------------------------------------ */
/* Diagnostico                                                         */
/* ------------------------------------------------------------------ */

/* Codigo da ultima excecao Modbus (1..11), 0 se nenhuma. */
int mbtcp_last_exception(void);

/* Texto do erro do cliente. */
const char *mbtcp_strerror(int error);

/* Texto da excecao Modbus. */
const char *mbtcp_exception_str(int exception_code);

#ifdef __cplusplus
}
#endif
#endif /* MBTCP_H */
