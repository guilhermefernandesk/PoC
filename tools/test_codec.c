/*
 * test_codec.c - testes host do codec (sem FreeRTOS).
 *
 * Compile/run:
 *   gcc -std=c99 -Wall -Wextra -Ilib/mbtcp \
 *       tools/test_codec.c lib/mbtcp/mbtcp_codec.c -lm -o /tmp/test_codec
 *   /tmp/test_codec
 */
#include "mbtcp_codec.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static int close_enough(float a, float b)
{
    return fabsf(a - b) < 0.001f;
}

int main(void)
{
    uint16_t regs[2];
    uint16_t r16[] = { 0x1234u };

    assert(mbcodec_get_u16(r16) == 0x1234u);
    assert(mbcodec_get_s16(r16) == 0x1234);

    r16[0] = 0xFFFEu;
    assert(mbcodec_get_s16(r16) == -2);

    mbcodec_put_u32(regs, 0x12345678u, MBCODEC_HI_LO);
    assert(regs[0] == 0x1234u);
    assert(regs[1] == 0x5678u);
    assert(mbcodec_get_u32(regs, MBCODEC_HI_LO) == 0x12345678u);

    mbcodec_put_u32(regs, 0x12345678u, MBCODEC_LO_HI);
    assert(regs[0] == 0x5678u);
    assert(regs[1] == 0x1234u);
    assert(mbcodec_get_u32(regs, MBCODEC_LO_HI) == 0x12345678u);

    mbcodec_put_s32(regs, -123456, MBCODEC_HI_LO);
    assert(mbcodec_get_s32(regs, MBCODEC_HI_LO) == -123456);
    mbcodec_put_s32(regs, -123456, MBCODEC_LO_HI);
    assert(mbcodec_get_s32(regs, MBCODEC_LO_HI) == -123456);

    mbcodec_put_f32(regs, 3.14159f, MBCODEC_HI_LO);
    assert(close_enough(mbcodec_get_f32(regs, MBCODEC_HI_LO), 3.14159f));
    mbcodec_put_f32(regs, -0.5f, MBCODEC_LO_HI);
    assert(close_enough(mbcodec_get_f32(regs, MBCODEC_LO_HI), -0.5f));

    assert(close_enough(mbcodec_scale(1234, 0.001f, 0.0f), 1.234f));
    assert(mbcodec_unscale(1.234f, 0.001f, 0.0f) == 1234);
    assert(close_enough(mbcodec_scale(-100, 0.5f, 10.0f), -40.0f));
    assert(mbcodec_unscale(-40.0f, 0.5f, 10.0f) == -100);
    assert(close_enough(mbcodec_scale(7, 0.0f, 3.0f), 10.0f));

    assert(mbcodec_unscale(NAN, 1.0f, 0.0f) == 0);

    assert(mbcodec_get_bit(0x8001u, 0) == 1u);
    assert(mbcodec_get_bit(0x8001u, 15) == 1u);
    assert(mbcodec_get_bit(0x8001u, 1) == 0u);
    assert(mbcodec_get_bit(0xFFFFu, 16u) == 0u);
    assert(mbcodec_set_bit(0x0000u, 3, 1) == 0x0008u);
    assert(mbcodec_set_bit(0xFFFFu, 3, 0) == 0xFFF7u);
    assert(mbcodec_set_bit(0x1234u, 16u, 1) == 0x1234u);

    assert(mbcodec_get_u16(NULL) == 0u);
    assert(mbcodec_get_s16(NULL) == 0);
    assert(mbcodec_get_u32(NULL, MBCODEC_HI_LO) == 0u);

    puts("test_codec: ok");
    return 0;
}
