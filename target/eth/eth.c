#include "eth/eth.h"

#include "eth/lan8742.h"
#include "eth/mdio.h"
#include "stm32h7xx.h"

#include <string.h>

/*
 * H7 normal DMA descriptor (4 words, 16 bytes). DMACCR.DSL is kept at 0,
 * so the hardware walks the ring with a 16-byte stride.
 *
 *   DESC0 = buffer 1 address
 *   DESC1 = buffer 2 address / timestamp
 *   DESC2 = control (TX length) / status
 *   DESC3 = status/control
 */
typedef struct {
    volatile uint32_t desc0;
    volatile uint32_t desc1;
    volatile uint32_t desc2;
    volatile uint32_t desc3;
} eth_desc_t;

#define RX_DESC_ADDR (ETH_D2_BASE)
#define TX_DESC_ADDR (RX_DESC_ADDR + 0x100u)
#define RX_BUFFER_ADDR (TX_DESC_ADDR + 0x100u)
#define TX_BUFFER_ADDR \
    (RX_BUFFER_ADDR + (ETH_RX_DESC_COUNT * ETH_FRAME_MAX))

#define RX_OWN 0x80000000u
#define RX_BUF1V 0x01000000u
#define RX_WB_OWN 0x80000000u
#define RX_WB_FD 0x20000000u
#define RX_WB_LD 0x10000000u
#define RX_WB_ES 0x00008000u
#define RX_WB_LENGTH 0x00007FFFu

#define TX_OWN 0x80000000u
#define TX_FD 0x20000000u
#define TX_LD 0x10000000u
#define TX_WB_OWN 0x80000000u
#define TX_WB_ES 0x00008000u

static eth_desc_t *const g_rx_desc = (eth_desc_t *)RX_DESC_ADDR;
static eth_desc_t *const g_tx_desc = (eth_desc_t *)TX_DESC_ADDR;

static uint32_t g_rx_index;
static uint32_t g_tx_index;
static bool g_started;

static bool g_last_link_up;
static bool g_last_speed_100;
static bool g_last_full_duplex;

static void apply_link_speed(const lan8742_link_t *link, bool mac_running)
{
    uint32_t maccr = ETH->MACCR;
    const uint32_t enable = maccr & (ETH_MACCR_TE | ETH_MACCR_RE);

    if (mac_running) {
        /* FES/DM should not be changed while the MAC is enabled. */
        ETH->MACCR = maccr & ~(ETH_MACCR_TE | ETH_MACCR_RE);
    }

    maccr = ETH->MACCR;
    maccr &= ~(ETH_MACCR_FES | ETH_MACCR_DM);

    if (link->link_up) {
        if (link->speed_100) {
            maccr |= ETH_MACCR_FES;
        }
        if (link->full_duplex) {
            maccr |= ETH_MACCR_DM;
        }
    }

    ETH->MACCR = maccr | enable;
}

static bool dma_reset(void)
{
    ETH->DMAMR |= ETH_DMAMR_SWR;

    for (uint32_t i = 0u; i < 1000000u; ++i) {
        if ((ETH->DMAMR & ETH_DMAMR_SWR) == 0u) {
            return true;
        }
    }

    return false;
}

static void build_descriptors(void)
{
    for (uint32_t i = 0u; i < ETH_RX_DESC_COUNT; ++i) {
        g_rx_desc[i].desc0 = RX_BUFFER_ADDR + (i * ETH_FRAME_MAX);
        g_rx_desc[i].desc1 = 0u;
        g_rx_desc[i].desc2 = 0u;
        g_rx_desc[i].desc3 = RX_OWN | RX_BUF1V;
    }

    for (uint32_t i = 0u; i < ETH_TX_DESC_COUNT; ++i) {
        g_tx_desc[i].desc0 = TX_BUFFER_ADDR + (i * ETH_FRAME_MAX);
        g_tx_desc[i].desc1 = 0u;
        g_tx_desc[i].desc2 = 0u;
        g_tx_desc[i].desc3 = 0u;
    }

    g_rx_index = 0u;
    g_tx_index = 0u;
}

