#include "XInput.h"
#include "Globals.h"
#include "esp32-hal-tinyusb.h"
#include "device/usbd_pvt.h"
#include "USBVendor.h"

XInputGamepad XInputPad;

// =====================================================================
// Button bits of the XInput input report
// =====================================================================

#define XI_BTN_DPAD_UP    0x0001u
#define XI_BTN_DPAD_DOWN  0x0002u
#define XI_BTN_DPAD_LEFT  0x0004u
#define XI_BTN_DPAD_RIGHT 0x0008u
#define XI_BTN_START      0x0010u
#define XI_BTN_BACK       0x0020u
#define XI_BTN_LS         0x0040u
#define XI_BTN_RS         0x0080u
#define XI_BTN_LB         0x0100u
#define XI_BTN_RB         0x0200u
#define XI_BTN_GUIDE      0x0400u
#define XI_BTN_A          0x1000u
#define XI_BTN_B          0x2000u
#define XI_BTN_X          0x4000u
#define XI_BTN_Y          0x8000u

// config.bleButtonMap slots (set on the config tool's Game Controller page and
// shared with the Bluetooth profiles) to report bits. 0 = unmapped,
// 13/14 = triggers (handled separately).
static const uint16_t SLOT_TO_BUTTON[16] = {
    0,
    XI_BTN_A, XI_BTN_B, XI_BTN_X, XI_BTN_Y,        // 1-4
    XI_BTN_LB, XI_BTN_RB, XI_BTN_BACK, XI_BTN_START, // 5-8
    XI_BTN_DPAD_UP, XI_BTN_DPAD_DOWN,               // 9-10
    XI_BTN_DPAD_LEFT, XI_BTN_DPAD_RIGHT,            // 11-12
    0, 0,                                           // 13-14 triggers
    XI_BTN_GUIDE,                                   // 15
};
#define SLOT_LEFT_TRIGGER  13
#define SLOT_RIGHT_TRIGGER 14

// =====================================================================
// USB state, shared between the TinyUSB task and the input pass
// =====================================================================

static bool s_enabled = false;          // set once by setup(), before USB starts
static uint8_t s_itfNum = 0;
static volatile uint8_t s_epIn = 0;
static volatile uint8_t s_epOut = 0;
// Set on bus reset so the next update resends the report: a freshly
// enumerated host has none of our state
static volatile bool s_resend = true;
// Motor levels (0-255) from the host's last rumble packet
static volatile uint8_t s_rumbleLeft = 0;   // strong (low frequency) motor
static volatile uint8_t s_rumbleRight = 0;  // weak (high frequency) motor

CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t s_txBuf[32];
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t s_rxBuf[32];

// =====================================================================
// Microsoft OS 2.0 descriptor set: compatible ID XUSB10 on our interface
// =====================================================================

// The core's BOS descriptor (esp32-hal-tinyusb.c, always sent) announces a
// descriptor set of MS_OS_20_DESC_LEN (0xB2) bytes under vendor request 2, and
// Windows asks for exactly that length, so this set is built to the same size.
// The DeviceInterfaceGUIDs property fills the space after the compatible ID;
// xusb22 ignores it.
#define XINPUT_MSOS_LEN          0xB2
#define XINPUT_MSOS_VENDOR_CODE  2      // VENDOR_REQUEST_MICROSOFT in the core
#define XINPUT_MSOS_INDEX        7      // MS_OS_20_DESCRIPTOR_INDEX

static const char MSOS_PROPERTY_NAME[] = "DeviceInterfaceGUIDs";
static const char MSOS_PROPERTY_GUID[] = "{3B660E26-A847-48FA-8BAC-B7282747AFAA}";
static_assert(10 + 8 + 8 + 20 + 10 + sizeof(MSOS_PROPERTY_NAME) * 2 + (sizeof(MSOS_PROPERTY_GUID) + 1) * 2
                  == XINPUT_MSOS_LEN,
              "MS OS 2.0 descriptor set must match the length the core's BOS descriptor announces");

