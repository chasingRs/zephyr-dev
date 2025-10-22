#define DT_DRV_COMPAT qst_qmi8658

#include "qmi8658.h"
#include "qmi8658_reg.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "zephyr/sys/byteorder.h"
LOG_MODULE_REGISTER(QMI8658, CONFIG_SENSOR_LOG_LEVEL);

static int qmi8658_set_accel_fs(const struct device *dev, const uint16_t fs) {
    const struct qmi8658_config *cfg = dev->config;
    struct qmi8658_data *data = dev->data;
    struct sensor_value accel_fs_value;
    uint8_t tmp, round_fs;

    if ((fs > 16) || (fs < 2)) {
        LOG_ERR("Unsupported accel fs range");
        return -ENOTSUP;
    }

    if (fs > 8) {
        tmp = BIT_ACCEL_FS_16G;
        round_fs = 16;
    } else if (fs > 4) {
        tmp = BIT_ACCEL_FS_8G;
        round_fs = 8;
    } else if (fs > 2) {
        tmp = BIT_ACCEL_FS_4G;
        round_fs = 4;
    } else {
        tmp = BIT_ACCEL_FS_2G;
        round_fs = 2;
    }

    sensor_g_to_ms2(round_fs, &accel_fs_value);
    data->accel_fs = accel_fs_value.val1;

    return cfg->bus_io->update(&cfg->bus, REG_CTRL2, (uint8_t) MASK_ACCEL_FS,
                               tmp);
}

static int qmi8658_set_gyro_fs(const struct device *dev, const uint16_t fs) {
    const struct qmi8658_config *cfg = dev->config;
    struct qmi8658_data *data = dev->data;
    struct sensor_value gyro_fs_value;
    uint8_t tmp;
    uint16_t round_fs;

    if ((fs > 2048) || (fs < 16)) {
        LOG_ERR("Unsupported gyro fs range");
        return -ENOTSUP;
    }

    if (fs > 1024) {
        tmp = BIT_GYRO_FS_2048DPS;
        round_fs = 2048;
    } else if (fs > 512) {
        tmp = BIT_GYRO_FS_1024DPS;
        round_fs = 1024;
    } else if (fs > 256) {
        tmp = BIT_GYRO_FS_512DPS;
        round_fs = 512;
    } else if (fs > 128) {
        tmp = BIT_GYRO_FS_256DPS;
        round_fs = 256;
    } else if (fs > 64) {
        tmp = BIT_GYRO_FS_128DPS;
        round_fs = 128;
    } else if (fs > 32) {
        tmp = BIT_GYRO_FS_64DPS;
        round_fs = 64;
    } else if (fs > 16) {
        tmp = BIT_GYRO_FS_32DPS;
        round_fs = 32;
    } else {
        tmp = BIT_GYRO_FS_16DPS;
        round_fs = 16;
    }

    sensor_degrees_to_rad(round_fs, &gyro_fs_value);
    data->gyro_fs = gyro_fs_value.val1;

    return cfg->bus_io->update(&cfg->bus, REG_CTRL3, (uint8_t) MASK_GYRO_FS,
                               tmp);
}

static int qmi8658_set_accel_odr(const struct device *dev, const uint16_t rate) {
    const struct qmi8658_config *cfg = dev->config;
    struct qmi8658_data *data = dev->data;
    uint8_t tmp;
    uint16_t round_rate;

    if ((rate > 8000) || (rate < 25)) {
        LOG_ERR("Unsupported accel odr frequency");
        return -ENOTSUP;
    }

    if (rate > 4000) {
        tmp = BIT_ACCEL_ODR_8000HZ;
        round_rate = 8000;
    } else if (rate > 2000) {
        tmp = BIT_ACCEL_ODR_4000HZ;
        round_rate = 4000;
    } else if (rate > 1000) {
        tmp = BIT_ACCEL_ODR_2000HZ;
        round_rate = 2000;
    } else if (rate > 500) {
        tmp = BIT_ACCEL_ODR_1000HZ;
        round_rate = 1000;
    } else if (rate > 200) {
        tmp = BIT_ACCEL_ODR_500HZ;
        round_rate = 500;
    } else if (rate > 100) {
        tmp = BIT_ACCEL_ODR_200HZ;
        round_rate = 200;
    } else if (rate > 50) {
        tmp = BIT_ACCEL_ODR_100HZ;
        round_rate = 100;
    } else {
        tmp = BIT_ACCEL_ODR_50HZ;
        round_rate = 50;
    }

    data->accel_hz = round_rate;

    return cfg->bus_io->update(&cfg->bus, REG_CTRL2, (uint8_t) MASK_ACCEL_ODR,
                               tmp);
}

