/*
 * mbtcp_map.h - mapa de pontos Modbus compilado em C
 *
 * Ideia: a aplicacao descreve os registradores em uma tabela const
 * (nome, endereco, tipo, acesso, escala, ...). O mapa cuida de ler,
 * escrever, decodificar e manter um cache.
 *
 * Nao cria task. A aplicacao chama mbmap_tick() de uma task existente
 * (periodo tipico: 10..50 ms) e usa mbmap_read_cached() para telemetria.
 *
 * Perfil alvo: SEW UHX65A. O layout de process data (quais palavras sao
 * controle/status/setpoint/actual) vem do MOVISUITE; veja o exemplo em
 * mbtcp_demo.c.
 *
 * Limitacoes desta versao:
 *  - um grupo de poll le no maximo 125 registradores ou 2000 bits;
 *  - escrita de 32 bits usa FC23 (le 1, escreve 2) porque o UHX65A nao
 *    garante FC16. Defina MBMAP_USE_FC16=1 para escravos que suportam.
 *  - bit dentro de registrador usa read-modify-write com FC03+FC06;
 *    nao e' atomico. Para o control word, use um ponto U16 inteiro.
 */
#ifndef MBTCP_MAP_H
#define MBTCP_MAP_H

#include <stdint.h>

#include "mbtcp.h"
#include "mbtcp_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tabela Modbus */
#define MBT_HOLDING  0u   /* FC03/FC06/FC16/FC23 */
#define MBT_INPUT    1u   /* FC04                 */
#define MBT_COIL     2u   /* FC01/FC05/FC15       */
#define MBT_DISCRETE 3u   /* FC02                 */

/* Tipo do valor */
#define MBV_U16   0u
#define MBV_S16   1u
#define MBV_U32   2u
#define MBV_S32   3u
#define MBV_F32   4u
#define MBV_BIT   5u

/* Acesso */
#define MBA_RO    0u
#define MBA_RW    1u
#define MBA_WO    2u

/* Ordem das palavras de 32 bits */
#define MBW_HI_LO MBCODEC_HI_LO
#define MBW_LO_HI MBCODEC_LO_HI

/* 1 = escreve 32 bits com FC16; 0 = FC23 (UHX65A) */
#ifndef MBMAP_USE_FC16
#define MBMAP_USE_FC16 0
#endif

typedef struct {
    const char *name;       /* nome unico, usado por mbmap_id()          */
    uint16_t addr;          /* endereco Modbus (offset ja aplicado)      */
    uint8_t  table;         /* MBT_*                                     */
    uint8_t  type;          /* MBV_*                                     */
    uint8_t  access;        /* MBA_*                                     */
    uint8_t  word_order;    /* MBW_*                                     */
    uint8_t  bit;           /* 0..15 para MBV_BIT                        */
    uint8_t  group;         /* id do grupo de poll, 0 = sem grupo        */
    uint16_t period_ms;     /* documentacao; o poll usa o grupo          */
    float    scale;         /* eng = raw * scale + offset                */
    float    offset;
    float    min;           /* min < max: valida escrita                 */
    float    max;
} mbpoint_t;

typedef struct {
    uint8_t  id;            /* id do grupo (1..255)                      */
    uint8_t  table;         /* MBT_HOLDING/INPUT/COIL/DISCRETE           */
    uint16_t rd_addr;       /* inicio do bloco                           */
    uint16_t rd_qty;        /* tamanho do bloco                          */
    uint16_t period_ms;     /* periodo de poll                           */
    uint32_t next_tick;     /* estado interno; nao preencher             */
} mbgroup_t;

typedef struct {
    uint8_t  valid;
    uint8_t  type;
    uint32_t timestamp;     /* xTaskGetTickCount() da ultima leitura     */
    int32_t  raw_i;
    uint32_t raw_u;
    float    eng;
} mbval_t;

typedef struct {
    const mbpoint_t *points;
    uint16_t point_count;
    mbgroup_t *groups;      /* next_tick e' atualizado por mbmap_tick()  */
    uint16_t group_count;
    mbval_t *cache;         /* point_count posicoes                      */
} mbmap_t;

/*
 * Inicializa o mapa. A tabela de pontos pode ser const; a de grupos
 * precisa ser gravavel por causa de next_tick. Zera o cache.
 */
int mbmap_init(mbmap_t *m, const mbpoint_t *points, uint16_t point_count,
               mbgroup_t *groups, uint16_t group_count, mbval_t *cache);

/* Indice do ponto ou -1. */
int mbmap_id(const mbmap_t *m, const char *name);

/* Le o ponto agora, atualiza o cache e copia para out (out pode ser NULL). */
int mbmap_read(mbmap_t *m, uint16_t id, mbval_t *out);

/* Escreve o valor de engenharia, respeitando access/min/max/escala. */
int mbmap_write(mbmap_t *m, uint16_t id, float eng);

/* Copia o cache. Retorne valid=0 se o ponto ainda nao foi lido. */
int mbmap_read_cached(const mbmap_t *m, uint16_t id, mbval_t *out);

/* Le um bloco do grupo e atualiza todos os pontos pertencentes a ele. */
int mbmap_poll_group(mbmap_t *m, uint8_t group_id);

/* Chama mbmap_poll_group() nos grupos vencidos. Retorna o primeiro erro. */
int mbmap_tick(mbmap_t *m);

/* Le o ponto ate (raw_u & mask) == value ou timeout_ms. */
int mbmap_wait(mbmap_t *m, uint16_t id, uint32_t mask,
               uint32_t value, uint32_t timeout_ms);

void mbmap_invalidate(mbmap_t *m, uint16_t id);

/* Loga todos os pontos validos via tcpprobe. */
void mbmap_dump(const mbmap_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MBTCP_MAP_H */