static uint8_t s_msos[XINPUT_MSOS_LEN];

static void buildMsOsDescriptor(uint8_t itf) {
    uint8_t* p = s_msos;
    auto put8  = [&](uint8_t v)  { *p++ = v; };
    auto put16 = [&](uint16_t v) { *p++ = v & 0xFF; *p++ = v >> 8; };
    auto put32 = [&](uint32_t v) { put16(v & 0xFFFF); put16(v >> 16); };
    // ASCII to UTF-16LE, count characters including the terminator
    auto putWide = [&](const char* s, size_t count) { for (size_t i = 0; i < count; i++) put16((uint8_t)s[i]); };

    // Set header: Windows 8.1 and later
    put16(0x000A); put16(MS_OS_20_SET_HEADER_DESCRIPTOR); put32(0x06030000); put16(XINPUT_MSOS_LEN);
    // Configuration subset
    put16(0x0008); put16(MS_OS_20_SUBSET_HEADER_CONFIGURATION); put8(0); put8(0);
    put16(XINPUT_MSOS_LEN - 0x0A);
    // Function subset: the XInput interface
    put16(0x0008); put16(MS_OS_20_SUBSET_HEADER_FUNCTION); put8(itf); put8(0);
    put16(XINPUT_MSOS_LEN - 0x0A - 0x08);
    // Compatible ID: Windows matches USB\MS_COMP_XUSB10 to its Xbox driver
    put16(0x0014); put16(MS_OS_20_FEATURE_COMPATBLE_ID);
    static const char compatId[8] = {'X', 'U', 'S', 'B', '1', '0', 0, 0};
    memcpy(p, compatId, 8); p += 8;
    memset(p, 0, 8); p += 8;  // sub-compatible ID
    // Registry property (REG_MULTI_SZ)
    put16(XINPUT_MSOS_LEN - 0x0A - 0x08 - 0x08 - 0x14); put16(MS_OS_20_FEATURE_REG_PROPERTY); put16(0x0007);
    put16(sizeof(MSOS_PROPERTY_NAME) * 2); putWide(MSOS_PROPERTY_NAME, sizeof(MSOS_PROPERTY_NAME));
    put16((sizeof(MSOS_PROPERTY_GUID) + 1) * 2); putWide(MSOS_PROPERTY_GUID, sizeof(MSOS_PROPERTY_GUID)); put16(0);
}

// Vendor control requests (every one that isn't WebUSB, which is off) reach the
// sketch only through a USBVendor object's onRequest() handler: the USB
// library always links its own tinyusb_vendor_control_request_cb(), which
// forwards to the one USBVendor instance. setup() creates that instance in
// XInput mode only; the normal mode keeps bcdUSB 2.0, so Windows never asks
// for the descriptor set there.
static bool vendorRequest(uint8_t rhport, uint8_t stage, arduino_usb_control_request_t const* arduinoRequest) {
    tusb_control_request_t const* request = (tusb_control_request_t const*)arduinoRequest;
    if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR ||
        request->bRequest != XINPUT_MSOS_VENDOR_CODE || request->wIndex != XINPUT_MSOS_INDEX ||
        request->bmRequestType_bit.direction != TUSB_DIR_IN) {
        return false;  // stall anything else, like the core does
    }
    if (stage != CONTROL_STAGE_SETUP) return true;
    return tud_control_xfer(rhport, request, (void*)s_msos, sizeof(s_msos));
}

// =====================================================================
// Configuration descriptor (Xbox 360 wired controller, interface 0 layout)
// =====================================================================

