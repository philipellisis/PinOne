#include "Arduino.h"
#include <Wire.h>
#include "MPU6050.h"

#define ICM42670_I2C_ADDR       0x69
#define ICM42670_WHO_AM_I       0x75
#define ICM42670_WHO_AM_I_VAL   0x67
#define ICM42670_PWR_MGMT0      0x1F
#define ICM42670_ACCEL_CONFIG0  0x21
#define ICM42670_ACCEL_DATA_X1  0x0B

uint8_t MPU6050::readReg(uint8_t reg)
{
    _wire->beginTransmission(ICM42670_I2C_ADDR);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom((uint8_t)ICM42670_I2C_ADDR, (uint8_t)1);
    return _wire->available() ? _wire->read() : 0xFF;
}

void MPU6050::writeReg(uint8_t reg, uint8_t val)
{
    _wire->beginTransmission(ICM42670_I2C_ADDR);
    _wire->write(reg);
    _wire->write(val);
    _wire->endTransmission();
}

bool MPU6050::init(TwoWire *wire)
{
    _wire = wire;
    initialized = false;
    delay(100);

    if (readReg(ICM42670_WHO_AM_I) != ICM42670_WHO_AM_I_VAL)
    {
        return false;
    }

    // No software reset — RESETN is wired to ESP32 EN pin so hardware reset
    // already guarantees a clean chip state on every boot.
    // After hardware reset the RC oscillator starts automatically; MCLK is ready.

    // Configure ACCEL_CONFIG0: ODR=100Hz (bits[3:0]=0x07), FS_SEL=±16g (bits[6:5]=00)
    writeReg(ICM42670_ACCEL_CONFIG0, 0x07);

    // Enable accelerometer in Low Noise mode
    // PWR_MGMT0: IDLE(bit4)=1 | ACCEL_MODE(bits[1:0])=0b11 → 0x13
    writeReg(ICM42670_PWR_MGMT0, 0x13);
    delay(50);

    initialized = true;
    return true;
}

void MPU6050::setAccelerometerRange(unsigned char new_range)
{
    if (!initialized) return;
    // ACCEL_UI_FS_SEL bits[6:5]: 0=±16g, 1=±8g, 2=±4g, 3=±2g
    uint8_t current = readReg(ICM42670_ACCEL_CONFIG0);
    writeReg(ICM42670_ACCEL_CONFIG0, (current & 0x9F) | ((new_range & 0x03) << 5));
    accelRange = new_range;
}

void MPU6050::read(void)
{
    if (!initialized) return;

    _wire->beginTransmission(ICM42670_I2C_ADDR);
    _wire->write(ICM42670_ACCEL_DATA_X1);
    _wire->endTransmission(false);
    _wire->requestFrom((uint8_t)ICM42670_I2C_ADDR, (uint8_t)6);

    uint8_t buffer[6];
    for (uint8_t i = 0; i < 6; i++) {
        buffer[i] = _wire->available() ? _wire->read() : 0xFF;
    }

    rawAccX = (int16_t)(buffer[0] << 8 | buffer[1]);
    rawAccY = (int16_t)(buffer[2] << 8 | buffer[3]);
    rawAccZ = (int16_t)(buffer[4] << 8 | buffer[5]);
}

int MPU6050::getX() { return rawAccX; }
int MPU6050::getY() { return rawAccY; }
int MPU6050::getZ() { return rawAccZ; }

void MPU6050::config() {}
