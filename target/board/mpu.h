#ifndef BOARD_MPU_H
#define BOARD_MPU_H

/*
 * D2 SRAM 0x30000000..0x30007FFF is used for ETH DMA descriptors and
 * buffers. It is mapped non-cacheable so the copy-based driver needs no
 * cache maintenance on the data path.
 */
void board_mpu_init(void);

#endif
