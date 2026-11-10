#include "board/mpu.h"

#include "stm32h7xx.h"

#define D2_ETH_BASE 0x30000000u

void board_mpu_init(void)
{
    MPU->CTRL = 0u;
    __DSB();
    __ISB();

    /*
     * 32 KiB = 2^15. SIZE field is log2(size) - 1 = 14.
     * AP = 0b011 (full access), TEX/C/B/S = 0 => normal, non-cacheable,
     * non-shareable memory. XN = 0 so the region can hold data only in
     * practice; execute-never is not required for the PoC.
     */
    MPU->RNR = 0u;
    MPU->RBAR = D2_ETH_BASE;
    MPU->RASR = (1u << 0) |          /* ENABLE */
                (0x3u << 24) |       /* AP full access */
                (14u << 1);          /* SIZE 32 KiB */

    /* Keep the default map for privileged accesses (FreeRTOS tasks). */
    MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
    __DSB();
    __ISB();
}
