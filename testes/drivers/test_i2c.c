#include "pca.h"
#include "pca_config.h"

void test_i2c_init(void) {
    pca_init(&io_exp, &hi2c4, EXP0_ADDR);
    pca_init(&io_exp2, &hi2c4, EXP1_ADDR);

    pca_dir(&io_exp, PCA_PORT0, EXP0_P0_DIR);
    pca_dir(&io_exp, PCA_PORT1, EXP0_P1_DIR);
    pca_dir(&io_exp2, PCA_PORT0, EXP1_P0_DIR);
    pca_dir(&io_exp2, PCA_PORT1, EXP1_P1_DIR);

    pca_out(&io_exp, PCA_PORT0, LD4_OFF);
    pca_out(&io_exp2, PCA_PORT1, LD7_OFF);
}

void test_i2c_blink(void) {
    pca_out(&io_exp, PCA_PORT0, LD3_GREEN);
    pca_out(&io_exp2, PCA_PORT1, LD8_OFF);

    if (pca_in(&io_exp, PCA_PORT1) & BTN_1_PRESSED) {
        pca_out(&io_exp, PCA_PORT0, LD4_GREEN);
    } else {
        pca_out(&io_exp, PCA_PORT0, LD4_OFF);
    }

    if (pca_in(&io_exp2, PCA_PORT0) & BTN_2_PRESSED) {
        pca_out(&io_exp2, PCA_PORT1, LD7_RED);
    } else {
        pca_out(&io_exp2, PCA_PORT1, LD7_OFF);
    }
}
