#include "pca.h"

/* Diagnostico: varre o barramento e pare no breakpoint do __NOP(). */
void scan_i2c(void) {
    I2C_HandleTypeDef *barramentos[] = { &hi2c4 };

    for (uint8_t b = 0; b < sizeof(barramentos) / sizeof(barramentos[0]); b++) {
        for (uint16_t addr = 1; addr < 128; addr++) {
            if (HAL_I2C_IsDeviceReady(barramentos[b], (uint16_t)(addr << 1),
                                      2, 10) == HAL_OK) {
                __NOP();
            }
        }
    }
}
