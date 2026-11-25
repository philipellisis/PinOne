#pragma once

#include <Arduino.h>
#include "Rumble.h"

// USB XInput mode: the board enumerates with an Xbox 360 controller interface
// (vendor class 0xFF / subclass 0x5D / protocol 0x01) in place of the HID
// joystick. The config/DOF HID channel, keyboard, consumer keys and CDC port
// stay, so the config tool still connects and can switch the mode back off.
//
// Windows loads its built-in Xbox driver (xusb22) for our own VID through the
// Microsoft OS 2.0 descriptor, which gives the interface the compatible ID
// XUSB10. That descriptor is only fetched when bcdUSB is 2.1 or higher, which
// usbHidSetup() sets in this mode.
//
// Selected by config.usbXInputMode; read once at power-up, so a change applies
// after a restart. Holding any button at power-up boots in normal mode instead.

// A product ID of its own, so Windows never applies the driver bindings and
// caches it keeps for the normal HID composite (0x9208) to this one. The config
// tool accepts both.
#define PINONE_XINPUT_PID 0x9209

// Length of the interface + vendor + two endpoint descriptors
#define XINPUT_DESC_LEN (9 + 17 + 7 + 7)

class XInputGamepad {
public:
    // Registers the XInput interface and class driver with TinyUSB. Call from
    // usbHidSetup(), before USB.begin().
    void setup();

    // Builds the report from the current inputs and sends it when it changed,
    // and drives the rumble outputs. Call every loop pass, in XInput mode only.
    void update();

private:
    struct __attribute__((packed)) Report {
        uint8_t  type     = 0x00;
        uint8_t  length   = 0x14;
        uint16_t buttons  = 0;   // XI_BTN_* bits below
        uint8_t  lt       = 0;
        uint8_t  rt       = 0;
        int16_t  lx       = 0;   // XInput sticks: +X = right, +Y = up
        int16_t  ly       = 0;
        int16_t  rx       = 0;
        int16_t  ry       = 0;
        uint8_t  reserved[6] = {};
    };
    static_assert(sizeof(Report) == 20, "XInput input report is 20 bytes");

    Report buildReport();
    bool send(const Report& report);

    Report _sent;
    RumbleOutputs _rumble;
};

extern XInputGamepad XInputPad;
