/*
 * mbtcp_map.h - mapa de pontos Modbus compilado em C
 *
 * A aplicacao descreve os holding registers em uma tabela const. O mapa
 * le, escreve, decodifica e mantem um cache dos valores de engenharia.
 *
 * Perfil alvo: SEW UHX65A (unit 0, offset 0, FC03/FC06/FC23).
 *
 * Nao cria task. A aplicacao chama mbmap_tick() de uma task existente
 * (periodo tipico: 10..50 ms) e usa mbmap_read_cached() para telemetria.
 *
 * Limitacoes desta versao:
 *  - somente holding registers (FC03/FC06/FC23);
 *  - um grupo de poll le no maximo 125 registers;
 *  - escrita de 32 bits usa FC23 (le 1, escreve 2 no mesmo endereco);
 *  - nao ha pontos de bit nem read-modify-write.
 */
#ifndef MBTCP_MAP_H
#define MBTCP_MAP_H

#include <stdint.h>

#include "mbtcp.h"
#include "mbtcp_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tipo do valor. */
#define MBV_U16 0u
#define MBV_S16 1u
#define MBV_U32 2u
#define MBV_S32 3u
#define MBV_F32 4u

/* Acesso. */
#define MBA_RO 0u
#define MBA_RW 1u
#define MBA_WO 2u

/* Ordem das palavras de 32 bits. */
#define MBW_HI_LO MBCODEC_HI_LO
#define MBW_LO_HI MBCODEC_LO_HI

/* Grupo 0 significa "ponto sem poll automatico". */
#define MBMAP_NO_GROUP 0u

/* Maior bloco lido de uma vez. */
#define MBMAP_MAX_REGISTERS 125u

typedef struct {
    const char *name;       /* nome unico, usado por mbmap_id()          */
    uint16_t address;       /* endereco Modbus (offset ja aplicado)      */
    uint8_t type;           /* MBV_*                                     */
    uint8_t access;         /* MBA_*                                     */
    uint8_t word_order;     /* MBW_*                                     */
    uint8_t group;          /* id do grupo de poll, 0 = sem grupo        */
    float scale;            /* valor = bruto * scale + offset            */
    float offset;
    float minimum;          /* faixa aceita para escrita                 */
    float maximum;
} mbpoint_t;

typedef struct {
    uint8_t id;             /* id do grupo (1..255)                      */
    uint16_t address;       /* inicio do bloco                           */
    uint16_t quantity;      /* tamanho do bloco, 1..125                  */
    uint16_t period_ms;     /* periodo minimo de poll                    */
    uint32_t next_tick;     /* estado interno; nao preencher             */
} mbgroup_t;

typedef struct {
    uint8_t valid;
    uint8_t type;
    uint32_t timestamp;     /* xTaskGetTickCount() da ultima leitura     */
    int32_t raw_signed;     /* interpretacao com sinal do valor bruto    */
    uint32_t raw_unsigned;  /* bits brutos do registrador(es)            */
    float value;            /* valor de engenharia                       */
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
int mbmap_init(mbmap_t *map, const mbpoint_t *points, uint16_t point_count,
               mbgroup_t *groups, uint16_t group_count, mbval_t *cache);

/* Indice do ponto ou -1. */
int mbmap_id(const mbmap_t *map, const char *name);

/* Le o ponto agora, atualiza o cache e copia para out (out pode ser NULL). */
int mbmap_read(mbmap_t *map, uint16_t point_id, mbval_t *out);

/* Escreve o valor de engenharia, respeitando access/min/max/escala. */
int mbmap_write(mbmap_t *map, uint16_t point_id, float value);

/* Copia o cache. Retorne valid=0 se o ponto ainda nao foi lido. */
int mbmap_read_cached(const mbmap_t *map, uint16_t point_id, mbval_t *out);

/* Le um bloco do grupo e atualiza todos os pontos pertencentes a ele. */
int mbmap_poll_group(mbmap_t *map, uint8_t group_id);

/* Chama mbmap_poll_group() nos grupos vencidos. Retorna o primeiro erro. */
int mbmap_tick(mbmap_t *map);

/* Le o ponto ate (raw_unsigned & mask) == expected ou timeout_ms. */
int mbmap_wait(mbmap_t *map, uint16_t point_id, uint32_t mask,
               uint32_t expected, uint32_t timeout_ms);

void mbmap_invalidate(mbmap_t *map, uint16_t point_id);

/* Loga todos os pontos validos via tcpprobe. */
void mbmap_dump(const mbmap_t *map);

#ifdef __cplusplus
}
#endif
#endif /* MBTCP_MAP_H */
