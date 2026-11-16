/*
 * mbtcp_codec.h - conversao registro <-> valor, sem FreeRTOS
 *
 * Camada pura C, para poder ser testada no PC. Nao inclui nenhum header
 * do FreeRTOS. Usada pelo mapa de pontos (mbtcp_map).
 */
#ifndef MBTCP_CODEC_H
#define MBTCP_CODEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ordem das palavras em valores de 32 bits. */
#define MBCODEC_HI_LO 0u   /* primeiro registrador = high word (AB CD) */
#define MBCODEC_LO_HI 1u   /* primeiro registrador = low word  (CD AB) */

int mbcodec_is_finite(float value);

uint16_t mbcodec_get_u16(const uint16_t *registers);
int16_t  mbcodec_get_s16(const uint16_t *registers);
uint32_t mbcodec_get_u32(const uint16_t *registers, uint8_t word_order);
int32_t  mbcodec_get_s32(const uint16_t *registers, uint8_t word_order);
float    mbcodec_get_f32(const uint16_t *registers, uint8_t word_order);

void mbcodec_put_u16(uint16_t *registers, uint16_t value);
void mbcodec_put_s16(uint16_t *registers, int16_t value);
void mbcodec_put_u32(uint16_t *registers, uint32_t value, uint8_t word_order);
void mbcodec_put_s32(uint16_t *registers, int32_t value, uint8_t word_order);
void mbcodec_put_f32(uint16_t *registers, float value, uint8_t word_order);

/* Valor de engenharia = valor bruto * scale + offset. */
float mbcodec_scale(int32_t raw, float scale, float offset);
float mbcodec_scale_unsigned(uint32_t raw, float scale, float offset);

/* Valor bruto arredondado e limitado a faixa do tipo. */
int32_t  mbcodec_unscale(float value, float scale, float offset);
uint32_t mbcodec_unscale_unsigned(float value, float scale, float offset);

#ifdef __cplusplus
}
#endif
#endif /* MBTCP_CODEC_H */
