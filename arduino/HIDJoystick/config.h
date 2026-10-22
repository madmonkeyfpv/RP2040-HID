// =============================================================================
//  HIDJoystick configuration (ATmega32U4: Arduino Leonardo, Micro, Pro Micro)
//
//  Pin numbers are Arduino pin numbers (as printed on the board; A0 etc. for
//  analog pins). Buttons, matrix columns, hat and encoder pins use the internal
//  pull-up and are ACTIVE LOW: wire each switch between the pin and GND.
//
//  Joystick button numbers are assigned automatically in this order:
//    1. DIRECT_BUTTON_PINS   (in the order listed)
//    2. Button matrix        (row by row, column by column)
//    3. Rotary encoders      (each encoder uses two buttons: CCW then CW)
//  Anything beyond 32 buttons is ignored.
// =============================================================================
#pragma once

// ---- Direct buttons ---------------------------------------------------------
// Default: every free digital pin on a Pro Micro (also valid on a Leonardo).
// Leave the list empty ( {} ) to disable.
#define DIRECT_BUTTON_PINS  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, 16 }

// ---- Button matrix ----------------------------------------------------------
// R rows x C columns gives R*C buttons from R+C pins, like a Leo Bodnar
// BU0836. Put a diode in series with each switch (cathode towards the row) to
// avoid ghosting when 3+ buttons are held. Leave both empty to disable.
#define MATRIX_ROW_PINS     {}
#define MATRIX_COL_PINS     {}

// ---- Hat switch (POV) -------------------------------------------------------
// Four pins for up, right, down, left. Uncomment to enable (and remove those
// pins from DIRECT_BUTTON_PINS).
// #define HAT_PINS         { 6, 7, 8, 9 }

// ---- Rotary encoders --------------------------------------------------------
// Pairs of {A, B} pins. Each detent gives a short press on a joystick button.
// Uncomment / edit to enable (and remove those pins from DIRECT_BUTTON_PINS).
#define ENCODER_PINS        {}
// #define ENCODER_PINS     { {14, 15} }
#define ENCODER_STEPS_PER_DETENT  4
#define ENCODER_PULSE_MS          30

// ---- Analog axes ------------------------------------------------------------
// The 32U4 has up to 12 analog inputs (A0-A5 on Leonardo headers, A0-A3 on a
// Pro Micro; A6-A11 share digital pins 4, 6, 8, 9, 10, 12). Wire each pot's
// ends to VCC and GND, wiper to the analog pin.
//
// Each entry is { pin, hid_axis, inverted }. hid_axis is one of
// AXIS_X, AXIS_Y, AXIS_Z, AXIS_RX, AXIS_RY, AXIS_RZ, AXIS_SLIDER, AXIS_DIAL.
// Axes that are not listed report their centre value.
#define ANALOG_AXES         { {A0, AXIS_X, false}, {A1, AXIS_Y, false}, {A2, AXIS_Z, false}, {A3, AXIS_RX, false} }
// Smoothing: 0 = none, higher = smoother but laggier (max 7).
#define AXIS_SMOOTHING      2
// Readings within this many counts (of 4095) of either end snap to the end.
#define AXIS_DEADZONE_EDGE  16

// ---- Misc -------------------------------------------------------------------
#define DEBOUNCE_MS         5
