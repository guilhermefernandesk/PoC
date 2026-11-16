/*
 * mbtcp_codec.c - conversao registro <-> valor, sem dependencia de FreeRTOS.
 */
#include "mbtcp_codec.h"

#include <string.h>

int mbcodec_is_finite(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7F800000u) != 0x7F800000u;
}

uint16_t mbcodec_get_u16(const uint16_t *registers)
{
    return (registers == NULL) ? 0u : registers[0];
}

int16_t mbcodec_get_s16(const uint16_t *registers)
{
    return (registers == NULL) ? 0 : (int16_t)registers[0];
}

uint32_t mbcodec_get_u32(const uint16_t *registers, uint8_t word_order)
{
    uint32_t high;
    uint32_t low;

    if (registers == NULL) {
        return 0u;
    }

    if (word_order == MBCODEC_LO_HI) {
        low = (uint32_t)registers[0];
        high = (uint32_t)registers[1];
    } else {
        high = (uint32_t)registers[0];
        low = (uint32_t)registers[1];
    }

    return (high << 16) | low;
}

int32_t mbcodec_get_s32(const uint16_t *registers, uint8_t word_order)
{
    return (int32_t)mbcodec_get_u32(registers, word_order);
}

float mbcodec_get_f32(const uint16_t *registers, uint8_t word_order)
{
    uint32_t bits = mbcodec_get_u32(registers, word_order);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

void mbcodec_put_u16(uint16_t *registers, uint16_t value)
{
    if (registers != NULL) {
        registers[0] = value;
    }
}

void mbcodec_put_s16(uint16_t *registers, int16_t value)
{
    if (registers != NULL) {
        registers[0] = (uint16_t)value;
    }
}

void mbcodec_put_u32(uint16_t *registers, uint32_t value, uint8_t word_order)
{
    uint16_t high = (uint16_t)(value >> 16);
    uint16_t low = (uint16_t)(value & 0xFFFFu);

    if (registers == NULL) {
        return;
    }

    if (word_order == MBCODEC_LO_HI) {
        registers[0] = low;
        registers[1] = high;
    } else {
        registers[0] = high;
        registers[1] = low;
    }
}

void mbcodec_put_s32(uint16_t *registers, int32_t value, uint8_t word_order)
{
    mbcodec_put_u32(registers, (uint32_t)value, word_order);
}

void mbcodec_put_f32(uint16_t *registers, float value, uint8_t word_order)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    mbcodec_put_u32(registers, bits, word_order);
}

float mbcodec_scale(int32_t raw, float scale, float offset)
{
    return ((float)raw * scale) + offset;
}

float mbcodec_scale_unsigned(uint32_t raw, float scale, float offset)
{
    return ((float)raw * scale) + offset;
}

static int can_unscale(float value, float scale, float offset)
{
    return mbcodec_is_finite(value) &&
           mbcodec_is_finite(scale) &&
           mbcodec_is_finite(offset) &&
           (scale != 0.0f);
}

int32_t mbcodec_unscale(float value, float scale, float offset)
{
    float raw;

    if (!can_unscale(value, scale, offset)) {
        return 0;
    }

    raw = (value - offset) / scale;
    if (!mbcodec_is_finite(raw)) {
        if (raw > 0.0f) {
            return INT32_MAX;
        }
        if (raw < 0.0f) {
            return INT32_MIN;
        }
        return 0;
    }
    if (raw >= (float)INT32_MAX) {
        return INT32_MAX;
    }
    if (raw <= (float)INT32_MIN) {
        return INT32_MIN;
    }

    /* Arredonda sem depender de libm. */
    if (raw >= 0.0f) {
        return (int32_t)(raw + 0.5f);
    }
    return (int32_t)(raw - 0.5f);
}

uint32_t mbcodec_unscale_unsigned(float value, float scale, float offset)
{
    float raw;

    if (!can_unscale(value, scale, offset)) {
        return 0u;
    }

    raw = (value - offset) / scale;
    if (!mbcodec_is_finite(raw)) {
        return (raw > 0.0f) ? UINT32_MAX : 0u;
    }
    if (raw <= 0.0f) {
        return 0u;
    }
    if (raw >= (float)UINT32_MAX) {
        return UINT32_MAX;
    }

    return (uint32_t)(raw + 0.5f);
}
