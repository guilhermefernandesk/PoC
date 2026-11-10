#include "board/clock.h"

#include "stm32h7xx.h"

/*
 * Clock tree implemented directly on RCC registers, no HAL.
 *
 * HSI 64 MHz -> PLL1: M=32, N=400, P=2, Q=2
 *   PLL input  = 2 MHz
 *   VCO        = 800 MHz (wide VCO range)
 *   SYSCLK     = 400 MHz
 *   HCLK       = 200 MHz (D1CPRE /1, HPRE /2)
 *   APB1/2/3/4 = 100 MHz
 *   Flash      = 4 wait states, WRHIGHFREQ=3
 */

#define PLL_M 32u
#define PLL_N 400u
#define PLL_P 2u
#define PLL_Q 2u
#define PLL_R 2u

static bool wait_until(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    for (uint32_t i = 0u; i < 2000000u; ++i) {
        if ((*reg & mask) == value) {
            return true;
        }
    }
    return false;
}

static void enable_eth_and_gpio_clocks(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_ETH1MACEN | RCC_AHB1ENR_ETH1TXEN |
                    RCC_AHB1ENR_ETH1RXEN;
    RCC->AHB2ENR |= RCC_AHB2ENR_SRAM1EN;
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN |
                    RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIOGEN;
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;
}

bool board_clock_init(void)
{
    /* HSI is the reset clock source. Make sure it is running. */
    RCC->CR |= RCC_CR_HSION;
    if (!wait_until(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY)) {
        return false;
    }

    /* LDO supply and VOS1 (400 MHz maximum for VOS1 on H743). */
    PWR->CR3 = (PWR->CR3 & ~PWR_CR3_BYPASS) | PWR_CR3_LDOEN;
    PWR->D3CR = (PWR->D3CR & ~PWR_D3CR_VOS) |
                (PWR_D3CR_VOS_1 | PWR_D3CR_VOS_0);
    if (!wait_until(&PWR->D3CR, PWR_D3CR_VOSRDY, PWR_D3CR_VOSRDY)) {
        return false;
    }

    /*
     * 4 wait states and WRHIGHFREQ=3 for 285..400 MHz.
     * Program this before switching SYSCLK to PLL1.
     */
    FLASH->ACR = FLASH_ACR_LATENCY_4WS |
                 FLASH_ACR_WRHIGHFREQ_1 | FLASH_ACR_WRHIGHFREQ_0;

    /* PLL1 source = HSI, M divider = 32. */
    RCC->PLLCKSELR = (RCC->PLLCKSELR &
                      ~(RCC_PLLCKSELR_PLLSRC | RCC_PLLCKSELR_DIVM1)) |
                     RCC_PLLCKSELR_PLLSRC_HSI |
                     ((uint32_t)PLL_M << RCC_PLLCKSELR_DIVM1_Pos);

    /*
     * PLL1RGE = 01 (2..4 MHz reference), PLL1VCOSEL = 0 (wide VCO).
     * No fractional divider.
     */
    RCC->PLLCFGR = RCC_PLLCFGR_PLL1RGE_1;
    RCC->PLL1FRACR = 0u;
    RCC->PLL1DIVR = ((uint32_t)(PLL_N - 1u) << RCC_PLL1DIVR_N1_Pos) |
                    ((uint32_t)(PLL_P - 1u) << RCC_PLL1DIVR_P1_Pos) |
                    ((uint32_t)(PLL_Q - 1u) << RCC_PLL1DIVR_Q1_Pos) |
                    ((uint32_t)(PLL_R - 1u) << RCC_PLL1DIVR_R1_Pos);

    RCC->CR |= RCC_CR_PLL1ON;
    if (!wait_until(&RCC->CR, RCC_CR_PLL1RDY, RCC_CR_PLL1RDY)) {
        return false;
    }

    /* CPU /1, AXI /2, APB3 /2, APB1 /2, APB2 /2, APB4 /2. */
    RCC->D1CFGR = RCC_D1CFGR_D1CPRE_DIV1 |
                  RCC_D1CFGR_HPRE_DIV2 |
                  RCC_D1CFGR_D1PPRE_DIV2;
    RCC->D2CFGR = RCC_D2CFGR_D2PPRE1_DIV2 |
                  RCC_D2CFGR_D2PPRE2_DIV2;
    RCC->D3CFGR = RCC_D3CFGR_D3PPRE_DIV2;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL1;
    if (!wait_until(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL1)) {
        return false;
    }

    enable_eth_and_gpio_clocks();

    SystemCoreClock = BOARD_SYSCLK_HZ;

    __DSB();
    __ISB();

    return true;
}
