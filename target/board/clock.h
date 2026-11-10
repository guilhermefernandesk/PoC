#ifndef BOARD_CLOCK_H
#define BOARD_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_SYSCLK_HZ 400000000u
#define BOARD_HCLK_HZ   200000000u
#define BOARD_PCLK_HZ   100000000u

bool board_clock_init(void);

#endif
