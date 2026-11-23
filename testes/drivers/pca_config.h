#ifndef PCA_CONFIG_H
#define PCA_CONFIG_H

/* Enderecos 7 bits dos PCA9535 (A2,A1,A0 = 0x03 e 0x01). */
#define EXP0_ADDR 0x23
#define EXP1_ADDR 0x21

#define PCA_TIMEOUT_MS 100

/* Registradores do PCA9535. */
#define PCA_REG_INPUT  0x00
#define PCA_REG_OUTPUT 0x02
#define PCA_REG_CONFIG 0x06

/* Direcao das portas: bit = PCA_OUTPUT (0) ou PCA_INPUT (1). */
#define EXP0_P0_DIR 0b11000000 /* LD4 P0.0..2, LD3 P0.3..5 */
#define EXP0_P1_DIR 0b11111111 /* botao BTN_1 em P1.0 */
#define EXP1_P0_DIR 0b11111111 /* botao BTN_2 em P0.7 */
#define EXP1_P1_DIR 0b00000011 /* LD7 P1.2..4, LD8 P1.5..7 */

/*
 * Valores ja prontos para a porta: o binario e' o byte escrito no PCA.
 * LEDs sao ativo-baixo (bit 0 acende, bit 1 apaga). Cada constante
 * mantem os LEDs vizinhos da mesma porta apagados.
 */

/* LD4: R=P0.2, G=P0.0, B=P0.1 */
#define LD4_OFF     0b11111111
#define LD4_GREEN   0b11111110
#define LD4_BLUE    0b11111101
#define LD4_YELLOW  0b11111010
#define LD4_RED     0b11111011
#define LD4_MAGENTA 0b11111001
#define LD4_CYAN    0b11111100
#define LD4_WHITE   0b11111000

/* LD3: R=P0.5, G=P0.3, B=P0.4 */
#define LD3_OFF     0b11111111
#define LD3_GREEN   0b11110111
#define LD3_BLUE    0b11101111
#define LD3_YELLOW  0b11010111
#define LD3_RED     0b11011111
#define LD3_MAGENTA 0b11001111
#define LD3_CYAN    0b11100111
#define LD3_WHITE   0b11000111

/* LD7: R=P1.4, G=P1.3, B=P1.2 */
#define LD7_OFF     0b11111111
#define LD7_GREEN   0b11110111
#define LD7_BLUE    0b11111011
#define LD7_YELLOW  0b11100111
#define LD7_RED     0b11101111
#define LD7_MAGENTA 0b11101011
#define LD7_CYAN    0b11110011
#define LD7_WHITE   0b11100011

/* LD8: R=P1.7, G=P1.5, B=P1.6 */
#define LD8_OFF     0b11111111
#define LD8_GREEN   0b11011111
#define LD8_BLUE    0b10111111
#define LD8_YELLOW  0b01011111
#define LD8_RED     0b01111111
#define LD8_MAGENTA 0b00111111
#define LD8_CYAN    0b10011111
#define LD8_WHITE   0b00011111

/* Botao pressionado = nivel 1. */
#define BTN_1_PRESSED 0b00000001 /* EXP0 P1.0 */
#define BTN_2_PRESSED 0b10000000 /* EXP1 P0.7 */

#endif /* PCA_CONFIG_H */
