#include "eth/mdio.h"

#include "stm32h7xx.h"

#include <stddef.h>

#define MDIO_TIMEOUT 100000u

static bool mdio_wait_idle(void)
{
    for (uint32_t i = 0u; i < MDIO_TIMEOUT; ++i) {
        if ((ETH->MACMDIOAR & ETH_MACMDIOAR_MB) == 0u) {
            return true;
        }
    }
    return false;
}

static uint32_t mdio_address(uint8_t phy_address,
                             uint8_t register_address,
                             uint32_t operation)
{
    return ((uint32_t)phy_address << ETH_MACMDIOAR_PA_Pos) |
           ((uint32_t)register_address << ETH_MACMDIOAR_RDA_Pos) |
           ETH_MACMDIOAR_CR_DIV102 |
           operation |
           ETH_MACMDIOAR_MB;
}

void mdio_init(void)
{
    /* Clear any pending transaction and set the clock range. */
    ETH->MACMDIOAR = (ETH->MACMDIOAR & ~ETH_MACMDIOAR_CR) |
                     ETH_MACMDIOAR_CR_DIV102;
}

bool mdio_write(uint8_t phy_address, uint8_t register_address, uint16_t value)
{
    if (!mdio_wait_idle()) {
        return false;
    }

    ETH->MACMDIODR = value;
    ETH->MACMDIOAR = mdio_address(phy_address, register_address,
                                  ETH_MACMDIOAR_MOC_WR);

    return mdio_wait_idle();
}

bool mdio_read(uint8_t phy_address, uint8_t register_address, uint16_t *value)
{
    if ((value == NULL) || !mdio_wait_idle()) {
        return false;
    }

    ETH->MACMDIOAR = mdio_address(phy_address, register_address,
                                  ETH_MACMDIOAR_MOC_RD);

    if (!mdio_wait_idle()) {
        return false;
    }

    *value = (uint16_t)(ETH->MACMDIODR & 0xFFFFu);
    return true;
}
