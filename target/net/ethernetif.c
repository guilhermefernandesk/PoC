#include "net/ethernetif.h"

#include "eth/eth.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"

#include <string.h>

#if LWIP_IPV6
#include "lwip/ethip6.h"
#endif

#define NETIF_MAC_ADDRESS \
    { 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u }

static uint8_t g_tx_frame[ETH_FRAME_MAX];
static uint8_t g_rx_frame[ETH_FRAME_MAX];
static bool g_link_up;

static void link_poll_callback(void *arg)
{
    struct netif *netif = (struct netif *)arg;
    eth_status_t status;

    if (!eth_link_update(&status)) {
        return;
    }

    if (status.link_up == g_link_up) {
        return;
    }

    g_link_up = status.link_up;

    if (g_link_up) {
        netif_set_link_up(netif);
    } else {
        netif_set_link_down(netif);
    }
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;

    if ((p == NULL) || (p->tot_len == 0u) || (p->tot_len > ETH_FRAME_MAX)) {
        return ERR_BUF;
    }

    const u16_t copied =
        pbuf_copy_partial(p, g_tx_frame, p->tot_len, 0u);
    if (copied != p->tot_len) {
        return ERR_BUF;
    }

    return eth_tx(g_tx_frame, p->tot_len) ? ERR_OK : ERR_IF;
}

err_t ethernetif_init(struct netif *netif)
{
    static const uint8_t mac[6] = NETIF_MAC_ADDRESS;

    if (netif == NULL) {
        return ERR_ARG;
    }

    netif->name[0] = 'e';
    netif->name[1] = '0';
    netif->mtu = 1500u;
    netif->hwaddr_len = 6u;
    memcpy(netif->hwaddr, mac, sizeof(mac));
    netif->state = NULL;

    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif

    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
                   NETIF_FLAG_ETHERNET;

    if (!eth_init(netif->hwaddr)) {
        return ERR_IF;
    }

    eth_status_t status;
    if (eth_link_update(&status) && status.link_up) {
        g_link_up = true;
        netif->flags |= NETIF_FLAG_LINK_UP;
    }

    return ERR_OK;
}

void ethernetif_input(struct netif *netif)
{
    if (netif == NULL) {
        return;
    }

    const uint16_t length = eth_rx(g_rx_frame, sizeof(g_rx_frame));
    if (length == 0u) {
        return;
    }

    struct pbuf *p = pbuf_alloc(PBUF_RAW, length, PBUF_RAM);
    if (p == NULL) {
        return;
    }

    if (pbuf_take(p, g_rx_frame, length) != ERR_OK) {
        pbuf_free(p);
        return;
    }

    if (netif->input(p, netif) != ERR_OK) {
        pbuf_free(p);
    }
}

void ethernetif_link_update(struct netif *netif)
{
    if (netif == NULL) {
        return;
    }

    /*
     * Poll the PHY and update MAC speed/duplex from the tcpip thread. This
     * serializes MAC register changes with low_level_output(), which is also
     * called from the tcpip thread.
     */
    if (tcpip_callback(link_poll_callback, netif) != ERR_OK) {
        link_poll_callback(netif);
    }
}
