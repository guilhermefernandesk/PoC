#ifndef GPIO_PORT_H
#define GPIO_PORT_H

#include <stdint.h>

#define LED_PIN 5u

typedef struct {
    void (*write)(uint16_t pin, int value);
    int  (*read)(uint16_t pin);
} GpioPort;

void gpio_port_write(uint16_t pin, int value);
int  gpio_port_read(uint16_t pin);

#endif
