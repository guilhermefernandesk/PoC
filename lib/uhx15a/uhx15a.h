/*
 * uhx15a.h - driver do controlador UHX15A
 *
 * A aplicacao usa apenas esta API. Enderecos, escala, bits, IP e timeout
 * ficam em uhx15a_config.h. Nao e' necessario conhecer registradores nem
 * chamar o transporte Modbus diretamente.
 *
 * Unidades:
 *   - posicao: graus
 *   - comandos de movimento: uhx15a_set_target()
 *
 * O driver nao cria task. Chame uhx15a_poll() de uma task de processo e
 * nunca do callback do MQTT.
 */
#ifndef UHX15A_H
#define UHX15A_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UHX15A_AXIS_AZIMUTH = 0,
    UHX15A_AXIS_ELEVATION,
    UHX15A_AXIS_COUNT
} uhx15a_axis_t;

typedef enum {
    UHX15A_OK = 0,
    UHX15A_ERR_ARG,
    UHX15A_ERR_LINK,
    UHX15A_ERR_TIMEOUT,
    UHX15A_ERR_PROTOCOL,
    UHX15A_ERR_MODBUS,
    UHX15A_ERR_RANGE,
    UHX15A_ERR_BUSY
} uhx15a_result_t;

/*
 * Configura a sessao a partir de uhx15a_config.h. Nao abre o socket;
 * a primeira leitura/escrita conecta.
 */
uhx15a_result_t uhx15a_init(void);

/* Le status e posicao real dos dois eixos. */
uhx15a_result_t uhx15a_poll(void);

/*
 * Escreve o setpoint de posicao. Nao habilita o drive nem dispara
 * movimento; o enable/controle vem nas proximas etapas.
 */
uhx15a_result_t uhx15a_set_target(uhx15a_axis_t axis, float position_deg);

/* Estado da ultima leitura. */
bool     uhx15a_online(void);
bool     uhx15a_enabled(uhx15a_axis_t axis);
bool     uhx15a_fieldbus_timeout(uhx15a_axis_t axis);
float    uhx15a_position_deg(uhx15a_axis_t axis);
uint16_t uhx15a_status_word(uhx15a_axis_t axis);

/* Texto curto do ultimo erro para log/telemetria. */
const char *uhx15a_last_error(void);

/* Fecha o socket e invalida o estado. */
void uhx15a_close(void);

#ifdef __cplusplus
}
#endif
#endif /* UHX15A_H */
