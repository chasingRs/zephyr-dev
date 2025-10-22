/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#if !DT_NODE_EXISTS(DT_NODELABEL(load_switch))
#error "Power switch node not enabled in dts"
#endif

static const struct gpio_dt_spec load_switch = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(load_switch), gpios, {0});

int main(void) {
    int ret;
    if (!gpio_is_ready_dt(&load_switch)) {
        printk("The load switch pin GPIO port is not ready.\n");
        return -1;
    }
    ret = gpio_pin_configure_dt(&load_switch, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("The load switch pin GPIO pin is not configured.\n");
        return -1;
    }
    k_sleep(K_SECONDS(1));
    ret = gpio_pin_set_dt(&load_switch, 1);
    if (ret < 0) {
        printk("The load switch pin pin is not configured.\n");
    }
    return 0;
}