static int qmi8658_set_gyro_odr(const struct device *dev, const uint16_t rate) {
    const struct qmi8658_config *cfg = dev->config;
    struct qmi8658_data *data = dev->data;
    uint8_t tmp;
    uint16_t round_rate;

    if ((rate > 8000) || (rate < 25)) {
        LOG_ERR("Unsupported gyro odr frequency");
        return -ENOTSUP;
    }

    if (rate > 4000) {
        tmp = BIT_GYRO_ODR_8000HZ;
        round_rate = 8000;
    } else if (rate > 2000) {
        tmp = BIT_GYRO_ODR_4000HZ;
        round_rate = 4000;
    } else if (rate > 1000) {
        tmp = BIT_GYRO_ODR_2000HZ;
        round_rate = 2000;
    } else if (rate > 500) {
        tmp = BIT_GYRO_ODR_1000HZ;
        round_rate = 1000;
    } else if (rate > 200) {
        tmp = BIT_GYRO_ODR_500HZ;
        round_rate = 500;
    } else if (rate > 100) {
        tmp = BIT_GYRO_ODR_200HZ;
        round_rate = 200;
    } else if (rate > 50) {
        tmp = BIT_GYRO_ODR_100HZ;
        round_rate = 100;
    } else {
        tmp = BIT_GYRO_ODR_50HZ;
        round_rate = 50;
    }

    data->gyro_hz = round_rate;

    return cfg->bus_io->update(&cfg->bus, REG_CTRL3, (uint8_t) MASK_GYRO_ODR,
                               tmp);
}

static int qmi8658_sensor_init(const struct device *dev) {
    int ret = 0;
    uint8_t value;
    const struct qmi8658_data *data = dev->data;
    const struct qmi8658_config *cfg = dev->config;

    k_msleep(3);

    ret = cfg->bus_io->write(&cfg->bus, REG_SOFT_RESET,BIT_SOFT_RESET);
    if (ret) {
        LOG_ERR("soft reset failed");
        return ret;
    }

    k_msleep(3);

    // Verify chip ID
    ret = cfg->bus_io->read(&cfg->bus, REG_WHO_AM_I, &value, 1);
    if (ret) {
        LOG_ERR("who_am_i read failed");
        return ret;
    }

    if (value != WHO_AM_I_QMI8658) {
        LOG_ERR("unexpected value for who_am_i: 0x%02X", value);
        return -EINVAL;
    }
    LOG_DBG("device id: 0x%02X", value);

    // Enable address auto increment and internal oscillator
    ret = cfg->bus_io->update(&cfg->bus, REG_CTRL1, BIT_ADDR_AI, BIT_ADDR_AI);
    if (ret) {
        LOG_ERR("reg_ctrl1 setup failed");
        return ret;
    }

    // enable acc and gyro in full mode, and
    // disable syncSample mode
    ret = cfg->bus_io->update(&cfg->bus, REG_CTRL7, BIT_GEN | BIT_AEN,
                              BIT_GEN | BIT_AEN);
    if (ret) {
        LOG_ERR("reg_ctrl7 setup failed");
        return ret;
    }

    ret = qmi8658_set_accel_fs(dev, data->accel_fs);
    if (ret) {
        LOG_ERR("set accel fs failed");
        return ret;
    }
    ret = qmi8658_set_accel_odr(dev, data->accel_hz);
    if (ret) {
        LOG_ERR("set accel odr failed");
        return ret;
    }
    ret = qmi8658_set_gyro_fs(dev, data->gyro_fs);
    if (ret) {
        LOG_ERR("set gyro fs failed");
        return ret;
    }
    ret = qmi8658_set_gyro_odr(dev, data->gyro_hz);
    if (ret) {
        LOG_ERR("set gyro odr failed");
        return ret;
    }
    k_sleep(K_MSEC(30));

    LOG_DBG("qmi8658 initialized successfully");
    return ret;
}

static void qmi8658_convert_accel(struct sensor_value *val, const int16_t raw_val) {
    const int64_t conv_val = ((int64_t) raw_val * SENSOR_G) >> ACCEL_SENSITIVITY_SHIFT;
    val->val1 = (int32_t) (conv_val / 1000000LL);
    val->val2 = (int32_t) (conv_val % 1000000LL);
}

static void qmi8658_convert_gyro(struct sensor_value *val, const int16_t raw_val) {
    const int64_t conv_val = ((int64_t) raw_val * SENSOR_PI) >> GYRO_SENSITIVITY_SHIFT;
    val->val1 = (int32_t) (conv_val / 1000000LL);
    val->val2 = (int32_t) (conv_val % 1000000LL);
}

