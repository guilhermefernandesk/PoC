#ifndef BOARD_GPIO_H
#define BOARD_GPIO_H

/*
 * RMII pinout for NUCLEO-H743ZI2 + LAN8742A:
 *
 *   PA1  ETH_REF_CLK   AF11
 *   PA2  ETH_MDIO      AF11
 *   PA7  ETH_CRS_DV    AF11
 *   PC1  ETH_MDC       AF11
 *   PC4  ETH_RXD0      AF11
 *   PC5  ETH_RXD1      AF11
 *   PG11 ETH_TX_EN     AF11
 *   PG13 ETH_TXD0      AF11
 *   PB13 ETH_TXD1      AF11
 */
void board_gpio_init(void);

#endif
