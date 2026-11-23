#include "pca.h"
#include "pca_config.h"

pca_t io_exp;
pca_t io_exp2;

void pca_init(pca_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr_7bit) {
    dev->hi2c = hi2c;
    dev->addr = (uint8_t)(addr_7bit << 1);
    dev->out[0] = 0xFF;
    dev->out[1] = 0xFF;
}

void pca_dir(pca_t *dev, pca_port_t port, uint8_t dir_mask) {
    uint8_t reg = (uint8_t)(PCA_REG_CONFIG + port);
    HAL_I2C_Mem_Write(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT,
                      &dir_mask, 1, PCA_TIMEOUT_MS);
}

void pca_out(pca_t *dev, pca_port_t port, uint8_t value) {
    uint8_t reg = (uint8_t)(PCA_REG_OUTPUT + port);
    dev->out[port] = value;
    HAL_I2C_Mem_Write(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT,
                      &value, 1, PCA_TIMEOUT_MS);
}

uint8_t pca_in(pca_t *dev, pca_port_t port) {
    uint8_t reg = (uint8_t)(PCA_REG_INPUT + port);
    uint8_t value = 0;
    HAL_I2C_Mem_Read(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT,
                     &value, 1, PCA_TIMEOUT_MS);
    return value;
}
