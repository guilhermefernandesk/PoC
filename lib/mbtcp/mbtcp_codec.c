/*
 * mbtcp_codec.c - conversao registro <-> valor, sem dependencia de FreeRTOS.
 */
#include "mbtcp_codec.h"

#include <string.h>

static float scale_or_one(float scale)
{
    return ((scale > 0.0f) || (scale < 0.0f)) ? scale : 1.0f;
}

/* IEEE-754 float32: NaN tem exp=0xFF e mantissa != 0. */
static int is_nan_f32(float f)
{
    uint32_t u;

    memcpy(&u, &f, sizeof(u));
    return ((u & 0x7F800000u) == 0x7F800000u) &&
           ((u & 0x007FFFFFu) != 0u);
}

uint16_t mbcodec_get_u16(const uint16_t *regs)
{
    return (regs == 0) ? 0u : regs[0];
}

int16_t mbcodec_get_s16(const uint16_t *regs)
{
    return (regs == 0) ? 0 : (int16_t)regs[0];
}

uint32_t mbcodec_get_u32(const uint16_t *regs, uint8_t word_order)
{
    uint32_t hi;
    uint32_t lo;

    if (regs == 0) {
        return 0u;
    }

    if (word_order == MBCODEC_LO_HI) {
        lo = (uint32_t)regs[0];
        hi = (uint32_t)regs[1];
    } else {
        hi = (uint32_t)regs[0];
        lo = (uint32_t)regs[1];
    }

    return (hi << 16) | lo;
}

int32_t mbcodec_get_s32(const uint16_t *regs, uint8_t word_order)
{
    return (int32_t)mbcodec_get_u32(regs, word_order);
}

float mbcodec_get_f32(const uint16_t *regs, uint8_t word_order)
{
    uint32_t u = mbcodec_get_u32(regs, word_order);
    float f;

    memcpy(&f, &u, sizeof(f));
    return f;
}

void mbcodec_put_u16(uint16_t *regs, uint16_t v)
{
    if (regs != 0) {
        regs[0] = v;
    }
}

void mbcodec_put_s16(uint16_t *regs, int16_t v)
{
    if (regs != 0) {
        regs[0] = (uint16_t)v;
    }
}

void mbcodec_put_u32(uint16_t *regs, uint32_t v, uint8_t word_order)
{
    uint16_t hi = (uint16_t)(v >> 16);
    uint16_t lo = (uint16_t)(v & 0xFFFFu);

    if (regs == 0) {
        return;
    }

    if (word_order == MBCODEC_LO_HI) {
        regs[0] = lo;
        regs[1] = hi;
    } else {
        regs[0] = hi;
        regs[1] = lo;
    }
}

void mbcodec_put_s32(uint16_t *regs, int32_t v, uint8_t word_order)
{
    mbcodec_put_u32(regs, (uint32_t)v, word_order);
}

void mbcodec_put_f32(uint16_t *regs, float v, uint8_t word_order)
{
    uint32_t u;

    memcpy(&u, &v, sizeof(u));
    mbcodec_put_u32(regs, u, word_order);
}

float mbcodec_scale(int32_t raw, float scale, float offset)
{
    return ((float)raw * scale_or_one(scale)) + offset;
}

int32_t mbcodec_unscale(float eng, float scale, float offset)
{
    float f = (eng - offset) / scale_or_one(scale);

    /* evita conversao float->int fora da faixa ou NaN (UB) */
    if (is_nan_f32(f)) {
        return 0;
    }
    if (f >= 2147483647.0f) {
        return INT32_MAX;
    }
    if (f <= -2147483648.0f) {
        return INT32_MIN;
    }

    /* arredonda sem depender de libm */
    if (f >= 0.0f) {
        return (int32_t)(f + 0.5f);
    }
    return (int32_t)(f - 0.5f);
}

uint8_t mbcodec_get_bit(uint16_t reg, uint8_t bit)
{
    if (bit > 15u) {
        return 0u;
    }
    return (uint8_t)((reg >> bit) & 1u);
}

uint16_t mbcodec_set_bit(uint16_t reg, uint8_t bit, uint8_t value)
{
    uint16_t mask;

    if (bit > 15u) {
        return reg;
    }

    mask = (uint16_t)(1u << bit);
    if (value != 0u) {
        return (uint16_t)(reg | mask);
    }
    return (uint16_t)(reg & (uint16_t)~mask);
}
