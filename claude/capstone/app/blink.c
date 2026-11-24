#include "blink.h"

static const GpioPort *g_port;
static int g_error;
static int g_level;

void blink_init(const GpioPort *port) {
    g_port = port;
    g_error = 0;
    g_level = 0;
    if (g_port && g_port->write) {
        g_port->write(LED_PIN, g_level);
    }
}

void blink_enter_error(void) {
    g_error = 1;
}

void blink_toggle(void) {
    g_level = g_error ? 1 : (g_level ? 0 : 1);
    if (g_port && g_port->write) {
        g_port->write(LED_PIN, g_level);
    }
}

int blink_is_error(void) {
    return g_error;
}
