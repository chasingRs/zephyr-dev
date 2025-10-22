// TODO: add SPI support
// TODO: add interrupt support
// TODO: low pass filter support
// TODO: temperature sensor support

#ifndef ZEPHYR_DRIVERS_SENSOR_QMI8658_QMI8658_H_
#define ZEPHYR_DRIVERS_SENSOR_QMI8658_QMI8658_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#define QMI8658_BUS_I2C DT_HAS_COMPAT_ON_BUS_STATUS_OKAY(qst_qmi8658, i2c)
#define QMI8658_BUS_SPI DT_HAS_COMPAT_ON_BUS_STATUS_OKAY(qst_qmi8658, spi)

union qmi8658_bus {
#ifdef QMI8658_BUS_I2C
    struct i2c_dt_spec i2c;
#endif
};

typedef int (*qmi8658_bus_check_fn)(const union qmi8658_bus *bus);

typedef int (*qmi8658_reg_read_fn)(const union qmi8658_bus *bus, uint16_t reg, uint8_t *data, size_t size);

typedef int (*qmi8658_reg_write_fn)(const union qmi8658_bus *bus, uint16_t reg, uint8_t data);

typedef int (*qmi8658_reg_update_fn)(const union qmi8658_bus *bus, uint16_t reg, uint8_t mask, uint8_t data);

struct qmi8658_bus_io {
    qmi8658_bus_check_fn check;
    qmi8658_reg_read_fn read;
    qmi8658_reg_write_fn write;
    qmi8658_reg_update_fn update;
};

#ifdef QMI8658_BUS_I2C
extern const struct qmi8658_bus_io qmi8658_bus_io_i2c;
#endif

struct qmi8658_data {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    uint16_t accel_fs;
    uint16_t accel_hz;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    uint16_t gyro_fs;
    uint16_t gyro_hz;
    uint16_t temp;
};

struct qmi8658_config {
    union qmi8658_bus bus;
    const struct qmi8658_bus_io *bus_io;
};
#endif //ZEPHYR_DRIVERS_SENSOR_QMI8658_QMI8658_H_