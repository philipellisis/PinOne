#include "BleController.h"
#include <Preferences.h>
#include "Globals.h"

// BLE HID report descriptor — keyboard (ID 1) + gamepad (ID 2) + DOF output (ID 3)
static const uint8_t hidReportMap[] = {
    // ---- Keyboard (Report ID 1) ----
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, BLE_REPORT_ID_KEYBOARD,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02,
    0x05, 0x07, 0x19, 0x00, 0x29, 0xFF, 0x15, 0x00,
    0x26, 0xFF, 0x00, 0x95, 0x06, 0x75, 0x08, 0x81, 0x00,
    0xC0,

    // ---- Gamepad (Report ID 2) ----
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, BLE_REPORT_ID_GAMEPAD,
    // 16 buttons
    0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02,
    // D-pad hat switch
    0x05, 0x01, 0x09, 0x39, 0x15, 0x01, 0x25, 0x08,
    0x35, 0x00, 0x46, 0x3B, 0x01, 0x66, 0x14, 0x00,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x42,
    0x75, 0x04, 0x95, 0x01, 0x15, 0x00, 0x25, 0x00,
    0x35, 0x00, 0x45, 0x00, 0x65, 0x00, 0x81, 0x03,
    // 5 axes
    0x05, 0x01, 0x09, 0x01, 0xA1, 0x00,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34, 0x09, 0x32,
    0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x05, 0x81, 0x02,
    0xC0,
    // DOF output feature report (Report ID 3)
    0x06, 0x00, 0xFF, 0x09, 0x01,
    0x75, 0x08, 0x95, 0x08, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x85, BLE_REPORT_ID_OUTPUT, 0xB1, 0x02,
    0xC0,
};

bool BleController::_connected = false;

const uint16_t BleController::SLOT_TO_BITMASK[9] = {
    0, GP_BTN_A, GP_BTN_B, GP_BTN_X, GP_BTN_Y,
    GP_BTN_LB, GP_BTN_RB, GP_BTN_SELECT, GP_BTN_START
};

static const char* NVS_NAMESPACE = "pinball";
static const char* NVS_KEY       = "btnmap";
static const char* NVS_NAME_KEY  = "devname";

// =====================================================================
// NimBLE Callbacks
// =====================================================================

class BleController::OutputCallbacks : public NimBLECharacteristicCallbacks {
    BleController* _ctrl;
public:
    OutputCallbacks(BleController* ctrl) : _ctrl(ctrl) {}

    void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo& info) override {
        const uint8_t* data = chr->getValue().data();
        size_t len          = chr->getValue().length();
        if (len < 8) return;
        memcpy(_ctrl->_outputPkt, data, 8);
        _ctrl->_outputDirty = true;
    }
};

class BleController::ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
        NimBLEDevice::startSecurity(info.getConnHandle());
        srv->updateConnParams(info.getConnHandle(), 6, 7, 0, 600);
    }

    void onAuthenticationComplete(NimBLEConnInfo& info) override {
        if (info.isEncrypted()) {
            BleController::_connected = true;
        } else {
            NimBLEDevice::getServer()->disconnect(info.getConnHandle());
        }
    }

    void onDisconnect(NimBLEServer* srv, NimBLEConnInfo& info, int reason) override {
        BleController::_connected = false;
        NimBLEDevice::startAdvertising();
    }
};

// =====================================================================
// NVS Load/Save
// =====================================================================

void BleController::initButtonMap() {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, true)) {
        size_t len = prefs.getBytesLength(NVS_KEY);
        if (len == NUM_RAW_BUTTONS) {
            uint8_t buf[NUM_RAW_BUTTONS];
            prefs.getBytes(NVS_KEY, buf, NUM_RAW_BUTTONS);
            for (int i = 0; i < NUM_RAW_BUTTONS; i++) {
                _buttonMap[i] = buf[i];
            }
        }
        prefs.end();
    }
}

void BleController::saveButtonMap() {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        uint8_t buf[NUM_RAW_BUTTONS];
        for (int i = 0; i < NUM_RAW_BUTTONS; i++) {
            buf[i] = (uint8_t)_buttonMap[i];
        }
        prefs.putBytes(NVS_KEY, buf, NUM_RAW_BUTTONS);
        prefs.end();
    }
}

void BleController::loadDeviceName(char* nameBuf, size_t bufSize) {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, true)) {
        String name = prefs.getString(NVS_NAME_KEY, "");
        prefs.end();
        if (name.length() > 0 && name.length() < bufSize) {
            strncpy(nameBuf, name.c_str(), bufSize - 1);
            nameBuf[bufSize - 1] = '\0';
            return;
        }
    }
    strncpy(nameBuf, "PinOne", bufSize - 1);
    nameBuf[bufSize - 1] = '\0';
}

