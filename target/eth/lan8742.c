#include "eth/lan8742.h"

#include "board/delay.h"
#include "eth/mdio.h"

#include <stddef.h>

#define PHY_REG_BCR 0x00u
#define PHY_REG_BSR 0x01u
#define PHY_REG_ID1 0x02u
#define PHY_REG_ID2 0x03u

#define BCR_RESET          0x8000u
#define BCR_AUTONEG_ENABLE 0x1000u
#define BCR_AUTONEG_RESTART 0x0200u

#define BSR_LINK_STATUS 0x0004u
#define BSR_AN_COMPLETE 0x0020u
#define BSR_100BASE_TX_FULL 0x4000u
#define BSR_100BASE_TX_HALF 0x2000u
#define BSR_10BASE_T_FULL 0x1000u
#define BSR_10BASE_T_HALF 0x0800u

static void decode_bsr(uint16_t bsr, lan8742_link_t *link)
{
    link->link_up = (bsr & BSR_LINK_STATUS) != 0u;
    link->speed_100 = (bsr & (BSR_100BASE_TX_FULL | BSR_100BASE_TX_HALF)) != 0u;
    link->full_duplex =
        (bsr & (BSR_100BASE_TX_FULL | BSR_10BASE_T_FULL)) != 0u;
}

bool lan8742_read_id(uint16_t *id1, uint16_t *id2)
{
    if ((id1 == NULL) || (id2 == NULL)) {
        return false;
    }

    return mdio_read(LAN8742_PHY_ADDRESS, PHY_REG_ID1, id1) &&
           mdio_read(LAN8742_PHY_ADDRESS, PHY_REG_ID2, id2);
}

bool lan8742_init(lan8742_link_t *link)
{
    if (link == NULL) {
        return false;
    }

    link->link_up = false;
    link->speed_100 = false;
    link->full_duplex = false;

    /* Soft reset. */
    if (!mdio_write(LAN8742_PHY_ADDRESS, PHY_REG_BCR, BCR_RESET)) {
        return false;
    }
    /* The LAN8742A software reset can take up to 0.5 s. */
    board_delay_ms(500u);

    /* Enable and restart auto-negotiation. */
    if (!mdio_write(LAN8742_PHY_ADDRESS, PHY_REG_BCR,
                    BCR_AUTONEG_ENABLE | BCR_AUTONEG_RESTART)) {
        return false;
    }

    /* Auto-negotiation and link can take a few seconds. */
    for (uint32_t i = 0u; i < 30u; ++i) {
        uint16_t bsr = 0u;
        if (!mdio_read(LAN8742_PHY_ADDRESS, PHY_REG_BSR, &bsr)) {
            return false;
        }

        if ((bsr & BSR_LINK_STATUS) != 0u) {
            /* Wait briefly for AN complete, then use whatever link state
             * the PHY reports. */
            for (uint32_t j = 0u; j < 10u; ++j) {
                if ((bsr & BSR_AN_COMPLETE) != 0u) {
                    break;
                }
                board_delay_ms(10u);
                if (!mdio_read(LAN8742_PHY_ADDRESS, PHY_REG_BSR, &bsr)) {
                    return false;
                }
            }
            decode_bsr(bsr, link);
            return true;
        }

        board_delay_ms(100u);
    }

    return false;
}

bool lan8742_poll(lan8742_link_t *link)
{
    uint16_t bsr = 0u;

    if ((link == NULL) ||
        !mdio_read(LAN8742_PHY_ADDRESS, PHY_REG_BSR, &bsr)) {
        return false;
    }

    decode_bsr(bsr, link);
    return true;
}
