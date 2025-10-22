//
// Created by pkj on 2025/10/17.
//

#ifndef APP_DEVICE_H
#define APP_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

struct sensor_data {
    const char *unit;
    float value;
};

enum led_id {
    LED_NET = 0,
    LED_USER
};

enum led_state {
    LED_OFF = 0,
    LED_ON
};

bool device_ready();

/**
 *
 * @param data_ptr in
 * @return
 */
int device_read_sensor(struct sensor_data *data_ptr);

int device_write_led(enum led_id, enum led_state);

void device_handle_command(const char *command);

#endif //APP_DEVICE_H