static void start_dma(void)
{
    /* Enable the MAC transmitter and receiver. */
    ETH->MACCR |= ETH_MACCR_TE | ETH_MACCR_RE;

    /* Flush the transmit FIFO before starting DMA. */
    ETH->MTLTQOMR |= ETH_MTLTQOMR_FTQ;

    ETH->DMACTCR |= ETH_DMACTCR_ST;
    ETH->DMACRCR |= ETH_DMACRCR_SR;

    ETH->DMACSR = ETH_DMACSR_TPS | ETH_DMACSR_RPS;
}

bool eth_init(const uint8_t mac[6])
{
    if (mac == NULL) {
        return false;
    }

    g_started = false;

    /* RMII is selected in board_gpio_init(). MDIO is needed by the PHY. */
    mdio_init();

    if (!dma_reset()) {
        return false;
    }

    /* MAC: 100 Mbps full duplex is the safe default before link is known. */
    ETH->MACCR = ETH_MACCR_FES | ETH_MACCR_DM | ETH_MACCR_ACS;

    /* Receive all frames; LwIP filters at the IP layer for the PoC. */
    ETH->MACPFR = ETH_MACPFR_RA;

    /* CRC checking enabled; 0x618 is the H7 default giant-packet limit. */
    ETH->MACECR = 0x00000618u;
    ETH->MACTFCR = 0u;
    ETH->MACRFCR = 0u;

    /* Store-and-forward in both MTL queues. */
    ETH->MTLTQOMR = ETH_MTLTQOMR_TSF;
    ETH->MTLRQOMR = ETH_MTLRQOMR_RSF;

    /* DMA system bus and channel configuration. */
    ETH->DMAMR = 0u;
    ETH->DMASBMR = ETH_DMASBMR_AAL | ETH_DMASBMR_FB;
    ETH->DMACCR = 0u; /* descriptor skip length 0 => 16-byte descriptors */
    ETH->DMACTCR = ETH_DMACTCR_TPBL_32PBL;
    ETH->DMACRCR = ETH_DMACRCR_RPBL_32PBL |
                   ((uint32_t)ETH_FRAME_MAX << ETH_DMACRCR_RBSZ_Pos);

    build_descriptors();

    ETH->DMACTDRLR = ETH_TX_DESC_COUNT - 1u;
    ETH->DMACRDRLR = ETH_RX_DESC_COUNT - 1u;
    ETH->DMACTDLAR = (uint32_t)g_tx_desc;
    ETH->DMACRDLAR = (uint32_t)g_rx_desc;
    ETH->DMACTDTPR = (uint32_t)g_tx_desc;
    ETH->DMACRDTPR = (uint32_t)&g_rx_desc[ETH_RX_DESC_COUNT - 1u];

    /* MAC address 0. */
    ETH->MACA0HR = ((uint32_t)mac[5] << 8) | (uint32_t)mac[4] |
                   ETH_MACA0HR_AE;
    ETH->MACA0LR = ((uint32_t)mac[3] << 24) | ((uint32_t)mac[2] << 16) |
                   ((uint32_t)mac[1] << 8) | (uint32_t)mac[0];

    __DSB();

    g_last_link_up = false;
    g_last_speed_100 = false;
    g_last_full_duplex = false;

    lan8742_link_t link;
    if (lan8742_init(&link)) {
        apply_link_speed(&link, false);
        g_last_link_up = link.link_up;
        g_last_speed_100 = link.speed_100;
        g_last_full_duplex = link.full_duplex;
    }

    start_dma();
    g_started = true;

    return true;
}

bool eth_link_update(eth_status_t *status)
{
    lan8742_link_t link;

    if ((status == NULL) || !g_started) {
        return false;
    }

    if (!lan8742_poll(&link)) {
        return false;
    }

    if ((link.link_up != g_last_link_up) ||
        (link.speed_100 != g_last_speed_100) ||
        (link.full_duplex != g_last_full_duplex)) {
        apply_link_speed(&link, true);
        g_last_link_up = link.link_up;
        g_last_speed_100 = link.speed_100;
        g_last_full_duplex = link.full_duplex;
    }

    status->link_up = link.link_up;
    status->speed_100 = link.speed_100;
    status->full_duplex = link.full_duplex;

    return true;
}

