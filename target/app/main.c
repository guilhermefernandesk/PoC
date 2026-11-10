#include "FreeRTOS.h"
#include "task.h"

#include "stm32h7xx.h"

#include "app_regs.h"
#include "board/clock.h"
#include "board/delay.h"
#include "board/gpio.h"
#include "board/mpu.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "mb_server.h"
#include "mb_transport_lwip.h"
#include "net/ethernetif.h"

static struct netif g_netif;

static void netif_task(void *argument)
{
    (void)argument;

    uint32_t ticks = 0u;

    for (;;) {
        ethernetif_input(&g_netif);

        /* The PHY does not change every millisecond; poll it twice a second. */
        if ((ticks++ % 250u) == 0u) {
            ethernetif_link_update(&g_netif);
        }

        vTaskDelay(pdMS_TO_TICKS(2u));
    }
}

static void modbus_task(void *argument)
{
    (void)argument;

    const mb_server_cfg_t cfg = {
        .transport = mb_transport_lwip_get(),
        .map = app_regs_map(),
        .port = 502u,
        .accept_timeout_ms = 1000u,
        .recv_timeout_ms = 30000u,
    };

    for (;;) {
        (void)mb_server_run(&cfg);
        vTaskDelay(pdMS_TO_TICKS(1000u));
    }
}

int main(void)
{
    if (!board_clock_init()) {
        for (;;) {
            __NOP();
        }
    }

    board_delay_init();
    board_mpu_init();

    /* Enable CP10/CP11 so the FreeRTOS FPU context code is safe. */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    /* D2 ETH memory is non-cacheable; the rest uses the default map. */
    SCB_EnableICache();
    SCB_EnableDCache();

    board_gpio_init();

    app_regs_init();

    /* tcpip_init() also runs lwip_init() and starts the tcpip thread. */
    tcpip_init(NULL, NULL);

    ip4_addr_t ip;
    ip4_addr_t netmask;
    ip4_addr_t gateway;

    IP4_ADDR(&ip, 192, 168, 1, 10);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 0, 0, 0, 0);

    if (netif_add(&g_netif, &ip, &netmask, &gateway, NULL,
                  ethernetif_init, tcpip_input) == NULL) {
        for (;;) {
            __NOP();
        }
    }

    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    if (xTaskCreate(netif_task, "net", 1024u, NULL, 4u, NULL) != pdPASS) {
        for (;;) {
            __NOP();
        }
    }

    if (xTaskCreate(modbus_task, "modbus", 2048u, NULL, 3u, NULL) != pdPASS) {
        for (;;) {
            __NOP();
        }
    }

    vTaskStartScheduler();

    for (;;) {
        __NOP();
    }
}
