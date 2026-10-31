#pragma once

#include <Arduino.h>
#include "NimBLEDevice.h"
#include "NimBLEHIDDevice.h"
#include "Outputs.h"
#include "XboxDescriptor.h"

// Report IDs
#define BLE_REPORT_ID_KEYBOARD  1
#define BLE_REPORT_ID_GAMEPAD   2
#define BLE_REPORT_ID_OUTPUT    3

// How often to re-notify the last known keyboard/gamepad report, to recover
// from a silently-dropped BLE notification (see BleController::update()).
#define BLE_HEARTBEAT_INTERVAL_MS 25

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
    // Re-reads the button map from config after the config tool saves it
    void applyConfig();

private:
    // BLE HID report structures
    struct KeyReport {
        uint8_t modifiers = 0;
        uint8_t keys[6]   = {};
    };

    // Xbox Series X input report (Report ID 1), matches xboxReportMap
    struct __attribute__((packed)) XboxReport {
        uint16_t lx      = 32768;
        uint16_t ly      = 32768;
        uint16_t rx      = 32768;
        uint16_t ry      = 32768;
        uint16_t brake   = 0;    // left trigger, 10 bits
        uint16_t accel   = 0;    // right trigger, 10 bits
        uint8_t  dpad    = DPAD_CENTERED;
        uint16_t buttons = 0;
        uint8_t  share   = 0;
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
    void loadDeviceName(char* nameBuf, size_t bufSize);
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
    XboxReport    _xbState{};

    // Latched at begin(): only one profile is ever advertised, so a config
    // change to bleXboxMode takes effect after a restart.
    bool _xboxMode = false;

    // Xbox rumble motors running (magnitude > 0) as last set by the host, mapped
    // to outputs 1 (strong motor) and 2 (weak motor) in applyOutputPackets().
    volatile bool _rumbleStrong = false;
    volatile bool _rumbleWeak   = false;
    bool _appliedRumbleStrong = false;
    bool _appliedRumbleWeak   = false;

    // DOF output packet received from the BLE host
    uint8_t _outputPkt[8] = {};
    volatile bool _outputDirty = false;

    // Previous state for change detection
    uint8_t  _prevKbdMode = 0;
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
