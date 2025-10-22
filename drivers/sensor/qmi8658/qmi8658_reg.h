#ifndef ZEPHYR_DRIVERS_SENSOR_QMI8658_QMI8658_REG_H_
#define ZEPHYR_DRIVERS_SENSOR_QMI8658_QMI8658_REG_H_

#include <zephyr/sys/util.h>

// Identify
#define WHO_AM_I_QMI8658 0x05
#define REVERSION_ID_QMI8658 0x7c

// Reg address define
#define REG_WHO_AM_I 0x00    // who am i register
#define REG_REVERSION_ID 0x01 // revision id register
#define REG_CTRL1 0x02      // SPI interface and sensor enable
#define REG_CTRL2 0x03      // Accelerometer settings
#define REG_CTRL3 0x04      // Gyro settings
#define REG_CTRL4 0x05      // reserved
#define REG_CTRL5 0x06      // Low-pass filter settings
#define REG_CTRL6 0x07      // AttitudeEngine settings
#define REG_CTRL7 0x08      // Sensor enable
#define REG_CTRL8 0x09      // Motion detection control
#define REG_CTRL9 0x0A      // Host commands

#define REG_CAL1_L 0x0B     // calibration 1 register, lower bits
#define REG_CAL1_H 0x0C     // calibration 1 register, higher bits

#define REG_CAL2_L 0x0D     // calibration 2 register, lower bits
#define REG_CAL2_H 0x0E     // calibration 2 register, higher bits

#define REG_CAL3_L 0x0F     // calibration 3 register, lower bits
#define REG_CAL3_H 0x10     // calibration 3 register, higher bits

#define REG_CAL4_L 0x11     // calibration 4 register, lower bits
#define REG_CAL4_H 0x12     // calibration 4 register, higher bits

#define REG_STATUS_INT 0x2D // status + interrupt register
#define REG_STATUS0 0x2E     // Output Data Over Run and Data Availability.
#define REG_STATUS1 0x2F     // Miscellaneous Status: Any Motion, No Motion,
// Significant Motion, Pedometer, Tap.

#define REG_TEMP_L 0x33     // lower bits of temperature data
#define REG_TEMP_H 0x34     // upper bits of temperature data

#define REG_AX_L 0x35       // lower bits of x-axis acceleration
#define REG_AX_H 0x36       // upper bits of x-axis acceleration
#define REG_AY_L 0x37
#define REG_AY_H 0x38
#define REG_AZ_L 0x39
#define REG_AZ_H 0x3A

#define REG_GX_L 0x3B       // lower bits of x-axis angular velocity
#define REG_GX_H 0x3C       // upper bits of x-axis angular velocity
#define REG_GY_L 0x3D
#define REG_GY_H 0x3E
#define REG_GZ_L 0x3F
#define REG_GZ_H 0x40

#define REG_SOFT_RESET 0x60 // soft reset register

// REG_CTRL1
#define BIT_SIM   BIT(7)    // 0: Enables 4-wire SPI interface, 1: Enables 3-wire SPI interface
#define BIT_ADDR_AI BIT(6)  // Address auto increment, 0: Disable, 1: Enable
#define BIT_BE    BIT(5)    // Big/little endian data selection, 0: Little-Endian, 1: Big-Endian
#define BIT_OSC_DIS BIT(0)   // Internal oscillator disable bit

// REG_CTRL2
#define MASK_ACCEL_FS GENMASK(6,4)
#define BIT_ACCEL_FS_2G  0x00
#define BIT_ACCEL_FS_4G  0x01
#define BIT_ACCEL_FS_8G  0x02
#define BIT_ACCEL_FS_16G 0x03

// In 6DOF mode (accelerometer and gyroscope are both enabled), the ODR is derived from the nature frequency of
// gyroscope, ODR below is for 6DOF mode, which is frequently been used.
#define MASK_ACCEL_ODR GENMASK(3,0)
#define BIT_ACCEL_ODR_8000HZ 0x00
#define BIT_ACCEL_ODR_4000HZ 0x01
#define BIT_ACCEL_ODR_2000HZ 0x02
#define BIT_ACCEL_ODR_1000HZ 0x03
#define BIT_ACCEL_ODR_500HZ  0x04
#define BIT_ACCEL_ODR_200HZ  0x05
#define BIT_ACCEL_ODR_100HZ  0x06
#define BIT_ACCEL_ODR_50HZ   0x07
#define BIT_ACCEL_ODR_25HZ   0x08

