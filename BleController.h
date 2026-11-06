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

// Minimum spacing of reports that only carry axis changes. The accelerometer
// changes on nearly every loop pass, so unpaced it floods the BLE stack. Button
// changes aren't held to this.
#define BLE_AXIS_REPORT_INTERVAL_MS 8

// NimBLE buffers (out of 12) that input reports must leave free. The stack answers
// the host's GATT requests from the same pool, and with none free it fails them
// with "insufficient resources" (see hasBufferRoom() in BleController.cpp).
#define BLE_MIN_FREE_MBUFS 4

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
    // Forgets every paired host and drops the current connection, so a host with a
    // stale pairing can start over
    void clearBonds();

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
        uint16_t rx      = 32768;
        uint16_t ry      = 32768;
        uint16_t lt      = 0;
        uint16_t rt      = 0;
    };

    // Stick axis indices, as used by Config's ble*Axis settings
    static const int NUM_STICK_AXES = 4;  // left X, left Y, right X, right Y

    // Both return false if the report wasn't sent, so the caller can retry it
    bool sendKeyboard(uint8_t modifiers, const uint8_t keys[6]);
    void releaseAllKeys();
    bool sendGamepad(uint16_t buttons, uint8_t dpad,
                     const uint16_t sticks[NUM_STICK_AXES],
                     uint16_t lt, uint16_t rt);
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
    // Host has enabled notifications on the input report (restored from the bond
    // on reconnect). notify() with a value doesn't check this itself.
    static volatile bool _kbSubscribed;
    static volatile bool _gpSubscribed;

    NimBLEServer*        _server        = nullptr;
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

    // Xbox rumble motor magnitudes (0-100) as last set by the host, applied to the
    // configured left (strong motor) and right (weak motor) outputs in
    // applyOutputPackets().
    volatile uint8_t _rumbleLeft  = 0;
    volatile uint8_t _rumbleRight = 0;

    // The output (1-based, 0 = none) a rumble motor last drove and the value it set
    struct RumbleOutput {
        uint8_t output = 0;
        uint8_t value  = 0;
    };
    RumbleOutput _appliedRumbleLeft;
    RumbleOutput _appliedRumbleRight;
    void applyRumble(uint8_t magnitude, uint8_t output, RumbleOutput& applied);

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
    // When the last gamepad report went out, to pace axis-only reports
    uint32_t _lastGpSend = 0;

    // Device name
    char _deviceName[33] = "PinOne";

    class ServerCallbacks;
    class OutputCallbacks;
    class InputCallbacks;
    friend class ServerCallbacks;
    friend class OutputCallbacks;
    friend class InputCallbacks;
};
