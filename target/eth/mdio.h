#ifndef ETH_MDIO_H
#define ETH_MDIO_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Clause 22 MDIO access through the STM32H7 ETH MAC.
 *
 * HCLK is 200 MHz, so the MDIO clock divider is /102 (~1.96 MHz).
 */
void mdio_init(void);
bool mdio_write(uint8_t phy_address, uint8_t register_address, uint16_t value);
bool mdio_read(uint8_t phy_address, uint8_t register_address, uint16_t *value);

#endif
