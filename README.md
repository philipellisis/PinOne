# PinOne V2 — ESP32-S3 Virtual Pinball Controller

PinOne V2 is a complete migration of the PinOne virtual pinball gamepad from the ATmega32U4 (Arduino Leonardo) to the ESP32-S3. All functionality is retained with the addition of native BLE support (no separate Bluetooth module needed).

## Pin Mapping: ATmega32U4 → ESP32-S3

| Function | ATmega32U4 Pin | ESP32-S3 GPIO | Type | Notes |
|---|---|---|---|---|
| Shift Register DATA | 0 | GPIO 4 | Digital Input | Serial data from 74HC165 |
| Shift Register CLOCK | 1 | GPIO 5 | Digital Output | Clock for shift registers |
| Shift Register LATCH | 4 | GPIO 6 | Digital Output | Latch/load for shift registers |
| I2C SDA | 2 | GPIO 8 | I2C | PCA9685 x3 + MPU6050 |
| I2C SCL | 3 | GPIO 9 | I2C | PCA9685 x3 + MPU6050 |
| Plunger | 23 (A5) | GPIO 1 | Analog Input | ADC1_CH0, 12-bit (set to 10-bit mode) |
| PWM Output 0 | 5 | GPIO 10 | LEDC ch0 | Variable PWM |
| PWM Output 1 | 6 | GPIO 11 | LEDC ch1 | Variable PWM |
| PWM Output 2 | 10 | GPIO 12 | LEDC ch2 | Variable PWM |
| PWM Output 3 | 9 | GPIO 13 | LEDC ch3 | Variable PWM |
| PWM Output 4 | 8 | GPIO 14 | LEDC ch4 | Variable PWM |
| PWM Output 5 | 7 (was digital) | GPIO 15 | LEDC ch5 | Upgraded to PWM |
| PWM Output 6 | 12 (was digital) | GPIO 16 | LEDC ch6 | Upgraded to PWM |
| PWM Output 7 | 18 (was digital) | GPIO 17 | LEDC ch7 | Upgraded to PWM |
| Digital Output 8 | 19 | GPIO 18 | Digital | On/off only |
| Digital Output 9 | 20 | GPIO 33 | Digital | On/off only |
| Digital Output 10 | 21 | GPIO 34 | Digital | On/off only |
| Digital Output 11 | 22 | GPIO 35 | Digital | On/off only |
| Digital Output 12 | 14 | GPIO 36 | Digital | On/off only |
| Digital Output 13 | 15 | GPIO 37 | Digital | On/off only |
| Digital Output 14 | 16 | GPIO 38 | Digital | On/off only |
| USB D+ | Native | GPIO 20 | USB | Fixed, USB-OTG |
| USB D- | Native | GPIO 19 | USB | Fixed, USB-OTG |
| Reset | Pin 11 (GPIO) | N/A | Software | Uses `ESP.restart()` |
| SPI to BLE ESP32 | 14/15/16/21 | **Eliminated** | N/A | BLE is now native |

### Pins Avoided
- GPIO 0, 3, 45, 46 — strapping pins
- GPIO 26-32 — used by flash/PSRAM on WROOM modules

### Key Differences from V1
- **PWM outputs increased from 5 to 8** using ESP32-S3's 8 LEDC channels
- **3.3V logic** — ESP32-S3 operates at 3.3V (ATmega32U4 was 5V). The 74HC165 shift registers and PCA9685 boards are compatible with 3.3V
- **Native BLE** — the separate ESP32 Bluetooth module and SPI bridge are eliminated
- **Native USB** — USB HID via TinyUSB (ESP32-S3 has built-in USB-OTG)
- **ADC** — 12-bit native, but set to 10-bit mode for calibration compatibility
- **IR** — uses the RMT peripheral instead of bit-banging for precise timing

## Build Configuration (Arduino IDE)

### Board Settings
- **Board**: ESP32S3 Dev Module
- **USB Mode**: USB-OTG (TinyUSB)
- **USB CDC On Boot**: Enabled
- **Partition Scheme**: Default 4MB with spiffs
- **PSRAM**: Disabled (unless your module has PSRAM)

### Required Libraries

Install via Arduino Library Manager:
- `Adafruit PWM Servo Driver Library` (for PCA9685)
- `Adafruit BusIO` (I2C dependency)
- `Adafruit MPU6050` + `Adafruit Unified Sensor`
- `NimBLE-Arduino` v2.3.7 (for BLE HID)

Built-in (no installation needed):
- `Preferences` (ESP32 NVS storage)
- `Wire` (I2C)
- TinyUSB (included when USB-OTG mode selected)

### Board Package
- **esp32 by Espressif** v3.3.6 — install via Arduino IDE Boards Manager
- Do NOT use v3.3.7 (BLE compatibility regression)

## External Hardware (Unchanged from V1)

| Component | Interface | Addresses |
|---|---|---|
| PCA9685 PWM Driver Board 1 | I2C | 0x40 (outputs 15-30) |
| PCA9685 PWM Driver Board 2 | I2C | 0x41 (outputs 31-46) |
| PCA9685 PWM Driver Board 3 | I2C | 0x42 (outputs 47-62) |
| MPU6050 Accelerometer | I2C | 0x68 |
| 74HC165 Shift Registers x3 | GPIO | 24 button inputs |

## Feature Summary

- **Inputs**: 24 buttons (shift registers) + 8 virtual buttons, analog plunger, 6-DOF accelerometer
- **Outputs**: 8 PWM + 7 digital GPIO + 48 PCA9685 channels = 63 total outputs
- **USB HID**: Gamepad (32 buttons, 6 axes, 2 hat switches) + Keyboard + Consumer Control
- **BLE HID**: Gamepad (Xbox-compatible) + Keyboard — native, no external module
- **Light Show**: Attract mode with random animations, night mode
- **IR Transmitter**: NEC, Samsung, Sony protocols via RMT peripheral
- **Configuration**: Serial protocol compatible with PinOne Config Tool
- **Storage**: ESP32 NVS (Preferences library) replaces ATmega32U4 EEPROM

## Firmware Version

V2 reports version `3.0.0` to the config tool (V1 reported `2.2.0`).

## Migration Notes

- Plunger calibration values from V1 will need to be recalibrated via the config tool
- The serial communication protocol is byte-identical to V1 — the config tool works without modification
- BLE button mapping is stored in NVS under the `pinball` namespace (same as the separate ESP32 firmware)
