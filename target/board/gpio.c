#include "board/gpio.h"

#include "stm32h7xx.h"

#define GPIO_AF_ETH 11u

static void configure_af(GPIO_TypeDef *port,
                         uint32_t pin,
                         uint32_t alternate,
                         uint32_t open_drain,
                         uint32_t speed,
                         uint32_t pull)
{
    const uint32_t shift = pin * 2u;

    port->MODER = (port->MODER & ~(0x3u << shift)) | (0x2u << shift);
    port->OTYPER = (port->OTYPER & ~(0x1u << pin)) | (open_drain << pin);
    port->OSPEEDR = (port->OSPEEDR & ~(0x3u << shift)) | (speed << shift);
    port->PUPDR = (port->PUPDR & ~(0x3u << shift)) | (pull << shift);

    const uint32_t af_shift = (pin & 0x7u) * 4u;
    const uint32_t af_index = (pin >> 3u) & 0x1u;
    port->AFR[af_index] =
        (port->AFR[af_index] & ~(0xFu << af_shift)) | (alternate << af_shift);
}

void board_gpio_init(void)
{
    /* Select RMII in SYSCFG. A dummy read ensures the write is committed. */
    SYSCFG->PMCR = (SYSCFG->PMCR & ~SYSCFG_PMCR_EPIS_SEL) |
                   SYSCFG_PMCR_EPIS_SEL_2;
    (void)SYSCFG->PMCR;

    /* High speed push-pull alternate function for all RMII signals. */
    configure_af(GPIOA, 1u, GPIO_AF_ETH, 0u, 0x3u, 0u); /* REF_CLK */
    configure_af(GPIOA, 2u, GPIO_AF_ETH, 0u, 0x3u, 0x1u); /* MDIO, pull-up */
    configure_af(GPIOA, 7u, GPIO_AF_ETH, 0u, 0x3u, 0u); /* CRS_DV */

    configure_af(GPIOC, 1u, GPIO_AF_ETH, 0u, 0x3u, 0u); /* MDC */
    configure_af(GPIOC, 4u, GPIO_AF_ETH, 0u, 0x3u, 0u); /* RXD0 */
    configure_af(GPIOC, 5u, GPIO_AF_ETH, 0u, 0x3u, 0u); /* RXD1 */

    configure_af(GPIOG, 11u, GPIO_AF_ETH, 0u, 0x3u, 0u); /* TX_EN */
    configure_af(GPIOG, 13u, GPIO_AF_ETH, 0u, 0x3u, 0u); /* TXD0 */

    configure_af(GPIOB, 13u, GPIO_AF_ETH, 0u, 0x3u, 0u); /* TXD1 */
}