void BleController::saveDeviceName(const uint8_t* buf) {
    char name[32];
    memcpy(name, buf, 31);
    name[31] = '\0';

    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putString(NVS_NAME_KEY, name);
        prefs.end();
    }
}

// =====================================================================
// Public API
// =====================================================================

void BleController::begin() {
    loadDeviceName(_deviceName, sizeof(_deviceName));
    initButtonMap();

    NimBLEDevice::init(_deviceName);
    NimBLEDevice::setPower(9);
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    _server = NimBLEDevice::createServer();
    static ServerCallbacks cb;
    _server->setCallbacks(&cb);
    _server->advertiseOnDisconnect(false);

    _hidDevice = new NimBLEHIDDevice(_server);
    _hidDevice->setManufacturer("PinOne");
    _hidDevice->setPnp(0x02, 0x0E8F, 0x9208, 0x0100);
    _hidDevice->setHidInfo(0x00, 0x01);
    _hidDevice->setBatteryLevel(100);
    _hidDevice->setReportMap((uint8_t*)hidReportMap, sizeof(hidReportMap));

    _kbReport = _hidDevice->getInputReport(BLE_REPORT_ID_KEYBOARD);
    _gpReport = _hidDevice->getInputReport(BLE_REPORT_ID_GAMEPAD);

    _outputReport = _hidDevice->getFeatureReport(BLE_REPORT_ID_OUTPUT);
    static OutputCallbacks outCb(this);
    _outputReport->setCallbacks(&outCb);

    _hidDevice->startServices();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setName(_deviceName);
    adv->addServiceUUID(_hidDevice->getHidService()->getUUID());
    adv->addServiceUUID(_hidDevice->getBatteryService()->getUUID());
    adv->setAppearance(HID_KEYBOARD);
    adv->enableScanResponse(true);
    adv->start();
}

void BleController::update() {
    // Apply any DOF output packets from the BLE host
    applyOutputPackets();

    if (!_connected) return;

    // Check if keyboard mode changed
    bool kbdMode = config.disableButtonPressWhenKeyboardEnabled;
    bool modeChanged = (kbdMode != _prevKbdMode);

    if (modeChanged) {
        releaseAllKeys();
        sendGamepad(0, DPAD_CENTERED, 32768, 32768, 0, 0, 32768);
        _prevKbdMod = 0;
        memset(_prevKbdKeys, 0, sizeof(_prevKbdKeys));
        _prevGpButtons = 0;
        _prevDpad = DPAD_CENTERED;
        _prevLX = _prevLY = _prevPlunger = 0;
        _prevLT = _prevRT = 0;
        _prevKbdMode = kbdMode;
    }

    if (kbdMode) {
        processKeyboardMode();
    } else {
        processGamepadMode();
    }
}

void BleController::processKeyboardMode() {
    uint8_t curMod = 0;
    uint8_t curKeys[6] = {0};
    int keyCount = 0;

    for (int i = 0; i < NUM_RAW_BUTTONS; i++) {
        if (!config.processedButtonState[i]) continue;
        uint8_t kc = config.buttonKeyboard[i];
        if (kc == 0x00 || kc == 0xFF) continue;
        if (kc >= 0xE0 && kc <= 0xE7) {
            curMod |= (1 << (kc - 0xE0));
        } else if (keyCount < 6) {
            curKeys[keyCount++] = kc;
        }
    }

    if (curMod != _prevKbdMod || memcmp(curKeys, _prevKbdKeys, 6) != 0) {
        sendKeyboard(curMod, curKeys);
        _prevKbdMod = curMod;
        memcpy(_prevKbdKeys, curKeys, 6);
    }
}

