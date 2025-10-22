//
// Created by pkj on 2025/10/17.
//

#include "zephyr/drivers/led.h"
#include "zephyr/logging/log.h"
LOG_MODULE_REGISTER(app_device, CONFIG_APP_LOG_LEVEL);

#include "device.h"
#include "zephyr/device.h"
#include "zephyr/drivers/sensor.h"

#define SENSOR_UNIT "Celsius"
#define SENSOR_CHAN SENSOR_CHAN_AMBIENT_TEMP

struct device_cmd {
    const char *command;

    void (*handler)();
};

static const struct device *leds = DEVICE_DT_GET_OR_NULL(DT_INST(0,gpio_leds));
static const struct device *sensor = DEVICE_DT_GET_OR_NULL(DT_ALIAS(ambient_temp0));

static void led_user_on_handler(void) {
    device_write_led(LED_USER, 1);
}

static void led_user_off_handler(void) {
    device_write_led(LED_USER, 0);
}

static const struct device_cmd device_cmds[] = {
    {.command = "led_on", .handler = led_user_on_handler},
    {.command = "led_off", .handler = led_user_off_handler},
};

static const size_t num_device_cmds = ARRAY_SIZE(device_cmds);

bool device_ready() {
    bool ready = true;

    if (leds != NULL && device_is_ready(leds)) {
        LOG_INF("Device is ready: %s\n", leds->name);
    } else {
        ready = false;
    }

    if (sensor != NULL && device_is_ready(sensor)) {
        LOG_INF("Device is ready: %s\n", sensor->name);
    } else {
        ready = false;
    }

    return ready;
}

int device_read_sensor(struct sensor_data *data_ptr) {
    int ret = 0;
    struct sensor_value sensor_val;

    data_ptr->unit = SENSOR_UNIT;

    if (sensor == NULL) {
        data_ptr->value = 0;
        return -1;
    }

    ret = sensor_sample_fetch(sensor);
    if (ret < 0) {
        LOG_ERR("Fail to read sensor data");
        return ret;
    }

    ret = sensor_channel_get(sensor, SENSOR_CHAN, &sensor_val);
    if (ret < 0) {
        LOG_ERR("Fail to read sensor channel");
        return ret;
    }

    data_ptr->value = sensor_value_to_float(&sensor_val);

    LOG_INF("Device read sensor value: %06f", data_ptr->value);

    return ret;
}

int device_write_led(const enum led_id id, const enum led_state state) {
    int ret = 0;
    if (state) {
        ret = led_on(leds, id);
    } else {
        ret = led_off(leds, id);
    }
    if (ret != 0) {
        LOG_ERR("Fail to write led");
        return ret;
    }
    return ret;
}

void device_handle_command(const char *command) {
    for (int i = 0; i < num_device_cmds; i++) {
        if (strcmp(device_cmds[i].command, command) == 0) {
            LOG_INF("Device command %s", command);
            return device_cmds[i].handler();
        }
    }
    LOG_ERR("Unknown device command %s", command);
}
