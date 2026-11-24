#ifndef BLINK_H
#define BLINK_H

#include "gpio_port.h"

void blink_init(const GpioPort *port);
void blink_toggle(void);
void blink_enter_error(void);
int  blink_is_error(void);

#endif
