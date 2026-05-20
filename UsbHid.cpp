#include "UsbHid.h"
#include <string.h>

// HID Report Descriptor for Gamepad (identical to V1 MinimalHID)
static const uint8_t _hidReportDescriptorGamepad[] = {
    /* Gamepad with 32 buttons and 6 axis*/
    0x05, 0x01,                    /* USAGE_PAGE (Generic Desktop) */
    0x09, 0x04,                    /* USAGE (Joystick) */
    0xa1, 0x01,                    /* COLLECTION (Application) */
    0x85, 0x01,                    /*   REPORT_ID (1) */
    /* 32 Buttons */
    0x05, 0x09,                    /*   USAGE_PAGE (Button) */
    0x19, 0x01,                    /*   USAGE_MINIMUM (Button 1) */
    0x29, 0x20,                    /*   USAGE_MAXIMUM (Button 32) */
    0x15, 0x00,                    /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                    /*   LOGICAL_MAXIMUM (1) */
    0x75, 0x01,                    /*   REPORT_SIZE (1) */
    0x95, 0x20,                    /*   REPORT_COUNT (32) */
    0x81, 0x02,                    /*   INPUT (Data,Var,Abs) */
    /* 4 16bit Axis */
    0x05, 0x01,                    /*   USAGE_PAGE (Generic Desktop) */
    0xa1, 0x00,                    /*   COLLECTION (Physical) */
    0x09, 0x30,                    /*     USAGE (X) */
    0x09, 0x31,                    /*     USAGE (Y) */
    0x09, 0x33,                    /*     USAGE (Rx) */
    0x09, 0x34,                    /*     USAGE (Ry) */
    0x16, 0x00, 0x80,              /*     LOGICAL_MINIMUM (-32768) */
    0x26, 0xFF, 0x7F,              /*     LOGICAL_MAXIMUM (32767) */
    0x75, 0x10,                    /*     REPORT_SIZE (16) */
    0x95, 0x04,                    /*     REPORT_COUNT (4) */
    0x81, 0x02,                    /*     INPUT (Data,Var,Abs) */
    /* 2 8bit Axis */
    0x09, 0x32,                    /*     USAGE (Z) */
    0x09, 0x35,                    /*     USAGE (Rz) */
    0x15, 0x80,                    /*     LOGICAL_MINIMUM (-128) */
    0x25, 0x7F,                    /*     LOGICAL_MAXIMUM (127) */
    0x75, 0x08,                    /*     REPORT_SIZE (8) */
    0x95, 0x02,                    /*     REPORT_COUNT (2) */
    0x81, 0x02,                    /*     INPUT (Data,Var,Abs) */
    0xc0,                          /*   END_COLLECTION */
    /* 2 Hat Switches */
    0x05, 0x01,                    /*   USAGE_PAGE (Generic Desktop) */
    0x09, 0x39,                    /*   USAGE (Hat switch) */
    0x09, 0x39,                    /*   USAGE (Hat switch) */
    0x15, 0x01,                    /*   LOGICAL_MINIMUM (1) */
    0x25, 0x08,                    /*   LOGICAL_MAXIMUM (8) */
    0x95, 0x02,                    /*   REPORT_COUNT (2) */
    0x75, 0x04,                    /*   REPORT_SIZE (4) */
    0x81, 0x02,                    /*   INPUT (Data,Var,Abs) */
    0xc0                           /* END_COLLECTION */
};

// HID Report Descriptor for Boot Keyboard (identical to V1)
static const uint8_t _hidReportDescriptorKeyboard[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x85, 0x03,                    //   REPORT_ID (3)

    // Modifiers
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)

    // Reserved
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)

    // LEDs
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

    // LED padding
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs)

    // Key codes
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)

    0xc0                           // END_COLLECTION
};

