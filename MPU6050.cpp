#include "Arduino.h"
#include <Wire.h>
#include "MPU6050.h"

// QST QMI8658 — I2C address is 0x6B with SA0 high, 0x6A with SA0 low.
// init() probes both so either wiring works.
#define QMI8658_I2C_ADDR_HI     0x6B
#define QMI8658_I2C_ADDR_LO     0x6A
#define QMI8658_WHO_AM_I        0x00
#define QMI8658_WHO_AM_I_VAL    0x05
#define QMI8658_CTRL1           0x02
#define QMI8658_CTRL2           0x03
#define QMI8658_CTRL5           0x06
#define QMI8658_CTRL7           0x08
#define QMI8658_RESET           0x60
#define QMI8658_ACCEL_DATA_X_L  0x35

uint8_t MPU6050::readReg(uint8_t reg)
{
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom((uint8_t)_addr, (uint8_t)1);
    return _wire->available() ? _wire->read() : 0xFF;
}

void MPU6050::writeReg(uint8_t reg, uint8_t val)
{
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(val);
    _wire->endTransmission();
}

bool MPU6050::init(TwoWire *wire)
{
    _wire = wire;
    initialized = false;
    delay(100);

    _addr = QMI8658_I2C_ADDR_HI;
    if (readReg(QMI8658_WHO_AM_I) != QMI8658_WHO_AM_I_VAL)
    {
        _addr = QMI8658_I2C_ADDR_LO;
        if (readReg(QMI8658_WHO_AM_I) != QMI8658_WHO_AM_I_VAL)
        {
            return false;
        }
    }

    // Soft reset for a clean chip state (the QMI8658 module has no reset pin
    // wired to the ESP32, unlike the previous ICM42670 board).
    writeReg(QMI8658_RESET, 0xB0);
    delay(20);

    // CTRL1: ADDR_AI(bit6)=1 for register auto-increment on burst reads,
    // BE(bit5)=0 so sensor data reads little-endian (L register first).
    writeReg(QMI8658_CTRL1, 0x40);

    // CTRL2: aFS bits[6:4]=0b011 (±16g), aODR bits[3:0]=0b0110 (125 Hz,
    // closest to the 100 Hz used on the ICM42670)
    writeReg(QMI8658_CTRL2, 0x36);

    // CTRL5: accel low-pass filter on, mode 3 (BW = 13.37% of ODR → ~16.7 Hz
    // at 125 Hz ODR — closest available to the ~25 Hz target).
    // Nudge motion is 1–20 Hz; cabinet motor/flipper vibration is >40 Hz.
    // aLPF_MODE bits[2:1]=0b11 | aLPF_EN bit0=1 → 0x07
    writeReg(QMI8658_CTRL5, 0x07);

    // CTRL7: enable accelerometer only (aEN bit0)
    writeReg(QMI8658_CTRL7, 0x01);
    delay(50);

    initialized = true;
    return true;
}

void MPU6050::setAccelerometerRange(unsigned char new_range)
{
    if (!initialized) return;
    // Config keeps the ICM42670 encoding (0=±16g, 1=±8g, 2=±4g, 3=±2g);
    // QMI8658 aFS bits[6:4] are reversed (0=±2g … 3=±16g), so translate.
    uint8_t fs = 3 - (new_range & 0x03);
    uint8_t current = readReg(QMI8658_CTRL2);
    writeReg(QMI8658_CTRL2, (current & 0x8F) | (fs << 4));
    accelRange = new_range;
}

void MPU6050::read(void)
{
    if (!initialized) return;

    _wire->beginTransmission(_addr);
    _wire->write(QMI8658_ACCEL_DATA_X_L);
    _wire->endTransmission(false);
    _wire->requestFrom((uint8_t)_addr, (uint8_t)6);

    uint8_t buffer[6];
    for (uint8_t i = 0; i < 6; i++) {
        buffer[i] = _wire->available() ? _wire->read() : 0xFF;
    }

    // QMI8658 outputs little-endian: L register first, then H
    int16_t newX = (int16_t)(buffer[1] << 8 | buffer[0]);
    int16_t newY = (int16_t)(buffer[3] << 8 | buffer[2]);
    int16_t newZ = (int16_t)(buffer[5] << 8 | buffer[4]);

    // Reject single-sample spikes: discard any reading that jumps more than
    // SPIKE_THRESH counts from the last accepted value in one 10ms window.
    // At ±16g / 16-bit, full scale is 32768 counts; 4000 ≈ ±2g per sample.
    const int16_t SPIKE_THRESH = 4000;
    if (abs(newX - rawAccX) < SPIKE_THRESH) rawAccX = newX;
    if (abs(newY - rawAccY) < SPIKE_THRESH) rawAccY = newY;
    if (abs(newZ - rawAccZ) < SPIKE_THRESH) rawAccZ = newZ;
}

int MPU6050::getX() { return rawAccX; }
int MPU6050::getY() { return rawAccY; }
int MPU6050::getZ() { return rawAccZ; }

void MPU6050::config() {}