static uint16_t loadDescriptor(uint8_t* dst, uint8_t* itf) {
    uint8_t ep = tinyusb_get_free_duplex_endpoint();
    if (ep == 0) return 0;
    s_itfNum = *itf;
    buildMsOsDescriptor(s_itfNum);

    uint8_t desc[XINPUT_DESC_LEN] = {
        // Interface: 2 endpoints, class 0xFF, subclass 0x5D, protocol 0x01
        9, TUSB_DESC_INTERFACE, *itf, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x5D, 0x01, 0,
        // Vendor descriptor (0x21) the Xbox 360 controller carries; names the endpoints
        0x11, 0x21, 0x00, 0x01, 0x01, 0x25, (uint8_t)(0x80 | ep), 0x14, 0x00, 0x00, 0x00, 0x00,
        0x13, ep, 0x08, 0x00, 0x00,
        // Interrupt IN, 32 bytes, every 1 ms
        7, TUSB_DESC_ENDPOINT, (uint8_t)(0x80 | ep), TUSB_XFER_INTERRUPT, 32, 0, 1,
        // Interrupt OUT (rumble / LED), 32 bytes, every 8 ms
        7, TUSB_DESC_ENDPOINT, ep, TUSB_XFER_INTERRUPT, 32, 0, 8,
    };
    *itf += 1;
    memcpy(dst, desc, sizeof(desc));
    return sizeof(desc);
}

// =====================================================================
// TinyUSB class driver
// =====================================================================

static void xinputInit(void) {}

static bool xinputDeinit(void) {
    return true;
}

static void xinputReset(uint8_t rhport) {
    (void)rhport;
    s_epIn = 0;
    s_epOut = 0;
    s_rumbleLeft = 0;
    s_rumbleRight = 0;
    s_resend = true;
}

static uint16_t xinputOpen(uint8_t rhport, tusb_desc_interface_t const* desc, uint16_t maxLen) {
    if (desc->bInterfaceClass != TUSB_CLASS_VENDOR_SPECIFIC || desc->bInterfaceSubClass != 0x5D ||
        desc->bInterfaceProtocol != 0x01) {
        return 0;
    }
    uint16_t len = sizeof(tusb_desc_interface_t);
    uint8_t const* p = tu_desc_next(desc);
    uint8_t found = 0;
    // Endpoints, skipping the vendor descriptor in front of them
    while (found < desc->bNumEndpoints && len < maxLen) {
        if (tu_desc_type(p) == TUSB_DESC_INTERFACE) break;
        if (tu_desc_type(p) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const* ep = (tusb_desc_endpoint_t const*)p;
            if (!usbd_edpt_open(rhport, ep)) return 0;
            if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN) {
                s_epIn = ep->bEndpointAddress;
            } else {
                s_epOut = ep->bEndpointAddress;
            }
            found++;
        }
        len += tu_desc_len(p);
        p = tu_desc_next(p);
    }
    if (s_epOut) {
        usbd_edpt_xfer(rhport, s_epOut, s_rxBuf, sizeof(s_rxBuf), false);
    }
    return len;
}

static bool xinputControl(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    (void)rhport; (void)stage; (void)request;
    return false;  // no class requests; stall
}

static bool xinputXfer(uint8_t rhport, uint8_t epAddr, xfer_result_t result, uint32_t xferred) {
    if (epAddr == s_epOut) {
        // Rumble: 00 08 00 <strong motor> <weak motor> 00 00 00. LED packets (01 03 ..) are ignored.
        if (result == XFER_RESULT_SUCCESS && xferred >= 5 && s_rxBuf[0] == 0x00 && s_rxBuf[1] == 0x08) {
            s_rumbleLeft = s_rxBuf[3];
            s_rumbleRight = s_rxBuf[4];
        }
        usbd_edpt_xfer(rhport, s_epOut, s_rxBuf, sizeof(s_rxBuf), false);
    }
    return true;
}

static const usbd_class_driver_t s_driver = {
    "XINPUT",        // name
    xinputInit,      // init
    xinputDeinit,    // deinit
    xinputReset,     // reset
    xinputOpen,      // open
    xinputControl,   // control_xfer_cb
    xinputXfer,      // xfer_cb
    nullptr,         // xfer_isr
    nullptr,         // sof
};

