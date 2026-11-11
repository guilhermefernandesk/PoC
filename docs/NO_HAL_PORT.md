# No-HAL target port notes

This document records the register-level decisions used by the STM32H743
target files. It is intended for the later production migration and for
debugging when the generated CubeMX project is replaced by a no-HAL build.

## Initialization order

`target/app/main.c` uses this order:

1. `board_clock_init()`
2. `board_delay_init()`
3. `board_mpu_init()`
4. enable CP10/CP11 in `SCB->CPACR`
5. `SCB_EnableICache()` / `SCB_EnableDCache()`
6. `board_gpio_init()`
7. `app_regs_init()`
8. `tcpip_init(NULL, NULL)`
9. `netif_add(..., ethernetif_init, tcpip_input)`
10. create the netif polling task and the Modbus task
11. `vTaskStartScheduler()`

The clock must be running before the DWT delay is used. The MPU must be
programmed before D-cache is enabled so the ETH DMA window is already
non-cacheable when the cache comes up. `SCB->CPACR` is set explicitly so the
FreeRTOS FPU context code can use CP10/CP11.

## Clock

`target/board/clock.c`:

- HSI 64 MHz is enabled and checked with `HSIRDY`.
- LDO is enabled and VOS1 is selected (`PWR->D3CR.VOS = 0b11`), then
  `VOSRDY` is checked.
- Flash is programmed to 4 wait states and `WRHIGHFREQ = 0b11` before the
  frequency increases.
- PLL1 uses HSI, `DIVM1 = 32`, `N = 400`, `P = 2`, `Q = 2`, `R = 2`.
  The PLL input is 2 MHz, the wide-band VCO is 800 MHz, and SYSCLK is
  400 MHz.
- D1CPRE = /1, HPRE = /2, APB3/APB1/APB2/APB4 = /2.
- The ETH MAC/TX/RX clocks, D2 SRAM1 clock, GPIO A/B/C/G clocks and SYSCFG
  clock are enabled after the switch.

If the board is changed to HSE or a different SYSCLK, update all of:
`board/clock.c`, `FreeRTOSConfig.h` (`configCPU_CLOCK_HZ`),
`target/board/delay.c` (constant from `clock.h`), and the MDIO divider if
HCLK leaves the 150–250 MHz range.

## RMII and GPIO

`target/board/gpio.c` selects RMII through `SYSCFG->PMCR.EPIS_SEL` and
configures the nine RMII pins as AF11, push-pull, very-high speed. PA2
(MDIO) also enables the internal pull-up; the board has an external pull-up
as well.

## MPU and cache

`target/board/mpu.c`:

- Region 0: base `0x30000000`, size 32 KiB, AP full access, TEX/C/B/S = 0
  (normal, non-cacheable, non-shareable).
- `MPU_CTRL = ENABLE | PRIVDEFENA`. FreeRTOS tasks run privileged, so the
  default map remains usable for code, AXI SRAM, peripherals and stacks.
- D-cache may be enabled because ETH descriptors and buffers never enter the
  cache. No `SCB_CleanDCache_by_Addr` or `SCB_InvalidateDCache_by_Addr` is
  needed on the Ethernet path.

The driver uses fixed addresses rather than linker-placed arrays:

```
0x30000000  4 RX descriptors (16 B each, 64 B)
0x30000100  4 TX descriptors (16 B each, 64 B)
0x30000200  4 RX buffers      (1536 B each, 6144 B)
0x30001A00  4 TX buffers      (1536 B each, 6144 B)
0x30003200  end of used window
```

The generated linker script must not allocate `.data`, `.bss`, heap or stack
in `0x30000000..0x30007FFF`.

## Ethernet MAC/DMA

`target/eth/eth.c` uses the H7 normal descriptor format with a 16-byte
stride (`DMACCR.DSL = 0`). In the H7, the descriptor words are:

```
DESC0  buffer 1 address
DESC1  buffer 2 address / timestamp
DESC2  control (TX length) / status
DESC3  status/control
```

RX descriptors are armed with `OWN | BUF1V`; TX descriptors with
`OWN | FD | LD`. The driver uses 4 descriptors per direction and a
1536-byte buffer per descriptor.

