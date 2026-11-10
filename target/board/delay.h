#ifndef BOARD_DELAY_H
#define BOARD_DELAY_H

#include <stdint.h>

/* DWT-based busy delay. Call after board_clock_init() and before use. */
void board_delay_init(void);
void board_delay_ms(uint32_t milliseconds);

#endif
