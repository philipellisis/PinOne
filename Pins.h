#ifndef PINS_H
#define PINS_H

// =============================================================================
// PinOne V2 - ESP32-S3 Pin Definitions
// =============================================================================
// Target: ESP32-S3-DevKitC-1 (WROOM module)
// Avoided: GPIO 0, 3, 45, 46 (strapping); GPIO 26-32 (flash/PSRAM)
// USB D+/D- are fixed at GPIO 19/20 (USB-OTG)

// --- Shift Register (Button Input) ---
#define PIN_SHIFT_DATA    4   // Serial data from shift register (QH)
#define PIN_SHIFT_CLOCK   5   // Shift register clock (CLK)
#define PIN_SHIFT_LATCH   6   // Shift register latch/load (SH/LD)

// --- I2C Bus (PCA9685 x3 + MPU6050) ---
#define PIN_I2C_SDA       8
#define PIN_I2C_SCL       9

// --- Plunger (Analog Input) ---
#define PIN_PLUNGER       7  // ADC2_CH7

// --- Direct GPIO Outputs (15 total) ---
// Outputs 0-7:  PWM via LEDC (8 channels, hardware max on ESP32-S3)
// Outputs 8-14: PWM via MCPWM (uses 7 of the 12 channels the peripheral
//               supports - 2 units x 3 timers x 2 generators - leaving
//               5 free on MCPWM_UNIT_1 for future expansion)
#define NUM_PWM_OUTPUTS   8
#define NUM_MCPWM_OUTPUTS 7
#define NUM_TOTAL_OUTPUTS 15

#endif
