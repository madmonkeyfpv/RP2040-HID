// =============================================================================
//  RP2040-HID configuration
//
//  Edit this file to match your wiring, then rebuild. Every pin number below is
//  a GPIO number (the "GPxx" label on the Pico), not a physical pin number.
//
//  All buttons, matrix columns, hat and encoder pins use the internal pull-up
//  resistor and are ACTIVE LOW: wire a switch between the GPIO and GND.
//
//  Joystick button numbers are assigned automatically in this order:
//    1. DIRECT_BUTTON_PINS   (in the order listed)
//    2. Button matrix        (row by row, column by column)
//    3. Rotary encoders      (each encoder uses two buttons: CCW then CW)
//  Anything beyond 32 buttons is ignored.
// =============================================================================
#pragma once

// ---- USB identity -----------------------------------------------------------
// 0x1209/0x0001 is the pid.codes test PID, fine for personal use. If you build
// several different boards for one PC, give each a different PID so Windows
// keeps their calibration apart.
#define USB_VID             0x1209
#define USB_PID             0x0001
#define USB_MANUFACTURER    "RP2040-HID"
#define USB_PRODUCT         "RP2040 Joystick"

// ---- Direct buttons ---------------------------------------------------------
// One switch per GPIO. Leave the list empty ( {} ) to disable.
#define DIRECT_BUTTON_PINS  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }

// ---- Button matrix ----------------------------------------------------------
// Like the Leo Bodnar BU0836 boards: R rows x C columns gives R*C buttons from
// R+C pins. Put a diode in series with every switch (anode on the column side,
// cathode on the row side) if you want more than two buttons held at once
// without ghosting. Leave both lists empty to disable.
#define MATRIX_ROW_PINS     {}
#define MATRIX_COL_PINS     {}

// ---- Hat switch (POV) -------------------------------------------------------
// Four GPIOs for up, right, down, left. Comment out to disable.
#define HAT_PINS            { 16, 17, 18, 19 }

// ---- Rotary encoders --------------------------------------------------------
// Pairs of {A, B} GPIOs. Each detent produces a short press on a joystick
// button (one for CCW, one for CW), which is how Leo Bodnar boards report
// encoders too. Leave empty ( {} ) to disable.
#define ENCODER_PINS        { {20, 21} }
// Quadrature steps per detent: 4 for most encoders (e.g. EC11), 2 or 1 for some.
#define ENCODER_STEPS_PER_DETENT  4
// How long each encoder "press" lasts, in milliseconds.
#define ENCODER_PULSE_MS          30

// ---- Analog axes ------------------------------------------------------------
// The RP2040 has ADC on GP26, GP27, GP28 (and GP29, which is wired to VSYS on
// a Raspberry Pi Pico). Connect a potentiometer's ends to 3V3 (ADC_VREF) and
// AGND, and the wiper to the ADC pin.
//
// Each entry is { gpio, hid_axis, inverted }. hid_axis is one of
// AXIS_X, AXIS_Y, AXIS_Z, AXIS_RX, AXIS_RY, AXIS_RZ, AXIS_SLIDER, AXIS_DIAL.
// Axes that are not listed report their centre value.
#define ANALOG_AXES         { {26, AXIS_X, false}, {27, AXIS_Y, false}, {28, AXIS_Z, false} }
// Smoothing: 0 = none, higher = smoother but laggier (max 7).
#define AXIS_SMOOTHING      3
// Readings within this many counts (of 4095) of either end snap to the end.
#define AXIS_DEADZONE_EDGE  16

// ---- Misc -------------------------------------------------------------------
// Switch state must be stable for this many milliseconds to register.
#define DEBOUNCE_MS         5
// Light the on-board LED when the PC has enumerated the device.
#define STATUS_LED          1