// TinyUSB asks for application class drivers when the stack starts; they are
// tried before its own, so the vendor class driver never claims our interface.
extern "C" usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = s_enabled ? 1 : 0;
    return &s_driver;
}

// =====================================================================
// XInputGamepad
// =====================================================================

void XInputGamepad::setup() {
    s_enabled = true;
    // The XInput interface takes the core's vendor interface slot. The USBVendor
    // created next then only serves as the route for vendor control requests
    // (see vendorRequest()): its constructor makes it the request handler, and
    // its own attempt to add a vendor interface fails because the slot is
    // taken, so no extra interface or endpoint appears.
    tinyusb_enable_interface(USB_INTERFACE_VENDOR, XINPUT_DESC_LEN, loadDescriptor);
    static USBVendor* requests = new USBVendor();
    requests->onRequest(vendorRequest);
}

// XInput's +Y is up, the HID convention (and the Bluetooth sticks) +Y down, so
// the Y axes are flipped to keep nudge and plunger moving the same way
static int16_t flipAxis(int16_t v) {
    return v == INT16_MIN ? INT16_MAX : (int16_t)-v;
}

XInputGamepad::Report XInputGamepad::buildReport() {
    Report report;
    // Keyboard mode 1 (keyboard only): a button with a keyboard key sends just
    // the key and falls back to the controller otherwise, like the HID gamepad
    // (Buttons::sendActualButtonPress()). Mode 2 sends both.
    bool keyboardOnly = config.disableButtonPressWhenKeyboardEnabled == 1;

    for (uint8_t i = 0; i < 32; i++) {
        if (!config.processedButtonState[i]) continue;
        if (keyboardOnly && config.getButtonKeyboard(i) > 0) continue;
        uint8_t slot = config.bleButtonMap[i];
        if (slot == SLOT_LEFT_TRIGGER) {
            report.lt = 255;
        } else if (slot == SLOT_RIGHT_TRIGGER) {
            report.rt = 255;
        } else if (slot < sizeof(SLOT_TO_BUTTON) / sizeof(SLOT_TO_BUTTON[0])) {
            report.buttons |= SLOT_TO_BUTTON[slot];
        }
    }

    // Each signal goes on the stick axis chosen on the Game Controller page
    // (shared with Bluetooth: 0 = left X, 1 = left Y, 2 = right X, 3 = right Y);
    // the rest stay centered
    int16_t sticks[4] = {0, 0, 0, 0};
    sticks[config.bleNudgeXAxis % 4]  = accel.getXAxis();
    sticks[config.bleNudgeYAxis % 4]  = accel.getYAxis();
    sticks[config.blePlungerAxis % 4] = plunger.getAdjustedValue();
    report.lx = sticks[0];
    report.ly = flipAxis(sticks[1]);
    report.rx = sticks[2];
    report.ry = flipAxis(sticks[3]);
    return report;
}

bool XInputGamepad::send(const Report& report) {
    uint8_t ep = s_epIn;
    if (ep == 0 || !tud_ready()) return false;
    // Fails while the previous report is still in flight; the next pass retries
    if (!usbd_edpt_claim(0, ep)) return false;
    memcpy(s_txBuf, &report, sizeof(report));
    if (!usbd_edpt_xfer(0, ep, s_txBuf, sizeof(report), false)) {
        usbd_edpt_release(0, ep);
        return false;
    }
    return true;
}

void XInputGamepad::update() {
    // A host that goes away (unplugged, asleep) can't send a rumble-off, so stop
    // the outputs here
    if (!tud_ready()) {
        s_rumbleLeft = 0;
        s_rumbleRight = 0;
    }
    _rumble.apply(s_rumbleLeft, s_rumbleRight, 255);

    Report report = buildReport();
    bool resend = s_resend;
    if (!resend && memcmp(&report, &_sent, sizeof(report)) == 0) return;
    if (send(report)) {
        _sent = report;
        if (resend) s_resend = false;
    }
}
