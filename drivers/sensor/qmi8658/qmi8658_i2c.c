#include "qmi8658.h"
#include "qmi8658_reg.h"

#if QMI8658_BUS_I2C
static inline int qmi8658_bus_check_i2c(const union qmi8658_bus *bus) {
    return i2c_is_ready_dt(&bus->i2c) ? 0 : -ENODEV;
}

static inline int qmi8658_reg_read_i2c(const union qmi8658_bus *bus, const uint16_t reg, uint8_t *data,
                                       const size_t size) {
    return i2c_write_read_dt(&bus->i2c, &reg, 1, data, size);
}

static inline int qmi8658_reg_write_i2c(const union qmi8658_bus *bus, const uint16_t reg, const uint8_t data) {
    int ret = 0;
    ret = i2c_reg_write_byte_dt(&bus->i2c, reg, data);

    return ret;
}

static inline int qmi8658_reg_update_i2c(const union qmi8658_bus *bus, const uint16_t reg, const uint8_t mask,
                                         const uint8_t val) {
    return i2c_reg_update_byte_dt(&bus->i2c, reg, mask, val);
}

const struct qmi8658_bus_io qmi8658_bus_io_i2c = {
    .check = qmi8658_bus_check_i2c,
    .read = qmi8658_reg_read_i2c,
    .write = qmi8658_reg_write_i2c,
    .update = qmi8658_reg_update_i2c,
};
#endif /* QMI8658_BUS_I2C */