static int qmi8658_channel_get(const struct device *dev, const enum sensor_channel chan,
                               struct sensor_value *val) {
    int res = 0;
    const struct qmi8658_data *data = dev->data;

    switch (chan) {
        case SENSOR_CHAN_ACCEL_XYZ:
            qmi8658_convert_accel(&val[0], data->accel_x);
            qmi8658_convert_accel(&val[1], data->accel_y);
            qmi8658_convert_accel(&val[2], data->accel_z);
            break;
        case SENSOR_CHAN_ACCEL_X:
            qmi8658_convert_accel(val, data->accel_x);
            break;
        case SENSOR_CHAN_ACCEL_Y:
            qmi8658_convert_accel(val, data->accel_y);
            break;
        case SENSOR_CHAN_ACCEL_Z:
            qmi8658_convert_accel(val, data->accel_z);
            break;
        case SENSOR_CHAN_GYRO_XYZ:
            qmi8658_convert_gyro(&val[0], data->gyro_x);
            qmi8658_convert_gyro(&val[1], data->gyro_y);
            qmi8658_convert_gyro(&val[2], data->gyro_z);
            break;
        case SENSOR_CHAN_GYRO_X:
            qmi8658_convert_gyro(val, data->gyro_x);
            break;
        case SENSOR_CHAN_GYRO_Y:
            qmi8658_convert_gyro(val, data->gyro_y);
            break;
        case SENSOR_CHAN_GYRO_Z:
            qmi8658_convert_gyro(val, data->gyro_z);
            break;
        default:
            LOG_ERR("Unsupported channel");
            res = -ENOTSUP;
            break;
    }

    return res;
}

static int qmi8658_sample_fetch_accel(const struct device *dev) {
    uint8_t ret = 0;
    const struct qmi8658_config *cfg = dev->config;
    struct qmi8658_data *data = dev->data;
    uint8_t buffer[ACCEL_DATA_SIZE];

    ret = cfg->bus_io->read(&cfg->bus, REG_AX_L, buffer, ACCEL_DATA_SIZE);
    if (ret) {
        LOG_ERR("read accel data failed");
        return ret;
    }
    data->accel_x = (int16_t) sys_get_le16(&buffer[0]);
    data->accel_y = (int16_t) sys_get_le16(&buffer[2]);
    data->accel_z = (int16_t) sys_get_le16(&buffer[4]);

    return ret;
}

static int qmi8658_sample_fetch_gyro(const struct device *dev) {
    uint8_t ret = 0;
    const struct qmi8658_config *cfg = dev->config;
    struct qmi8658_data *data = dev->data;
    uint8_t buffer[GYRO_DATA_SIZE];

    ret = cfg->bus_io->read(&cfg->bus, REG_GX_L, buffer, GYRO_DATA_SIZE);
    if (ret) {
        LOG_ERR("read gyro data failed");
        return ret;
    }
    data->gyro_x = (int16_t) sys_get_le16(&buffer[0]);
    data->gyro_y = (int16_t) sys_get_le16(&buffer[2]);
    data->gyro_z = (int16_t) sys_get_le16(&buffer[4]);

    return ret;
}

static int qmi8658_sample_fetch(const struct device *dev, const enum sensor_channel chan) {
    uint8_t ret = 0;
    uint8_t status;
    const struct qmi8658_config *cfg = dev->config;

    ret = cfg->bus_io->read(&cfg->bus,REG_STATUS_INT, &status, 1);
    if (ret) {
        LOG_ERR("read status 0 failed");
        return ret;
    }
    if (!FIELD_GET(BIT_AVAIL, status)) {
        return -EBUSY;
    }

    switch (chan) {
        case SENSOR_CHAN_ALL:
            ret = qmi8658_sample_fetch_accel(dev);
            if (ret) {
                break;
            }
            ret = qmi8658_sample_fetch_gyro(dev);
            if (ret) {
                break;
            }
            break;
        case SENSOR_CHAN_ACCEL_XYZ:
        case SENSOR_CHAN_ACCEL_X:
        case SENSOR_CHAN_ACCEL_Y:
        case SENSOR_CHAN_ACCEL_Z:
            ret = qmi8658_sample_fetch_accel(dev);
            break;
        case SENSOR_CHAN_GYRO_XYZ:
        case SENSOR_CHAN_GYRO_X:
        case SENSOR_CHAN_GYRO_Y:
        case SENSOR_CHAN_GYRO_Z:
            ret = qmi8658_sample_fetch_gyro(dev);
            break;
        default:
            return -ENOTSUP;
    }
    return ret;
}

