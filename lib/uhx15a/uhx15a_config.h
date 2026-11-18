/*
 * uhx15a_config.h - configuracao do driver UHX15A
 *
 * Tudo que e' configuracao do equipamento fica aqui: sessao Modbus TCP,
 * enderecos, escala, faixas e bits de status. O uhx15a.c contem apenas
 * codigo.
 *
 * MAPA PROVISORIO: os enderecos abaixo foram copiados do simulador para
 * o primeiro teste de comunicacao. Confirme no UHX15A/MOVISUITE antes de
 * escrever no equipamento real.
 */
#ifndef UHX15A_CONFIG_H
#define UHX15A_CONFIG_H

/* ------------------------------------------------------------------ */
/* Sessao Modbus TCP                                                   */
/* ------------------------------------------------------------------ */

/* #ifndef permite que o teste host sobrescreva via -D sem editar este .h. */
#ifndef UHX15A_IP
#define UHX15A_IP "192.168.10.4"
#endif

#ifndef UHX15A_PORT
#define UHX15A_PORT 502u
#endif

#ifndef UHX15A_UNIT_ID
#define UHX15A_UNIT_ID 0u
#endif

#ifndef UHX15A_TIMEOUT_MS
#define UHX15A_TIMEOUT_MS 800u
#endif

#ifndef UHX15A_RETRIES_READ
#define UHX15A_RETRIES_READ 1u
#endif

#define UHX15A_RX_BUF   0u
#define UHX15A_TX_BUF   0u
#define UHX15A_NODELAY  1u

/* ------------------------------------------------------------------ */
/* Process data - mapa provisorio                                      */
/* ------------------------------------------------------------------ */

#define UHX15A_POLL_START  1u
#define UHX15A_POLL_COUNT  16u

#define UHX15A_AZ_CONTROL  1u
#define UHX15A_AZ_STATUS   2u
#define UHX15A_AZ_TARGET   3u
#define UHX15A_AZ_ACTUAL   5u

#define UHX15A_EL_CONTROL  9u
#define UHX15A_EL_STATUS   10u
#define UHX15A_EL_TARGET   11u
#define UHX15A_EL_ACTUAL   13u

/* Regioes funcionais de um mesmo eixo nao podem compartilhar registradores. */
#define UHX15A_OVERLAP(a, an, b, bn) \
    (((a) < ((b) + (bn))) && ((b) < ((a) + (an))))

#if UHX15A_OVERLAP(UHX15A_AZ_CONTROL, 1u, UHX15A_AZ_STATUS, 1u) || \
    UHX15A_OVERLAP(UHX15A_AZ_CONTROL, 1u, UHX15A_AZ_TARGET, 2u) || \
    UHX15A_OVERLAP(UHX15A_AZ_CONTROL, 1u, UHX15A_AZ_ACTUAL, 2u) || \
    UHX15A_OVERLAP(UHX15A_AZ_STATUS, 1u, UHX15A_AZ_TARGET, 2u) || \
    UHX15A_OVERLAP(UHX15A_AZ_STATUS, 1u, UHX15A_AZ_ACTUAL, 2u) || \
    UHX15A_OVERLAP(UHX15A_AZ_TARGET, 2u, UHX15A_AZ_ACTUAL, 2u)
#error "regioes do eixo azimute se sobrepoem"
#endif

#if UHX15A_OVERLAP(UHX15A_EL_CONTROL, 1u, UHX15A_EL_STATUS, 1u) || \
    UHX15A_OVERLAP(UHX15A_EL_CONTROL, 1u, UHX15A_EL_TARGET, 2u) || \
    UHX15A_OVERLAP(UHX15A_EL_CONTROL, 1u, UHX15A_EL_ACTUAL, 2u) || \
    UHX15A_OVERLAP(UHX15A_EL_STATUS, 1u, UHX15A_EL_TARGET, 2u) || \
    UHX15A_OVERLAP(UHX15A_EL_STATUS, 1u, UHX15A_EL_ACTUAL, 2u) || \
    UHX15A_OVERLAP(UHX15A_EL_TARGET, 2u, UHX15A_EL_ACTUAL, 2u)
#error "regioes do eixo elevacao se sobrepoem"
#endif

#undef UHX15A_OVERLAP

#if (UHX15A_POLL_COUNT < 1u) || (UHX15A_POLL_COUNT > 125u)
#error "UHX15A_POLL_COUNT precisa ficar em 1..125 (limite do FC03)"
#endif

#if (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u) > 65535u
#error "bloco de poll UHX15A passa do endereco Modbus maximo"
#endif

/* O poll precisa cobrir todos os enderecos usados pelo driver. */
#if (UHX15A_AZ_CONTROL < UHX15A_POLL_START) || \
    (UHX15A_AZ_STATUS < UHX15A_POLL_START) || \
    (UHX15A_AZ_TARGET < UHX15A_POLL_START) || \
    (UHX15A_AZ_ACTUAL < UHX15A_POLL_START) || \
    (UHX15A_EL_CONTROL < UHX15A_POLL_START) || \
    (UHX15A_EL_STATUS < UHX15A_POLL_START) || \
    (UHX15A_EL_TARGET < UHX15A_POLL_START) || \
    (UHX15A_EL_ACTUAL < UHX15A_POLL_START)
#error "endereco UHX15A antes de UHX15A_POLL_START"
#endif

#if ((UHX15A_AZ_CONTROL + 1u) > (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u)) || \
    ((UHX15A_AZ_STATUS + 1u) > (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u)) || \
    ((UHX15A_AZ_TARGET + 1u) > (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u)) || \
    ((UHX15A_AZ_ACTUAL + 1u) > (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u)) || \
    ((UHX15A_EL_CONTROL + 1u) > (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u)) || \
    ((UHX15A_EL_STATUS + 1u) > (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u)) || \
    ((UHX15A_EL_TARGET + 1u) > (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u)) || \
    ((UHX15A_EL_ACTUAL + 1u) > (UHX15A_POLL_START + UHX15A_POLL_COUNT - 1u))
#error "endereco UHX15A fora de UHX15A_POLL_COUNT"
#endif

/* Valor de engenharia = bruto * escala + offset. */
#define UHX15A_POSITION_SCALE   1.0f
#define UHX15A_POSITION_OFFSET  0.0f

#define UHX15A_AZ_MIN_DEG  (-180.0f)
#define UHX15A_AZ_MAX_DEG  ( 180.0f)
#define UHX15A_EL_MIN_DEG  ( -90.0f)
#define UHX15A_EL_MAX_DEG  (  90.0f)

#define UHX15A_STATUS_ENABLED           0x0001u
#define UHX15A_STATUS_FIELDBUS_TIMEOUT  0x8000u

#endif /* UHX15A_CONFIG_H */
