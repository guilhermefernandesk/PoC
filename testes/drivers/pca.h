#ifndef PCA_H
#define PCA_H

#include "stm32h7xx_hal.h"
#include "i2c.h"

typedef enum {
    PCA_PORT0 = 0,
    PCA_PORT1 = 1
} pca_port_t;

typedef enum {
    PCA_OUTPUT = 0,
    PCA_INPUT  = 1
} pca_dir_t;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t addr;    /* endereco ja deslocado para a HAL */
    uint8_t out[2];  /* ultimo valor escrito em cada porta */
} pca_t;

extern pca_t io_exp;
extern pca_t io_exp2;

void pca_init(pca_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr_7bit);
void pca_dir(pca_t *dev, pca_port_t port, uint8_t dir_mask);
void pca_out(pca_t *dev, pca_port_t port, uint8_t value);
uint8_t pca_in(pca_t *dev, pca_port_t port);

void scan_i2c(void);
void test_i2c_init(void);
void test_i2c_blink(void);

#endif /* PCA_H */