static int qmi8658_attr_set(const struct device *dev, const enum sensor_channel chan, const enum sensor_attribute attr,
                            const struct sensor_value *val) {
    int ret = 0;
    __ASSERT_NO_MSG(val!=NULL);

    switch (chan) {
        case SENSOR_CHAN_ACCEL_X:
        case SENSOR_CHAN_ACCEL_Y:
        case SENSOR_CHAN_ACCEL_Z:
        case SENSOR_CHAN_ACCEL_XYZ:
            if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
                ret = qmi8658_set_accel_odr(dev, val->val1);
            } else if (attr == SENSOR_ATTR_FULL_SCALE) {
                ret = qmi8658_set_accel_fs(dev, sensor_ms2_to_g(val));
            } else {
                LOG_ERR("Unsupported attribute %d", attr);
                ret = -ENOTSUP;
            }
            break;
        case SENSOR_CHAN_GYRO_X:
        case SENSOR_CHAN_GYRO_Y:
        case SENSOR_CHAN_GYRO_Z:
        case SENSOR_CHAN_GYRO_XYZ:
            if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
                ret = qmi8658_set_gyro_odr(dev, val->val1);
            } else if (attr == SENSOR_ATTR_FULL_SCALE) {
                ret = qmi8658_set_gyro_fs(dev, val->val1);
            } else {
                LOG_ERR("Unsupported attribute %d", attr);
                ret = -ENOTSUP;
            }
            break;

        default:
            LOG_ERR("Unsupported channel %d", attr);
            ret = -ENOTSUP;
            break;
    }
    return ret;
}

static int qmi8658_attr_get(const struct device *dev, const enum sensor_channel chan, const enum sensor_attribute attr,
                            struct sensor_value *val) {
    int ret = 0;
    const struct qmi8658_data *data = dev->data;
    __ASSERT_NO_MSG(val!=NULL);

    val->val2 = 0;
    switch (chan) {
        case SENSOR_CHAN_ACCEL_X:
        case SENSOR_CHAN_ACCEL_Y:
        case SENSOR_CHAN_ACCEL_Z:
        case SENSOR_CHAN_ACCEL_XYZ:
            if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
                val->val1 = data->accel_hz;
            } else if (attr == SENSOR_ATTR_FULL_SCALE) {
                val->val1 = data->accel_fs;
            } else {
                LOG_ERR("Unsupported channel %d", attr);
                ret = -ENOTSUP;
            }
            break;
        case SENSOR_CHAN_GYRO_X:
        case SENSOR_CHAN_GYRO_Y:
        case SENSOR_CHAN_GYRO_Z:
        case SENSOR_CHAN_GYRO_XYZ:
            if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
                val->val1 = data->gyro_hz;
            } else if (attr == SENSOR_ATTR_FULL_SCALE) {
                val->val1 = data->gyro_fs;
            } else {
                LOG_ERR("Unsupported channel %d", attr);
                ret = -ENOTSUP;
            }
            break;
        default:
            LOG_ERR("Unsupported channel %d", attr);
            ret = -ENOTSUP;
            break;
    }
    return ret;
}

static inline int qmi8658_bus_check(const struct device *dev) {
    const struct qmi8658_config *cfg = dev->config;
    return cfg->bus_io->check(&cfg->bus);
}

static int qmi8658_init(const struct device *dev) {
    if (qmi8658_bus_check(dev) < 0) {
        LOG_ERR("Bus is not ready");
        return -ENODEV;
    }
    if (qmi8658_sensor_init(dev)) {
        LOG_ERR("Sensor init failed");
        return -EIO;
    }
    return 0;
}

static DEVICE_API(sensor, qmi8658_driver_api) = {
    .sample_fetch = qmi8658_sample_fetch,
    .channel_get = qmi8658_channel_get,
    .attr_get = qmi8658_attr_get,
    .attr_set = qmi8658_attr_set,
};

#define QMI8658_INIT(inst) \
    static struct qmi8658_data qmi8658_data_##inst = { \
      .accel_hz = DT_INST_PROP(inst, accel_hz), \
      .accel_fs = DT_INST_PROP(inst, accel_fs), \
      .gyro_hz  = DT_INST_PROP(inst, gyro_hz),  \
      .gyro_fs  = DT_INST_PROP(inst, gyro_fs),  \
    }; \
    static const struct qmi8658_config qmi8658_cfg_##inst = { \
        .bus.i2c = I2C_DT_SPEC_INST_GET(inst), \
        .bus_io = &qmi8658_bus_io_i2c,\
    }; \
    SENSOR_DEVICE_DT_INST_DEFINE(inst, qmi8658_init, NULL, &qmi8658_data_##inst, \
        &qmi8658_cfg_##inst, POST_KERNEL, \
        CONFIG_SENSOR_INIT_PRIORITY, &qmi8658_driver_api);

DT_INST_FOREACH_STATUS_OKAY(QMI8658_INIT)