void BleController::processGamepadMode() {
    uint16_t curButtons = 0;
    bool dpadUp = false, dpadDown = false, dpadLeft = false, dpadRight = false;
    uint16_t curLT = 0, curRT = 0;

    for (int i = 0; i < NUM_RAW_BUTTONS; i++) {
        if (!config.processedButtonState[i]) continue;
        int slot = _buttonMap[i];
        switch (slot) {
            case 1: case 2: case 3: case 4:
            case 5: case 6: case 7: case 8:
                curButtons |= SLOT_TO_BITMASK[slot];
                break;
            case 9:  dpadUp    = true; break;
            case 10: dpadDown  = true; break;
            case 11: dpadLeft  = true; break;
            case 12: dpadRight = true; break;
            case 13: curLT = 1023;     break;
            case 14: curRT = 1023;     break;
            default: break;
        }
    }

    uint8_t dpad = buildDpad(dpadUp, dpadRight, dpadDown, dpadLeft);

    // Read analog axes directly from globals
    int16_t plungerVal = plunger.getAdjustedValue();
    int16_t accelX = accel.getXValue();
    int16_t accelY = accel.getYValue();

    // Scale plunger from -128..127 to -32767..32767
    int32_t ps = ((int32_t)plungerVal * 32767) / 127;
    if (ps >  32767) ps =  32767;
    if (ps < -32767) ps = -32767;
    int16_t scaledPlunger = (int16_t)ps;

    if (curButtons != _prevGpButtons || dpad != _prevDpad ||
        accelX != _prevLX || accelY != _prevLY || scaledPlunger != _prevPlunger ||
        curLT != _prevLT || curRT != _prevRT) {

        sendGamepad(
            curButtons, dpad,
            (uint16_t)((int32_t)accelX + 32768),
            (uint16_t)((int32_t)accelY + 32768),
            (uint16_t)(curLT * 64),
            (uint16_t)(curRT * 64),
            (uint16_t)((int32_t)scaledPlunger + 32768)
        );
        _prevGpButtons = curButtons;
        _prevDpad      = dpad;
        _prevLX        = accelX;
        _prevLY        = accelY;
        _prevPlunger   = scaledPlunger;
        _prevLT        = curLT;
        _prevRT        = curRT;
    }
}

void BleController::applyOutputPackets() {
    uint8_t buf[8];
    while (getOutputPacket(buf)) {
        if (buf[0] == 0xFF) {
            // Single output: [0xFF, outputId, value, ...]
            outputs.updateOutput(buf[1], buf[2]);
        } else if (buf[0] <= 8) {
            // Bank update: [bankIndex, v0, v1, v2, v3, v4, v5, v6]
            uint8_t baseIndex = buf[0] * 7;
            for (int i = 0; i < 7; i++) {
                outputs.updateOutput(baseIndex + i, buf[1 + i]);
            }
        }
    }
}

// =====================================================================
// Config Tool Interface
// =====================================================================

void BleController::updateButtonMap(const uint8_t* mapData, uint8_t len) {
    if (len > NUM_RAW_BUTTONS) len = NUM_RAW_BUTTONS;
    for (int i = 0; i < len; i++) {
        _buttonMap[i] = mapData[i];
    }
    saveButtonMap();
    releaseAllKeys();
    sendGamepad(0, DPAD_CENTERED, 32768, 32768, 0, 0, 32768);
}

void BleController::updateDeviceName(const uint8_t* nameData, uint8_t len) {
    saveDeviceName(nameData);
}

// =====================================================================
// BLE HID Send Methods
// =====================================================================

void BleController::sendKeyboard(uint8_t modifiers, const uint8_t keys[6]) {
    if (!_connected || !_kbReport) return;
    _kbState.modifiers = modifiers;
    memcpy(_kbState.keys, keys, 6);
    _kbReport->setValue((uint8_t*)&_kbState, sizeof(_kbState));
    _kbReport->notify();
}

void BleController::releaseAllKeys() {
    if (!_kbReport) return;
    _kbState = KeyReport{};
    _kbReport->setValue((uint8_t*)&_kbState, sizeof(_kbState));
    if (_connected) _kbReport->notify();
}

void BleController::sendGamepad(uint16_t buttons, uint8_t dpad,
                                 uint16_t lx, uint16_t ly,
                                 uint16_t lt, uint16_t rt,
                                 uint16_t z) {
    if (!_connected || !_gpReport) return;
    _gpState.buttons = buttons;
    _gpState.dpad    = dpad;
    _gpState.lx      = lx;
    _gpState.ly      = ly;
    _gpState.lt      = lt;
    _gpState.rt      = rt;
    _gpState.z       = z;
    _gpReport->setValue((uint8_t*)&_gpState, sizeof(_gpState));
    _gpReport->notify();
}

bool BleController::getOutputPacket(uint8_t* buf) {
    if (!_outputDirty) return false;
    _outputDirty = false;
    memcpy(buf, _outputPkt, 8);
    return true;
}

uint8_t BleController::buildDpad(bool up, bool right, bool down, bool left) {
    if (up    && right) return DPAD_UP_RIGHT;
    if (right && down)  return DPAD_DOWN_RIGHT;
    if (down  && left)  return DPAD_DOWN_LEFT;
    if (left  && up)    return DPAD_UP_LEFT;
    if (up)    return DPAD_UP;
    if (right) return DPAD_RIGHT;
    if (down)  return DPAD_DOWN;
    if (left)  return DPAD_LEFT;
    return DPAD_CENTERED;
}
