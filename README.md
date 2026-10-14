# PinOne Ultra — ESP32-S3 Virtual Pinball Controller

PinOne Ultra is a migration of the PinOne virtual pinball gamepad from the ATmega32U4 (Arduino Leonardo) to the ESP32-S3. The V1 feature set is retained, with native BLE (no separate Bluetooth module), up to 15 direct PWM outputs, velocity-based nudge input and input profiles added.

## Pin Mapping

Pin numbers are Arduino/ESP32 GPIO numbers. Source of truth: [Pins.h](Pins.h) (shift register, I2C, plunger) and `Outputs::outputList` in [Outputs.h](Outputs.h) (direct outputs).

| Function | ESP32-S3 GPIO | Type | Notes |
|---|---|---|---|
| Shift Register DATA | GPIO 4 | Digital Input | Serial data from the 74HC165 chain |
| Shift Register CLOCK | GPIO 5 | Digital Output | Clock for shift registers |
| Shift Register LATCH | GPIO 6 | Digital Output | Latch/load for shift registers |
| I2C SDA | GPIO 8 | I2C | PCA9685 x3 + accelerometer |
| I2C SCL | GPIO 9 | I2C | PCA9685 x3 + accelerometer |
| Plunger | GPIO 7 | Analog Input | ADC2_CH7, 12-bit, 11 dB attenuation, read in millivolts |
| Output 0 | GPIO 40 | PWM (LEDC) | 1 kHz, 8-bit |
| Output 1 | GPIO 38 | PWM (LEDC) | 1 kHz, 8-bit |
| Output 2 | GPIO 39 | PWM (LEDC) | 1 kHz, 8-bit |
| Output 3 | GPIO 14 | PWM (LEDC) | 1 kHz, 8-bit |
| Output 4 | GPIO 15 | PWM (LEDC) | 1 kHz, 8-bit |
| Output 5 | GPIO 48 | PWM (LEDC) | 1 kHz, 8-bit |
| Output 6 | GPIO 13 | PWM (LEDC) | 1 kHz, 8-bit |
| Output 7 | GPIO 18 | PWM (LEDC) | 1 kHz, 8-bit |
| Output 8 | GPIO 12 | PWM (MCPWM) | 1 kHz |
| Output 9 | GPIO 11 | PWM (MCPWM) | 1 kHz |
| Output 10 | GPIO 47 | PWM (MCPWM) | 1 kHz |
| Output 11 | GPIO 21 | PWM (MCPWM) | 1 kHz |
| Output 12 | GPIO 45 | PWM (MCPWM) | 1 kHz. GPIO 45 is a strapping pin — keep the load from pulling it high at reset |
| Output 13 | GPIO 42 | PWM (MCPWM) | 1 kHz |
| Output 14 | GPIO 10 | PWM (MCPWM) | 1 kHz |
| USB D+ / D- | GPIO 20 / GPIO 19 | USB | Fixed, native USB-OTG |

