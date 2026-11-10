#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321
#define BYTE_ORDER LITTLE_ENDIAN

/* The PoC has no console. Keep diagnostics and assertions free of printf so
 * the target does not pull newlib stdio/syscalls into the image. */
#define LWIP_PLATFORM_DIAG(x) \
    do {                      \
    } while (0)

#define LWIP_PLATFORM_ASSERT(x) \
    do {                        \
        (void)(x);              \
        for (;;) {              \
        }                       \
    } while (0)

/* Simple deterministic source for TCP initial sequence numbers. */
static inline uint32_t lwip_platform_rand(void)
{
    static uint32_t state = 0x12345678u;
    state = (state * 1664525u) + 1013904223u;
    return state;
}

#define LWIP_RAND() ((u32_t)lwip_platform_rand())

#endif