static uint32_t rx_buffer_address(uint32_t index)
{
    return RX_BUFFER_ADDR + (index * ETH_FRAME_MAX);
}

static void rearm_rx_descriptor(uint32_t index)
{
    eth_desc_t *desc = &g_rx_desc[index];

    /*
     * RDES0 is overwritten with VLAN tag information on RX write-back.
     * Restore the buffer address before handing the descriptor back to DMA.
     */
    desc->desc0 = rx_buffer_address(index);
    desc->desc1 = 0u;
    desc->desc2 = 0u;

    __DMB();
    desc->desc3 = RX_OWN | RX_BUF1V;
    __DMB();

    ETH->DMACRDTPR = (uint32_t)desc;
}

uint16_t eth_rx(uint8_t *buffer, uint16_t max_length)
{
    if ((buffer == NULL) || (max_length == 0u) || !g_started) {
        return 0u;
    }

    const uint32_t index = g_rx_index;
    eth_desc_t *desc = &g_rx_desc[index];
    __DMB();
    const uint32_t status = desc->desc3;

    if ((status & RX_WB_OWN) != 0u) {
        return 0u;
    }

    if (((status & RX_WB_FD) == 0u) ||
        ((status & RX_WB_LD) == 0u) ||
        ((status & RX_WB_ES) != 0u)) {
        rearm_rx_descriptor(index);
        g_rx_index = (index + 1u) % ETH_RX_DESC_COUNT;
        return 0u;
    }

    const uint16_t length = (uint16_t)(status & RX_WB_LENGTH);

    if ((length == 0u) || (length > max_length)) {
        rearm_rx_descriptor(index);
        g_rx_index = (index + 1u) % ETH_RX_DESC_COUNT;
        return 0u;
    }

    memcpy(buffer, (const void *)(uintptr_t)rx_buffer_address(index), length);

    rearm_rx_descriptor(index);
    g_rx_index = (index + 1u) % ETH_RX_DESC_COUNT;

    return length;
}

bool eth_tx(const uint8_t *buffer, uint16_t length)
{
    if ((buffer == NULL) || (length == 0u) || (length > ETH_FRAME_MAX) ||
        !g_started) {
        return false;
    }

    const uint32_t index = g_tx_index;
    eth_desc_t *desc = &g_tx_desc[index];
    const uint32_t buffer_address = TX_BUFFER_ADDR + (index * ETH_FRAME_MAX);

    /* Wait for the descriptor to be released by the DMA. */
    for (uint32_t i = 0u; i < 1000000u; ++i) {
        if ((desc->desc3 & TX_WB_OWN) == 0u) {
            break;
        }
        __NOP();
    }

    if ((desc->desc3 & TX_WB_OWN) != 0u) {
        return false;
    }

    memcpy((void *)(uintptr_t)buffer_address, buffer, length);

    /* TDES0/TDES1 are overwritten with timestamp data on TX write-back. */
    desc->desc0 = buffer_address;
    desc->desc1 = 0u;
    desc->desc2 = (uint32_t)length & 0x3FFFu;

    __DMB();
    desc->desc3 = TX_OWN | TX_FD | TX_LD;
    __DSB();

    g_tx_index = (index + 1u) % ETH_TX_DESC_COUNT;
    ETH->DMACTDTPR = (uint32_t)&g_tx_desc[g_tx_index];

    /* Wait for completion so the caller can reuse its buffer. */
    for (uint32_t i = 0u; i < 1000000u; ++i) {
        if ((desc->desc3 & TX_WB_OWN) == 0u) {
            return (desc->desc3 & TX_WB_ES) == 0u;
        }
        __NOP();
    }

    return false;
}
