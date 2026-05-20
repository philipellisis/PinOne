#include "Arduino.h"
#include <Wire.h>
#include "MPU6050.h"

#define ICM42670_I2C_ADDR       0x68
#define ICM42670_WHO_AM_I       0x75
#define ICM42670_WHO_AM_I_VAL   0x67
#define ICM42670_DEVICE_CONFIG  0x01
#define ICM42670_PWR_MGMT0      0x1F
#define ICM42670_ACCEL_CONFIG0  0x21
#define ICM42670_ACCEL_DATA_X1  0x0B

bool MPU6050::init(TwoWire *wire)
{
    if (i2c_dev)
    {
        delete i2c_dev;
    }
    delay(100);

    wire->setTimeOut(1000);
    i2c_dev = new Adafruit_I2CDevice(ICM42670_I2C_ADDR, wire);

    if (!i2c_dev->begin())
        return false;

    Adafruit_BusIO_Register who_am_i = Adafruit_BusIO_Register(i2c_dev, ICM42670_WHO_AM_I, 1);
    if (who_am_i.read() != ICM42670_WHO_AM_I_VAL)
        return false;

    Adafruit_BusIO_Register device_config = Adafruit_BusIO_Register(i2c_dev, ICM42670_DEVICE_CONFIG, 1);
    Adafruit_BusIO_RegisterBits soft_reset = Adafruit_BusIO_RegisterBits(&device_config, 1, 0);
    soft_reset.write(1);
    delay(10);

    config();
    setAccelerometerRange(0x00);

    Adafruit_BusIO_Register pwr_mgmt0 = Adafruit_BusIO_Register(i2c_dev, ICM42670_PWR_MGMT0, 1);
    pwr_mgmt0.write(0x0C);

    delay(50);

    return true;
}

void MPU6050::setAccelerometerRange(unsigned char new_range)
{
    // ACCEL_FS_SEL: 0x00=±16g, 0x01=±8g, 0x02=±4g, 0x03=±2g
    Adafruit_BusIO_Register accel_config = Adafruit_BusIO_Register(i2c_dev, ICM42670_ACCEL_CONFIG0, 1);
    Adafruit_BusIO_RegisterBits accel_fs_sel = Adafruit_BusIO_RegisterBits(&accel_config, 3, 4);
    accelRange = new_range;
    accel_fs_sel.write(new_range);
}

void MPU6050::read(void)
{
    Adafruit_BusIO_Register data_reg = Adafruit_BusIO_Register(i2c_dev, ICM42670_ACCEL_DATA_X1, 6);

    uint8_t buffer[6];
    data_reg.read(buffer, 6);

    rawAccX = (int16_t)(buffer[0] << 8 | buffer[1]);
    rawAccY = (int16_t)(buffer[2] << 8 | buffer[3]);
    rawAccZ = (int16_t)(buffer[4] << 8 | buffer[5]);
}

int MPU6050::getX() { return rawAccX; }
int MPU6050::getY() { return rawAccY; }
int MPU6050::getZ() { return rawAccZ; }

void MPU6050::config()
{
    // Set ODR to 100Hz in ACCEL_CONFIG0 bits [3:0]; FS_SEL set separately
    Adafruit_BusIO_Register accel_config = Adafruit_BusIO_Register(i2c_dev, ICM42670_ACCEL_CONFIG0, 1);
    Adafruit_BusIO_RegisterBits accel_odr = Adafruit_BusIO_RegisterBits(&accel_config, 4, 0);
    accel_odr.write(0x07); // 100Hz
}
