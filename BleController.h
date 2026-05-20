#pragma once

#include <Arduino.h>
#include "NimBLEDevice.h"
#include "NimBLEHIDDevice.h"
#include "Outputs.h"

// Report IDs
#define BLE_REPORT_ID_KEYBOARD  1
#define BLE_REPORT_ID_GAMEPAD   2
#define BLE_REPORT_ID_OUTPUT    3

// Gamepad button bitmasks (Xbox layout)
#define GP_BTN_A        0x0001u
#define GP_BTN_B        0x0002u
#define GP_BTN_X        0x0008u
#define GP_BTN_Y        0x0010u
#define GP_BTN_LB       0x0040u
#define GP_BTN_RB       0x0080u
#define GP_BTN_SELECT   0x0400u
#define GP_BTN_START    0x0800u

// D-pad hat values
#define DPAD_CENTERED    0x00u
#define DPAD_UP          0x01u
#define DPAD_UP_RIGHT    0x02u
#define DPAD_RIGHT       0x03u
#define DPAD_DOWN_RIGHT  0x04u
#define DPAD_DOWN        0x05u
#define DPAD_DOWN_LEFT   0x06u
#define DPAD_LEFT        0x07u
#define DPAD_UP_LEFT     0x08u

class BleController {
public:
    BleController() = default;

    void begin();
    bool isConnected() const { return _connected; }

    // Called from main loop - reads globals and sends BLE HID reports
    void update();

    // Called from Communication.cpp for config tool
    void updateButtonMap(const uint8_t* mapData, uint8_t len);
    void updateDeviceName(const uint8_t* nameData, uint8_t len);

private:
    // BLE HID report structures
    struct KeyReport {
        uint8_t modifiers = 0;
        uint8_t keys[6]   = {};
    };

    struct __attribute__((packed)) GamepadReport {
        uint16_t buttons = 0;
        uint8_t  dpad    = DPAD_CENTERED;
        uint16_t lx      = 32768;
        uint16_t ly      = 32768;
        uint16_t lt      = 0;
        uint16_t rt      = 0;
        uint16_t z       = 32768;
    };

    void sendKeyboard(uint8_t modifiers, const uint8_t keys[6]);
    void releaseAllKeys();
    void sendGamepad(uint16_t buttons, uint8_t dpad,
                     uint16_t lx, uint16_t ly,
                     uint16_t lt, uint16_t rt,
                     uint16_t z);
    bool getOutputPacket(uint8_t* buf);
    void applyOutputPackets();

    // Button mapping
    static const int NUM_RAW_BUTTONS = 32;
    int _buttonMap[NUM_RAW_BUTTONS] = {
        6, 14, 5, 13, 4, 1, 3, 2, 8, 0,
        0,  0, 0,  0, 0, 0, 0, 0, 0, 0,
        0,  0, 0,  0,11, 12, 9,10, 7, 0,
        0,  0
    };

    void initButtonMap();
    void saveButtonMap();
    void loadDeviceName(char* nameBuf, size_t bufSize);
    void saveDeviceName(const uint8_t* buf);
    uint8_t buildDpad(bool up, bool right, bool down, bool left);
    void processGamepadMode();
    void processKeyboardMode();

    static const uint16_t SLOT_TO_BITMASK[9];

    static bool _connected;

    NimBLEServer*         _server        = nullptr;
    NimBLEHIDDevice*      _hidDevice     = nullptr;
    NimBLECharacteristic* _kbReport      = nullptr;
    NimBLECharacteristic* _gpReport      = nullptr;
    NimBLECharacteristic* _outputReport  = nullptr;

    KeyReport     _kbState{};
    GamepadReport _gpState{};

    // DOF output packet received from the BLE host
    uint8_t _outputPkt[8] = {};
    volatile bool _outputDirty = false;

    // Previous state for change detection
    bool     _prevKbdMode = false;
    uint8_t  _prevKbdMod = 0;
    uint8_t  _prevKbdKeys[6] = {0};
    uint16_t _prevGpButtons = 0;
    uint8_t  _prevDpad = DPAD_CENTERED;
    int16_t  _prevLX = 0, _prevLY = 0, _prevPlunger = 0;
    uint16_t _prevLT = 0, _prevRT = 0;

    // Heartbeat timing
    uint32_t _lastUpdate = 0;

    // Device name
    char _deviceName[33] = "PinOne";

    class ServerCallbacks;
    class OutputCallbacks;
    friend class ServerCallbacks;
    friend class OutputCallbacks;
};
