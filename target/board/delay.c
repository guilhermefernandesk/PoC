#include "board/delay.h"

#include "board/clock.h"
#include "stm32h7xx.h"

void board_delay_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void board_delay_ms(uint32_t milliseconds)
{
    const uint32_t cycles_per_ms = BOARD_SYSCLK_HZ / 1000u;
    uint32_t total = milliseconds * cycles_per_ms;

    if ((milliseconds != 0u) && (total / milliseconds != cycles_per_ms)) {
        total = 0xFFFFFFFFu;
    }

    const uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < total) {
        __NOP();
    }
}
