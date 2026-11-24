#include "unity.h"
#include "blink.h"
#include "mock_gpio_port.h"

void setUp(void) {
    blink_init(&(GpioPort){
        .write = gpio_port_write_Stub,
        .read  = gpio_port_read_Stub,
    });
}

void tearDown(void) {}

void test_blink_starts_idle_with_led_off(void) {
    TEST_ASSERT_FALSE(blink_is_error());
}

void test_blink_toggles_in_idle(void) {
    gpio_port_write_Expect(LED_PIN, 1);
    blink_toggle();

    gpio_port_write_Expect(LED_PIN, 0);
    blink_toggle();
}

void test_blink_error_keeps_led_on(void) {
    blink_enter_error();

    gpio_port_write_Expect(LED_PIN, 1);
    blink_toggle();

    gpio_port_write_Expect(LED_PIN, 1);
    blink_toggle();

    TEST_ASSERT_TRUE(blink_is_error());
}