// HID Report Descriptor for Consumer Control (identical to V1)
static const uint8_t _hidReportDescriptorConsumer[] = {
    0x05, 0x0c,                    // USAGE_PAGE (Consumer Devices)
    0x09, 0x01,                    // USAGE (Consumer Control)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x85, 0x02,                    //   REPORT_ID (2)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x03,              //   LOGICAL_MAXIMUM (1023)
    0x19, 0x00,                    //   USAGE_MINIMUM (Unassigned)
    0x2A, 0xFF, 0x03,              //   USAGE_MAXIMUM (0x03FF)
    0x75, 0x10,                    //   REPORT_SIZE (16)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};

// Combined descriptor for TinyUSB composite device
static const uint8_t _combinedReportDescriptor[] = {
    // Include all three descriptors sequentially
    // Gamepad (Report ID 1)
    0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0x85, 0x01,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x20, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x20, 0x81, 0x02,
    0x05, 0x01, 0xa1, 0x00, 0x09, 0x30, 0x09, 0x31,
    0x09, 0x33, 0x09, 0x34, 0x16, 0x00, 0x80, 0x26,
    0xFF, 0x7F, 0x75, 0x10, 0x95, 0x04, 0x81, 0x02,
    0x09, 0x32, 0x09, 0x35, 0x15, 0x80, 0x25, 0x7F,
    0x75, 0x08, 0x95, 0x02, 0x81, 0x02, 0xc0,
    0x05, 0x01, 0x09, 0x39, 0x09, 0x39, 0x15, 0x01,
    0x25, 0x08, 0x95, 0x02, 0x75, 0x04, 0x81, 0x02,
    0xc0,
    // Consumer Control (Report ID 2)
    0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x02,
    0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A,
    0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00,
    0xc0,
    // Keyboard (Report ID 3)
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x03,
    0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x03,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01,
    0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x03,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xc0
};

static USBHID HID;

// =====================================================================
// MinimalGamepad Implementation (TinyUSB)
// =====================================================================

MinimalGamepad::MinimalGamepad() : _hid(), _ready(false) {
}

void MinimalGamepad::begin() {
    memset(&_report, 0, sizeof(_report));
    _ready = true;
}

uint16_t MinimalGamepad::_onGetDescriptor(uint8_t* buffer) {
    memcpy(buffer, _combinedReportDescriptor, sizeof(_combinedReportDescriptor));
    return sizeof(_combinedReportDescriptor);
}

void MinimalGamepad::xAxis(int16_t value) {
    _report.xAxis = value;
    write();
}

void MinimalGamepad::yAxis(int16_t value) {
    _report.yAxis = value;
    write();
}

void MinimalGamepad::zAxis(int8_t value) {
    _report.zAxis = value;
    write();
}

void MinimalGamepad::press(uint8_t button) {
    if (button >= 1 && button <= 32) {
        _report.buttons |= (1UL << (button - 1));
        write();
    }
}

void MinimalGamepad::release(uint8_t button) {
    if (button >= 1 && button <= 32) {
        _report.buttons &= ~(1UL << (button - 1));
        write();
    }
}

void MinimalGamepad::write() {
    if (!_ready) return;
    HID.SendReport(1, &_report, sizeof(_report));
}

// =====================================================================
// BootKeyboardClass Implementation (TinyUSB)
// =====================================================================

BootKeyboardClass::BootKeyboardClass() {
    memset(_keyReport, 0, sizeof(_keyReport));
}

void BootKeyboardClass::begin() {
    memset(_keyReport, 0, sizeof(_keyReport));
}

void BootKeyboardClass::press(uint8_t key) {
    // Handle modifier keys
    if (key >= 0xE0 && key <= 0xE7) {
        _keyReport[0] |= (1 << (key - 0xE0));
    } else {
        // Check if key is already pressed
        for (uint8_t i = 2; i < 8; i++) {
            if (_keyReport[i] == key) {
                return;
            }
        }
        // Find empty slot for regular key
        for (uint8_t i = 2; i < 8; i++) {
            if (_keyReport[i] == 0) {
                _keyReport[i] = key;
                break;
            }
        }
    }
    sendReport();
}

void BootKeyboardClass::release(uint8_t key) {
    // Handle modifier keys
    if (key >= 0xE0 && key <= 0xE7) {
        _keyReport[0] &= ~(1 << (key - 0xE0));
    } else {
        // Remove regular key
        for (uint8_t i = 2; i < 8; i++) {
            if (_keyReport[i] == key) {
                _keyReport[i] = 0;
                break;
            }
        }
    }
    sendReport();
}

void BootKeyboardClass::sendReport() {
    HID.SendReport(3, _keyReport, sizeof(_keyReport));
}

// =====================================================================
// SingleConsumerClass Implementation (TinyUSB)
// =====================================================================

SingleConsumerClass::SingleConsumerClass() : _consumerReport(0) {
}

void SingleConsumerClass::begin() {
    _consumerReport = 0;
}

void SingleConsumerClass::press(uint16_t key) {
    _consumerReport = key;
    uint8_t report[2] = {(uint8_t)(_consumerReport & 0xFF), (uint8_t)(_consumerReport >> 8)};
    HID.SendReport(2, report, sizeof(report));
}

void SingleConsumerClass::release(uint16_t key) {
    if (_consumerReport == key) {
        _consumerReport = 0;
        uint8_t report[2] = {0, 0};
        HID.SendReport(2, report, sizeof(report));
    }
}

// =====================================================================
// Global instances
// =====================================================================

MinimalGamepad Gamepad1;
BootKeyboardClass BootKeyboard;
SingleConsumerClass SingleConsumer;

void usbHidSetup() {
    HID.addDevice(&Gamepad1, sizeof(_combinedReportDescriptor));
    USB.VID(0x0E8F);
    USB.PID(0x9207);
    USB.productName("PinOne V2");
    USB.manufacturerName("PinOne");
}

void usbHidStart() {
    HID.begin();
    Gamepad1.begin();
    BootKeyboard.begin();
    SingleConsumer.begin();
}