Key MAC/DMA values:

- `MACCR`: 100 Mbit, full duplex, automatic pad/CRC strip; TE/RE enabled at
  start.
- `MACPFR.RA = 1`: receive all, LwIP filters at the IP layer.
- `MACECR = 0x00000618`: CRC checking enabled; H7 default giant-packet
  limit. Checksum offload is left to LwIP in software.
- `MTLTQOMR.TSF` and `MTLRQOMR.RSF`: store-and-forward.
- `DMASBMR.AAL | FB`: address-aligned, fixed burst.
- `DMACTCR` / `DMACRCR`: 32-beat PBL, RX buffer size 1536.
- `DMACTDRLR` / `DMACRDRLR`: ring length minus one.
- `DMACTDLAR` / `DMACRDLAR`: descriptor base addresses.
- `DMACTDTPR` / `DMACRDTPR`: tail pointers.

RX processing copies from the non-cacheable DMA buffer into an LwIP pbuf and
immediately re-arms the descriptor. TX processing waits for the descriptor
to be free, copies the frame into the non-cacheable TX buffer, writes the
length and `OWN | FD | LD`, advances the TX tail pointer, and waits for
completion.

The driver does not enable the ETH interrupt. `netif_task` polls `eth_rx`
every 2 ms. This is deliberate: it removes the ETH IRQ priority and
FreeRTOS-from-ISR interaction from the PoC failure surface. For production,
an interrupt-driven RX path can replace the poll without changing the
Modbus module.

## PHY

`target/eth/lan8742.c` uses Clause 22 MDIO:

- PHY address 0
- BCR soft reset (500 ms worst-case wait), then auto-negotiation
  enable/restart
- BSR link, AN complete, 100 Mbit and full-duplex bits
- expected ID1/ID2: `0x0007` / `0xC130`

If the PHY ID is not `0x0007C130`, check the RMII REF_CLK, PHY address
strapping, MDIO/MDC wiring and the `SYSCFG` RMII selection before changing
the driver.

## LwIP/FreeRTOS boundary

- The Modbus task drives the FreeModbus stack: `eMBTCPInit`,
  `eMBEnable`, then an `eMBPoll()` loop; all I/O happens inside
  `eMBPoll()` via `ports/lwip/porttcp.c`.
- `porttcp.c` uses netconn (`netconn_new`, `netconn_bind`,
  `netconn_listen`, `netconn_accept`, `netconn_recv_tcp_pbuf`,
  `netconn_write`).
- `LWIP_SO_RCVTIMEO=1` is required so `accept` and `recv` can return
  `ERR_TIMEOUT` to the port.
- `LWIP_FREERTOS_THREAD_STACKSIZE_IS_STACKWORDS=1` keeps
  `TCPIP_THREAD_STACKSIZE=2048` as 8 KiB of FreeRTOS stack.
- `INCLUDE_vTaskDelay=1` and `INCLUDE_vTaskSuspend=1` are required by the
  LwIP FreeRTOS port.
- `ethernetif` uses `tcpip_input` as the netif input function; received
  frames are queued to the tcpip thread.
- `sys_arch.c` must be the FreeRTOS LwIP port. It provides the mutexes,
  mailboxes and threads that LwIP needs with `NO_SYS=0`.
- The LwIP transport uses one static `mb_conn` object, so the target does not
  need a libc heap or `_sbrk` for Modbus connections.

The LwIP heap (`MEM_SIZE`) and FreeRTOS heap (`configTOTAL_HEAP_SIZE`) are
separate. The PoC uses 64 KiB for each. Increase the FreeRTOS heap if more
tasks or larger stacks are added; increase `MEM_SIZE` if TCP windows or pbuf
churn grow.

## Why the RS232/PLC path is not reused

The PoC assumes a clean Modbus TCP boundary. The legacy RS232 protocol
documentation and frames were not available, so no attempt is made to map
legacy fields into Modbus registers automatically. Once the PLC map is known,
only `app/app_regs.c` and the IP/port/unit settings need to change.
