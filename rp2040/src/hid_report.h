#pragma once

#include <stdint.h>

// HID axis slots, in report order.
enum {
    AXIS_X = 0,
    AXIS_Y,
    AXIS_Z,
    AXIS_RX,
    AXIS_RY,
    AXIS_RZ,
    AXIS_SLIDER,
    AXIS_DIAL,
    AXIS_COUNT
};

#define AXIS_MAX        4095
#define AXIS_CENTER     2048
#define BUTTON_COUNT    32
#define HAT_CENTERED    8

// Same layout as a Leo Bodnar BU0836X: 8 x 12-bit axes, one 8-way hat and
// 32 buttons. The descriptor is fixed so Windows never sees it change.
typedef struct __attribute__((packed)) {
    uint16_t axis[AXIS_COUNT];  // 0..AXIS_MAX
    uint8_t  hat;               // 0..7 clockwise from up, HAT_CENTERED = released
    uint32_t buttons;           // bit 0 = button 1
} joystick_report_t;

#define JOYSTICK_REPORT_DESCRIPTOR \
    0x05, 0x01,             /* Usage Page (Generic Desktop)           */ \
    0x09, 0x04,             /* Usage (Joystick)                       */ \
    0xA1, 0x01,             /* Collection (Application)               */ \
    /* 8 axes, 16-bit fields holding 0..4095 */                          \
    0x09, 0x30,             /*   Usage (X)                            */ \
    0x09, 0x31,             /*   Usage (Y)                            */ \
    0x09, 0x32,             /*   Usage (Z)                            */ \
    0x09, 0x33,             /*   Usage (Rx)                           */ \
    0x09, 0x34,             /*   Usage (Ry)                           */ \
    0x09, 0x35,             /*   Usage (Rz)                           */ \
    0x09, 0x36,             /*   Usage (Slider)                       */ \
    0x09, 0x37,             /*   Usage (Dial)                         */ \
    0x15, 0x00,             /*   Logical Minimum (0)                  */ \
    0x26, 0xFF, 0x0F,       /*   Logical Maximum (4095)               */ \
    0x75, 0x10,             /*   Report Size (16)                     */ \
    0x95, 0x08,             /*   Report Count (8)                     */ \
    0x81, 0x02,             /*   Input (Data,Var,Abs)                 */ \
    /* Hat switch, 4 bits, out-of-range value = centred */               \
    0x09, 0x39,             /*   Usage (Hat switch)                   */ \
    0x15, 0x00,             /*   Logical Minimum (0)                  */ \
    0x25, 0x07,             /*   Logical Maximum (7)                  */ \
    0x35, 0x00,             /*   Physical Minimum (0)                 */ \
    0x46, 0x3B, 0x01,       /*   Physical Maximum (315)               */ \
    0x65, 0x14,             /*   Unit (Eng Rot: Degrees)              */ \
    0x75, 0x04,             /*   Report Size (4)                      */ \
    0x95, 0x01,             /*   Report Count (1)                     */ \
    0x81, 0x42,             /*   Input (Data,Var,Abs,Null State)      */ \
    0x65, 0x00,             /*   Unit (None)                          */ \
    0x45, 0x00,             /*   Physical Maximum (0)                 */ \
    0x75, 0x04,             /*   Report Size (4)                      */ \
    0x95, 0x01,             /*   Report Count (1)                     */ \
    0x81, 0x03,             /*   Input (Const) - padding              */ \
    /* 32 buttons */                                                     \
    0x05, 0x09,             /*   Usage Page (Button)                  */ \
    0x19, 0x01,             /*   Usage Minimum (1)                    */ \
    0x29, 0x20,             /*   Usage Maximum (32)                   */ \
    0x15, 0x00,             /*   Logical Minimum (0)                  */ \
    0x25, 0x01,             /*   Logical Maximum (1)                  */ \
    0x75, 0x01,             /*   Report Size (1)                      */ \
    0x95, 0x20,             /*   Report Count (32)                    */ \
    0x81, 0x02,             /*   Input (Data,Var,Abs)                 */ \
    0xC0                    /* End Collection                         */
