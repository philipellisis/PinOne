#ifndef USB_HID_H
#define USB_HID_H

#include <Arduino.h>
#include "USB.h"
#include "USBCDC.h"
#include "USBHIDGamepad.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "USBHID.h"
#include "USBHIDVendor.h"

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
        int16_t zAxis;
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
    // axis: 0 = X, 1 = Y, 2 = Z, 3 = Rx, 4 = Ry (the 16-bit axes)
    void setAxis(uint8_t axis, int16_t value);
    // Zero every axis without sending a report
    void resetAxes();
    void press(uint8_t button);
    void release(uint8_t button);
    bool write();
    // Release every button in the report and send it. Returns true when a button was held.
    bool clearButtons();
    // Call every pass: resends the report when the last send failed and refreshes it
    // now and then, so the host never keeps showing a button that is no longer pressed.
    void service();

    // USBHIDDevice interface
    uint16_t _onGetDescriptor(uint8_t* buffer) override;

private:
    USBHID _hid;
    HID_GamepadReport_Data_t _report;
    bool _ready;
    bool _dirty = false;
    uint32_t _lastSent = 0;
    uint32_t _lastTry = 0;
    bool sendNow(uint32_t timeoutMs);
};

// Wrapper around ESP32 USB keyboard using same API as V1
class BootKeyboardClass {
public:
    BootKeyboardClass();
    void begin();
    void press(uint8_t key);
    void release(uint8_t key);
    // Release every key and modifier; sends a report only when something was held.
    bool releaseAll();
    void service();

private:
    uint8_t _keyReport[8]; // modifiers + reserved + 6 keys
    bool _dirty = false;
    uint32_t _lastSent = 0;
    uint32_t _lastTry = 0;
    bool sendNow(uint32_t timeoutMs);
    void sendReport();
};

// Wrapper around ESP32 USB consumer control using same API as V1
class SingleConsumerClass {
public:
    SingleConsumerClass();
    void begin();
    void press(uint16_t key);
    void release(uint16_t key);
    bool releaseAll();
    void service();

private:
    uint16_t _consumerReport;
    bool _dirty = false;
    uint32_t _lastSent = 0;
    uint32_t _lastTry = 0;
    bool sendNow(uint32_t timeoutMs);
};

// Buffered Print wrapper around a USBHIDVendor channel. USBHIDVendor::write()
// blocks on one full HID report transfer per call with no cross-call
// buffering, so routing many small Print calls (e.g. one per config field)
// straight through it would stall the main loop on hundreds of blocking USB
// round trips. This accumulates a full message in RAM and flushes it as a
// single write() call.
class HidPrintBuffer : public Print {
public:
    void begin(USBHIDVendor* dev) { _dev = dev; _len = 0; }
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    void flush();

private:
    USBHIDVendor* _dev = nullptr;
    // Sized for the largest message: full ASCII sendConfig() dump. Worst case (every
    // byte-valued field near 255, e.g. keyboard maps full of high keycodes) is close to
    // 2.1KB with the input-profile fields included, so this has headroom above that.
    uint8_t _buf[2560];
    size_t _len = 0;
};

// Global instances (same names as V1 for compatibility)
extern MinimalGamepad Gamepad1;
extern BootKeyboardClass BootKeyboard;
extern SingleConsumerClass SingleConsumer;
extern USBCDC ComSerial;  // USB CDC serial port; kept only for bootloader/upload enumeration
extern USBHIDVendor HidConfig;   // raw HID channel used for config-tool communication
extern HidPrintBuffer ConfigOut; // buffered Print helper for outbound HidConfig messages

// millis() of the last config-channel transfer to the PC; the periodic report refresh stays
// out of the way while the config tool is being answered.
extern volatile uint32_t g_lastVendorTxMs;

void usbHidSetup();  // Call before USB.begin() — registers HID + CDC, sets VID/PID
void usbHidStart();  // Call after USB.begin() — opens HID endpoints

#endif
