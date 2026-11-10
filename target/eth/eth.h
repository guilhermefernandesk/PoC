#ifndef ETH_H
#define ETH_H

#include <stdbool.h>
#include <stdint.h>

#define ETH_FRAME_MAX 1536u

#define ETH_D2_BASE 0x30000000u
#define ETH_RX_DESC_COUNT 4u
#define ETH_TX_DESC_COUNT 4u

typedef struct {
    bool link_up;
    bool speed_100;
    bool full_duplex;
} eth_status_t;

/*
 * Configure MAC/DMA/PHY and start the copy-based Ethernet driver.
 *
 * The caller must already have run:
 *   board_clock_init();
 *   board_delay_init();
 *   board_mpu_init();
 *   board_gpio_init();
 */
bool eth_init(const uint8_t mac[6]);

/* Poll the PHY and update MAC speed/duplex when the link changes. */
bool eth_link_update(eth_status_t *status);

/* Copy one received Ethernet frame. Returns 0 when no frame is ready. */
uint16_t eth_rx(uint8_t *buffer, uint16_t max_length);

/* Copy and transmit one Ethernet frame. Returns false on timeout/error. */
bool eth_tx(const uint8_t *buffer, uint16_t length);

#endif
