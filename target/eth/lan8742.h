#ifndef LAN8742_H
#define LAN8742_H

#include <stdbool.h>
#include <stdint.h>

#define LAN8742_PHY_ADDRESS 0u
#define LAN8742_ID1         0x0007u
#define LAN8742_ID2         0xC130u

typedef struct {
    bool link_up;
    bool speed_100;
    bool full_duplex;
} lan8742_link_t;

/* Reset the PHY, start auto-negotiation and wait for a link. */
bool lan8742_init(lan8742_link_t *link);

/* Read the current link state without restarting auto-negotiation. */
bool lan8742_poll(lan8742_link_t *link);

bool lan8742_read_id(uint16_t *id1, uint16_t *id2);

#endif
