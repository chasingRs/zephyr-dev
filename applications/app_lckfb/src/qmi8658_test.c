#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_main, CONFIG_APP_LOG_LEVEL);

int main(void) {
    const struct device *const dev = DEVICE_DT_GET_ONE(qst_qmi8658);
    struct sensor_value acc[3], gyr[3];
    struct sensor_value full_scale, sampling_freq;

    if (!device_is_ready(dev)) {
        LOG_ERR("Device %s is not ready\n", dev->name);
        return 0;
    }

    LOG_INF("Device %p name is %s\n", dev, dev->name);

    full_scale.val1 = 40; /* ms/s^2 */
    full_scale.val2 = 0;
    sampling_freq.val1 = 100; /* Hz. Performance mode */
    sampling_freq.val2 = 0;

    sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_FULL_SCALE,
                    &full_scale);
    /* Set sampling frequency last as this also sets the appropriate
     * power mode. If already sampling, change to 0.0Hz before changing
     * other attributes
     */
    sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
                    SENSOR_ATTR_SAMPLING_FREQUENCY,
                    &sampling_freq);

    /* Setting scale in degrees/s to match the sensor scale */
    full_scale.val1 = 1024; /* dps */
    full_scale.val2 = 0;
    sampling_freq.val1 = 100; /* Hz. Performance mode */
    sampling_freq.val2 = 0;

    sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_FULL_SCALE,
                    &full_scale);
    /* Set sampling frequency last as this also sets the appropriate
     * power mode. If already sampling, change sampling frequency to
     * 0.0Hz before changing other attributes
     */
    sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
                    SENSOR_ATTR_SAMPLING_FREQUENCY,
                    &sampling_freq);

    while (1) {
        /* 10ms period, 100Hz Sampling frequency */
        k_sleep(K_MSEC(50));

        sensor_sample_fetch(dev);

        sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, acc);
        sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyr);

        printf("%f,%f,%f,"
               "%f,%f,%f\n",
               sensor_value_to_double(&acc[0]),
               sensor_value_to_double(&acc[1]),
               sensor_value_to_double(&acc[2]),
               sensor_value_to_double(&gyr[0]),
               sensor_value_to_double(&gyr[1]),
               sensor_value_to_double(&gyr[2]));
    }
    return 0;
}
