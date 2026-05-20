#ifndef USB_HID_H
#define USB_HID_H

#include <Arduino.h>
#include "USB.h"
#include "USBHIDGamepad.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "USBHID.h"

// Media key constants (same as V1)
#define MEDIA_VOLUME_MUTE    0xE2
#define MEDIA_VOLUME_DOWN    0xEA
#define MEDIA_VOLUME_UP      0xE9

// Keyboard key code mapping function
inline uint8_t KeyboardKeycode(uint8_t key) {
    return key;
}

// Gamepad report structure (matches V1 HID report layout exactly)
typedef union __attribute__((packed)) {
    uint32_t buttons;
    struct __attribute__((packed)) {
        uint8_t button1 : 1;
        uint8_t button2 : 1;
        uint8_t button3 : 1;
        uint8_t button4 : 1;
        uint8_t button5 : 1;
        uint8_t button6 : 1;
        uint8_t button7 : 1;
        uint8_t button8 : 1;

        uint8_t button9 : 1;
        uint8_t button10 : 1;
        uint8_t button11 : 1;
        uint8_t button12 : 1;
        uint8_t button13 : 1;
        uint8_t button14 : 1;
        uint8_t button15 : 1;
        uint8_t button16 : 1;

        uint8_t button17 : 1;
        uint8_t button18 : 1;
        uint8_t button19 : 1;
        uint8_t button20 : 1;
        uint8_t button21 : 1;
        uint8_t button22 : 1;
        uint8_t button23 : 1;
        uint8_t button24 : 1;

        uint8_t button25 : 1;
        uint8_t button26 : 1;
        uint8_t button27 : 1;
        uint8_t button28 : 1;
        uint8_t button29 : 1;
        uint8_t button30 : 1;
        uint8_t button31 : 1;
        uint8_t button32 : 1;

        int16_t xAxis;
        int16_t yAxis;
        int16_t rxAxis;
        int16_t ryAxis;
        int8_t zAxis;
        int8_t rzAxis;
        uint8_t dPad1 : 4;
        uint8_t dPad2 : 4;
    };
} HID_GamepadReport_Data_t;

// Custom HID device for the gamepad report descriptor (matches V1 exactly)
class MinimalGamepad : public USBHIDDevice {
public:
    MinimalGamepad();

    void begin();
    void xAxis(int16_t value);
    void yAxis(int16_t value);
    void zAxis(int8_t value);
    void press(uint8_t button);
    void release(uint8_t button);
    void write();

    // USBHIDDevice interface
    uint16_t _onGetDescriptor(uint8_t* buffer) override;

private:
    USBHID _hid;
    HID_GamepadReport_Data_t _report;
    bool _ready;
};

// Wrapper around ESP32 USB keyboard using same API as V1
class BootKeyboardClass {
public:
    BootKeyboardClass();
    void begin();
    void press(uint8_t key);
    void release(uint8_t key);

private:
    uint8_t _keyReport[8]; // modifiers + reserved + 6 keys
    void sendReport();
};

// Wrapper around ESP32 USB consumer control using same API as V1
class SingleConsumerClass {
public:
    SingleConsumerClass();
    void begin();
    void press(uint16_t key);
    void release(uint16_t key);

private:
    uint16_t _consumerReport;
};

// Global instances (same names as V1 for compatibility)
extern MinimalGamepad Gamepad1;
extern BootKeyboardClass BootKeyboard;
extern SingleConsumerClass SingleConsumer;

void usbHidSetup();  // Call before USB.begin() — registers HID descriptor and sets VID/PID
void usbHidStart();  // Call after USB.begin() — opens HID endpoints

#endif
