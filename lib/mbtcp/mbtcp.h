/*
 * mbtcp.h - cliente Modbus TCP para FreeRTOS-Plus-TCP
 *
 * Um socket, um mutex, sem task permanente. O socket e' criado no
 * primeiro uso e reconectado automaticamente se cair.
 *
 * Depende de tcpprobe.h apenas para logging (tcpprobe_log/_hex).
 *
 * Thread-safety: as funcoes publicas podem ser chamadas de qualquer task;
 * o acesso ao socket e' serializado por um mutex interno.
 *
 * Perfil alvo: controlador SEW UHX65A (unit 0, offset 0, FC3/6/23).
 * As demais FCs tambem estao disponiveis para outros escravos/testes.
 */
#ifndef MBTCP_H
#define MBTCP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MBTCP_VERSION "2.0.0"

/* ------------------------------------------------------------------ */
/* Configuracao                                                        */
/* ------------------------------------------------------------------ */

/* Maior frame Modbus TCP (MBAP 6 + unit 1 + PDU 253) */
#define MBTCP_MAX_FRAME 260

/* Maior PDU (function code + data) */
#define MBTCP_MAX_PDU 253

/* Timeout padrao de connect/send/recv, em ms */
#define MBTCP_TIMEOUT_MS 1000u

/* Tempo maximo esperando o mutex do socket, em ms */
#define MBTCP_MUTEX_TIMEOUT_MS 3000u

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
/* Configuracao da sessao                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *ip;          /* "a.b.c.d"                                 */
    uint16_t port;           /* 502 para SEW; 1502 no simulador           */
    uint8_t  unit;           /* UHX65A: 0                                 */
    uint32_t timeout_ms;     /* 0 = MBTCP_TIMEOUT_MS                      */
    uint8_t  retries_read;   /* tentativas extras em leitura (0..5)       */
    uint8_t  retries_write;  /* tentativas extras em escrita (default 0)  */
    uint16_t rx_buf;         /* 0 = default do stack                      */
    uint16_t tx_buf;         /* 0 = default do stack                      */
    uint8_t  nodelay;        /* 1 = TCP_NODELAY, se disponivel            */
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
    int      last_error;
    uint8_t  connected;
} mbtcp_stats_t;

/* Preenche cfg com os defaults (UHX65A: port 502, unit 0). */
void mbtcp_cfg_default(mbtcp_cfg_t *cfg);

/*
 * Configura a sessao sem abrir socket. A primeira transacao conecta.
 * Pode ser chamada mais de uma vez; fecha a sessao anterior.
 */
int mbtcp_init(const mbtcp_cfg_t *cfg);

/* Abre (ou reabre) a conexao agora. */
int mbtcp_connect(void);

/* Fecha o socket. Seguro chamar sem ter aberto. */
void mbtcp_disconnect(void);

/* Troca o unit ID da sessao. */
int mbtcp_set_unit(uint8_t unit);

/* Estatisticas (ponteiro para o estado interno; leitura simples). */
const mbtcp_stats_t *mbtcp_stats(void);

/* ------------------------------------------------------------------ */
/* API compativel com a versao anterior                                */
/* ------------------------------------------------------------------ */

int mbtcp_open(const char *ip, uint16_t port, uint32_t timeout_ms);
void mbtcp_close(void);

/* FC03/FC04, com unit explicito (compatibilidade). */
int mbtcp_read(uint8_t unit, uint8_t fc, uint16_t addr,
               uint16_t qty, uint16_t *out);

/* FC06, com unit explicito (compatibilidade). */
int mbtcp_write(uint8_t unit, uint16_t addr, uint16_t value);

/* FC16, com unit explicito (compatibilidade). */
int mbtcp_write_multi(uint8_t unit, uint16_t addr,
                      uint16_t qty, const uint16_t *values);

/* ------------------------------------------------------------------ */
/* API nova (usa o unit configurado em mbtcp_init)                     */
/* ------------------------------------------------------------------ */

/* FC01/FC02: bits em 1 byte por coil (0 ou 1). qty: 1..2000. */
int mbtcp_read_coils(uint16_t addr, uint16_t qty, uint8_t *bits);
int mbtcp_read_discrete_inputs(uint16_t addr, uint16_t qty, uint8_t *bits);

/* FC03/FC04: qty 1..125. */
int mbtcp_read_holding(uint16_t addr, uint16_t qty, uint16_t *out);
int mbtcp_read_input(uint16_t addr, uint16_t qty, uint16_t *out);

/* FC05/FC06. */
int mbtcp_write_coil(uint16_t addr, int on);
int mbtcp_write_register(uint16_t addr, uint16_t value);

/* FC15/FC16. */
int mbtcp_write_coils(uint16_t addr, uint16_t qty, const uint8_t *bits);
int mbtcp_write_registers(uint16_t addr, uint16_t qty,
                          const uint16_t *values);

/* FC22 (0x16): mask write register. */
int mbtcp_mask_write_register(uint16_t addr,
                              uint16_t and_mask, uint16_t or_mask);

/* FC23 (0x17): read/write multiplos. rqty 1..125, wqty 1..121. */
int mbtcp_read_write_registers(uint16_t raddr, uint16_t rqty, uint16_t *rout,
                               uint16_t waddr, uint16_t wqty,
                               const uint16_t *win);

/* FC24 (0x18): read FIFO. count <= 31. */
int mbtcp_read_fifo(uint16_t addr, uint16_t *out, uint16_t cap,
                    uint16_t *count);

/* FC20/FC21 (0x14/0x15): um file record por chamada. */
int mbtcp_read_file_record(uint16_t file, uint16_t record,
                           uint16_t qty, uint16_t *out);
int mbtcp_write_file_record(uint16_t file, uint16_t record,
                            uint16_t qty, const uint16_t *in);

/* FC43/14 (0x2B/0x0E): Read Device Identification. */
typedef void (*mbtcp_devinfo_cb)(uint8_t obj_id, const uint8_t *data,
                                 uint8_t len, void *ctx);

int mbtcp_read_device_id(uint8_t category,
                         mbtcp_devinfo_cb cb, void *ctx);

/* Igual, mas comeca pelo objeto obj_id (categoria 4 = objeto individual). */
int mbtcp_read_device_id_obj(uint8_t category, uint8_t obj_id,
                             mbtcp_devinfo_cb cb, void *ctx);

/* PDU cru: qualquer function code. Retorna a PDU de resposta (fc + data). */
int mbtcp_raw_pdu(const uint8_t *pdu, uint16_t pdu_len,
                  uint8_t *rsp, uint16_t rsp_cap, uint16_t *rsp_len);

/* Codigo da ultima excecao Modbus (1..11), 0 se nenhuma. */
int mbtcp_last_exception(void);

/* Texto do erro. */
const char *mbtcp_strerror(int err);

/* Texto da excecao Modbus. */
const char *mbtcp_exception_str(int code);

/* ------------------------------------------------------------------ */
/* Demo/autoteste (implementada em mbtcp_demo.c, nao compilada pelo  */
/* mbtcp.mk).                                                         */
/* ------------------------------------------------------------------ */

int demo_init(void);
int mbtcp_selftest(const char *ip, uint16_t port, uint8_t unit);

#ifdef __cplusplus
}
#endif
#endif /* MBTCP_H */
