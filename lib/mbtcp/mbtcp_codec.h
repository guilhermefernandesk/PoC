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
#define MBCODEC_HI_LO 0u   /* registrador baixo = high word (AB CD) */
#define MBCODEC_LO_HI 1u   /* registrador baixo = low word  (CD AB) */

uint16_t mbcodec_get_u16(const uint16_t *regs);
int16_t  mbcodec_get_s16(const uint16_t *regs);
uint32_t mbcodec_get_u32(const uint16_t *regs, uint8_t word_order);
int32_t  mbcodec_get_s32(const uint16_t *regs, uint8_t word_order);
float    mbcodec_get_f32(const uint16_t *regs, uint8_t word_order);

void mbcodec_put_u16(uint16_t *regs, uint16_t v);
void mbcodec_put_s16(uint16_t *regs, int16_t v);
void mbcodec_put_u32(uint16_t *regs, uint32_t v, uint8_t word_order);
void mbcodec_put_s32(uint16_t *regs, int32_t v, uint8_t word_order);
void mbcodec_put_f32(uint16_t *regs, float v, uint8_t word_order);

/* eng = raw * scale + offset. scale == 0 e' tratado como 1. */
float   mbcodec_scale(int32_t raw, float scale, float offset);
int32_t mbcodec_unscale(float eng, float scale, float offset);

/* Bit helpers. bit: 0..15. */
uint8_t  mbcodec_get_bit(uint16_t reg, uint8_t bit);
uint16_t mbcodec_set_bit(uint16_t reg, uint8_t bit, uint8_t value);

#ifdef __cplusplus
}
#endif
#endif /* MBTCP_CODEC_H */
