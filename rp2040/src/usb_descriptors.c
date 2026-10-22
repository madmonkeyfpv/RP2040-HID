#include <string.h>

#include "tusb.h"
#include "pico/unique_id.h"

#include "config.h"
#include "hid_report.h"

// ---- Device descriptor ------------------------------------------------------

static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

// ---- HID report descriptor --------------------------------------------------

static const uint8_t desc_hid_report[] = { JOYSTICK_REPORT_DESCRIPTOR };

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

// ---- Configuration descriptor -----------------------------------------------

enum { ITF_NUM_HID, ITF_NUM_TOTAL };

#define EPNUM_HID           0x81
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // 1 ms polling interval, like the Leo Bodnar boards.
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report),
                       EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

// ---- String descriptors -----------------------------------------------------

static const char *const string_desc[] = {
    [1] = USB_MANUFACTURER,
    [2] = USB_PRODUCT,
};

static uint16_t desc_str[33];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    const char *str;
    uint8_t len;

    if (index == 0) {
        desc_str[1] = 0x0409;  // English (US)
        len = 1;
    } else {
        if (index == 3) {
            // Unique per board, so several boards on one PC stay distinct.
            pico_get_unique_board_id_string(serial, sizeof(serial));
            str = serial;
        } else if (index < sizeof(string_desc) / sizeof(string_desc[0]) && string_desc[index]) {
            str = string_desc[index];
        } else {
            return NULL;
        }
        len = (uint8_t)strlen(str);
        if (len > 32) len = 32;
        for (uint8_t i = 0; i < len; i++) desc_str[1 + i] = str[i];
    }

    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
    return desc_str;
}