Outputs 15-62 are not on GPIOs; they are the 48 channels of the three PCA9685 boards (see [External Hardware](#external-hardware)).

### Pins Avoided
- GPIO 0, 3, 46 — strapping pins
- GPIO 26-32 — used by flash/PSRAM on WROOM modules

### Key Differences from V1
- **15 direct PWM outputs** (V1 had 5) — outputs 0-7 use the ESP32-S3's 8 LEDC channels, outputs 8-14 use 7 of the MCPWM peripheral's 12 channels. Each output can be configured as PWM (dimmable) or on/off-only in the config tool
- **3.3V logic** — the ESP32-S3 runs at 3.3V (ATmega32U4 was 5V). The 74HC165 shift registers and PCA9685 boards are compatible with 3.3V
- **Native BLE** — the separate ESP32 Bluetooth module and SPI bridge are gone
- **Native USB** — USB HID via TinyUSB (the ESP32-S3's built-in USB-OTG)
- **ADC** — 12-bit, read in millivolts; plunger calibration values are stored in mV (0-3300)
- **Accelerometer** — a QST QMI8658 replaces the MPU6050 (see below)
- **IR** — uses the RMT peripheral instead of bit-banging
- **Two FreeRTOS tasks** — see [Architecture](#architecture)

## Architecture

- **Input pass** (`loop()`, core 1): plunger, accelerometer, buttons, USB gamepad/keyboard reports and BLE updates. Every input is read on every pass.
- **Output task** (`OutputTask.cpp`, core 0, priority 2): DOF and config-tool communication, light show, direct PWM and PCA9685 writes, IR transmit, profile-change notifications. The input pass hands it work through a command queue (`postOutputCommand()`) so slow I2C/IR work never delays button or plunger reads.
- **Mutexes** (`Threading.h`): `inputMutex` is held during each input pass and during config-tool config/BLE-map transfers; `i2cMutex` guards the shared I2C bus (accelerometer reads vs. PCA9685 writes).

## Build Configuration (Arduino IDE)

### Board Settings
- **Board**: ESP32S3 Dev Module (or ESP32-S3-USB-OTG)
- **USB Mode**: USB-OTG (TinyUSB)
- **USB CDC On Boot**: Enabled
- **Partition Scheme**: Default 4MB with spiffs
- **PSRAM**: Disabled (unless your module has PSRAM)

### Required Libraries

Install via Arduino Library Manager:
- `Adafruit PWM Servo Driver Library` (for PCA9685)
- `Adafruit BusIO` (I2C dependency)
- `NimBLE-Arduino` v2.3.7 (for BLE HID)

Built-in (no installation needed):
- `Preferences` (ESP32 NVS storage)
- `Wire` (I2C)
- TinyUSB / `USBHID` (included when USB-OTG mode selected)
- ESP-IDF `driver/mcpwm.h` and `driver/rmt_tx.h` (MCPWM outputs, IR)

The accelerometer driver ([MPU6050.cpp](MPU6050.cpp)) talks to the QMI8658 directly over `Wire`, so no accelerometer library is needed.

### Board Package
- **esp32 by Espressif** v3.3.6 — install via Arduino IDE Boards Manager
- Do NOT use v3.3.7 (BLE compatibility regression)

## Command-Line Build (arduino-cli)

You can build and flash without the Arduino IDE using [arduino-cli](https://arduino.github.io/arduino-cli/).

### Install arduino-cli
```powershell
winget install --id ArduinoSA.CLI --source winget
```
(Or download a release binary from the [arduino-cli releases page](https://github.com/arduino/arduino-cli/releases).)

### One-time setup
```powershell
arduino-cli config init
arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.6
arduino-cli lib install "Adafruit PWM Servo Driver Library" "Adafruit BusIO" "NimBLE-Arduino@2.3.7"
```

### FQBN
```
esp32:esp32:esp32s3usbotg
```
This board's default options (`USBMode=default` → USB-OTG, `PartitionScheme=default` → 4MB with spiffs) already match the settings above, so no `--board-options` flags are needed.

### Compile
```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3usbotg .
```

### Upload
```powershell
arduino-cli board list
arduino-cli compile --fqbn esp32:esp32:esp32s3usbotg -u -p COM5 .
```

## Windows "USB Game Controllers" Panel Shows the Wrong Name

Windows' legacy DirectInput control panel (`joy.cpl`) displays the **HID
interface's** string descriptor for composite HID devices, not the device
product string set via `USB.productName()` in `UsbHid.cpp`. The installed
ESP32 Arduino core hardcodes that interface string to `"TinyUSB HID"`, so
the board always shows up there as "TinyUSB HID"-something regardless of
what the sketch sets as the product name — there's no public API on
`USBHID`/`USBHIDDevice` to override it from sketch code.

This only affects that legacy panel's display name; it doesn't affect
actual input functionality (games/Steam use XInput/RawInput and see the
real "PinOne Ultra" product name fine).

**Fix** (a toolchain patch, not part of this repo — must be reapplied after
any ESP32 core reinstall/update):

1. Locate the installed core's `USBHID.cpp`, e.g.:
   `%LOCALAPPDATA%\Arduino15\packages\esp32\hardware\esp32\<version>\libraries\USB\src\USBHID.cpp`
2. Inside `tusb_hid_load_descriptor()`, find:
   ```cpp
   uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB HID");
   ```
3. Change the string literal to the desired name, e.g.:
   ```cpp
   uint8_t str_index = tinyusb_add_string_descriptor("PinOne Ultra");
   ```
4. Recompile and reflash. Windows may keep showing a stale cached name
   until you unplug/replug the device (or, if it's really stuck, delete
   the corresponding `HKEY_CURRENT_USER\...\DirectInput\VID_xxxx&PID_xxxx`
   registry key).

Windows also caches DirectInput calibration/layout per VID+PID. While the HID
report descriptor is still changing, the PID in `usbHidSetup()` is bumped so
Windows builds a fresh cache rather than reusing a stale one.

## External Hardware

| Component | Interface | Address | Notes |
|---|---|---|---|
| PCA9685 PWM Driver Board 1 | I2C | 0x40 | outputs 15-30 |
| PCA9685 PWM Driver Board 2 | I2C | 0x41 | outputs 31-46 |
| PCA9685 PWM Driver Board 3 | I2C | 0x42 | outputs 47-62 |
| QMI8658 Accelerometer | I2C | 0x6B (SA0 high) or 0x6A (SA0 low) | probed at boot; the class is still named `MPU6050` in code |
| 74HC165 Shift Registers x3 | GPIO | — | 24 button inputs |

Presence of each PCA9685 board and the accelerometer (and the address it answered on) is detected at boot and reported to the config tool.

## Feature Summary

- **Inputs**: 24 physical buttons (shift registers) + 8 shifted/virtual buttons (buttons 5-12 become buttons 25-32 while the shift button is held), analog plunger, 2-axis accelerometer nudge
- **Outputs**: 15 direct PWM outputs (8 LEDC + 7 MCPWM) + 48 PCA9685 channels = 63 total. Per-output type (none/noisy/light show/shared), max on-level, max on-time and PWM vs. on/off mode
- **USB HID**: Gamepad (32 buttons, X/Y/Rx/Ry/Z axes at 16-bit, Rz 8-bit, 2 hat switches) + Keyboard + Consumer Control + a raw vendor HID channel for the config tool
- **Plunger**: calibrated min/mid/max in mV, quick-release detection, optional launch-button press at the end positions
- **Accelerometer**: X/Y position on the gamepad X/Y axes, with adjustable range, dead zone, orientation (board rotation in 5° steps, pins up, mounted on side), auto-centering, tilt buttons, and optional velocity output on Rx/Ry
- **Input profiles**: up to 4 keyboard-mapping profiles, switched by holding a configurable button, with output pulses/light flash to confirm the active profile
- **BLE HID**: Xbox-style gamepad + keyboard + DOF output report, native NimBLE; device name and button map are configurable
- **Light Show**: attract mode with random animations, night mode
- **Button Lock**: a configurable button (hold or toggle, like night mode) disables all buttons except an allowed set (default 1, 2, 3, 4, 9, 23, 24) to prevent accidental bumps; state shows in the config tool's Inputs page
- **IR Transmitter**: NEC, Samsung, Sony protocols via RMT peripheral
- **Configuration**: USB HID vendor channel to the PinOne Config Tool
- **DOF**: output packets arrive over the HID vendor channel; a legacy USB CDC serial DOF fallback (enabled by default, `legacySerialDofEnabled`) supports un-updated DirectOutput installs
- **Storage**: ESP32 NVS (Preferences library) — `pinone` namespace for settings, `pinball` namespace for the BLE button map and device name

## Firmware Version

V2 reports version `3.0.0` to the config tool (V1 reported `2.2.0`).

## Migration Notes

- Plunger calibration values from V1 will need to be recalibrated via the config tool (values are now millivolts)
- Config-tool communication (admin/config protocol, live button/output/plunger/accelerometer state) uses a raw USB HID vendor channel rather than the virtual COM port, to avoid exclusivity/driver issues on some Windows machines. The CDC serial port remains for firmware upload/bootloader reset and the legacy DOF serial fallback
- The BLE button mapping is stored in NVS under the `pinball` namespace (same as the separate ESP32 firmware)