// REG_CTRL3
#define MASK_GYRO_FS GENMASK(6,4)
#define BIT_GYRO_FS_16DPS 0x00
#define BIT_GYRO_FS_32DPS 0x01
#define BIT_GYRO_FS_64DPS 0x02
#define BIT_GYRO_FS_128DPS 0x03
#define BIT_GYRO_FS_256DPS 0x04
#define BIT_GYRO_FS_512DPS 0x05
#define BIT_GYRO_FS_1024DPS 0x06
#define BIT_GYRO_FS_2048DPS 0x07

#define MASK_GYRO_ODR GENMASK(3,0)
#define BIT_GYRO_ODR_8000HZ 0x00
#define BIT_GYRO_ODR_4000HZ 0x01
#define BIT_GYRO_ODR_2000HZ 0x02
#define BIT_GYRO_ODR_1000HZ 0x03
#define BIT_GYRO_ODR_500HZ  0x04
#define BIT_GYRO_ODR_200HZ  0x05
#define BIT_GYRO_ODR_100HZ  0x06
#define BIT_GYRO_ODR_50HZ   0x07
#define BIT_GYRO_ODR_25HZ   0x08

// REG_CTRL5, Sensor Data Processing Settings. Register Address: 6 (0x06)
#define MASK_GLPF_MODE GENMASK(6,5)
#define BIT_GLPF_2_66  0x00
#define BIT_GLPF_3_63  0x01
#define BIT_GLPF_5_39  0x02
#define BIT_GLPF_13_37 0x03

#define BIT_GLPF_EN BIT(4) // Gyro low pass filter enable bit

#define MASK_ALPG_MODE GENMASK(2,1)
#define BIT_ALPF_2_66  0x00
#define BIT_ALPF_3_63  0x01
#define BIT_ALPF_5_39  0x02
#define BIT_ALPF_13_37 0x03

#define BIT_ALPF_EN BIT(0) // Acc low pass filter enable bit


// REG_CTRL7, Sensor Enable Control. Register Address: 8 (0x08)
#define BIT_GEN BIT(1) // Gyro sensor enable bit
#define BIT_AEN BIT(0) // Acc sensor enable bit
#define BIT_SYNC_SAMPLE_EN BIT(7) // Synchronized sample enable bit

// 0: Gyroscope in Full Mode (Drive and Sense are enabled).
// 1: Gyroscope in Snooze Mode (only Drive enabled).
#define BIT_GSN BIT(4)

// REG_CTRL8
#define BIT_ACTIVITY_INT_SEL BIT(6) // Activity interrupt select bit
#define BIT_PEDO_EN BIT(4) // Pedometer function enable bit
#define BIT_SIG_MOTION_EN BIT(3) // Step detection function enable bit
#define BIT_NO_MOTION_EN BIT(2) // No motion detection function enable bit
#define BIT_ANY_MOTION_EN BIT(1) // Any motion detection function enable bit
#define BIT_TAP_EN BIT(0) // Tap detection function enable bit

// REG_STATUS_INT
// If syncSmpl (CTRL7.bit7) = 1:
// 0: Sensor Data is not available, 1: Sensor Data is available for reading
// If syncSmpl = 0, this bit shows the same value of INT2 level
#define BIT_AVAIL  BIT(0)

// REG_STATUS0
#define BIT_GDA BIT(1)  // new accel data available
#define BIT_ADA BIT(0)  // new gyro data available

// REG_STATUS1
#define BIT_SIG_MOTION BIT(7)
#define BIT_NO_MOTION BIT(6)
#define BIT_ANY_MOTION BIT(5)
#define BIT_PEDOMETER BIT(4)
#define BIT_WOM BIT(2)
#define BIT_TAP BIT(1)

// REG_RESET
#define BIT_SOFT_RESET 0xB0

// Const value
#define ACCEL_DATA_SIZE 6
#define GYRO_DATA_SIZE 6
#define TEMP_DATA_SZE 2

// Sensitivity shift const value
#define ACCEL_SENSITIVITY_SHIFT 14
// Gyro sensitivity shift value
#define GYRO_SENSITIVITY_SHIFT 11

#endif //ZEPHYR_DRIVERS_SENSOR_QMI8658_QMI8658_REG_H_
