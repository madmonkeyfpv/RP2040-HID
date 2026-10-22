// =============================================================================
//  RP2040-HID configuration
//
//  Works like a Leo Bodnar BU0836X: every usable pin is an input, with no
//  wiring to configure.
//    - ADC-capable pins are analog axes (X, Y, Z, Rx, Ry, Rz, Slider, Dial in
//      pin order). Wire a pot's ends to 3V3 and AGND, wiper to the pin.
//    - Every other pin is a button. Wire a switch between the pin and GND
//      (internal pull-ups, no resistors needed). Input N = joystick button N.
//
//  The pin lists are picked automatically from the board you build for
//  (-DPICO_BOARD=...). Pin numbers are GPIO numbers ("GPxx").
// =============================================================================
#pragma once

// ---- Pin layout -------------------------------------------------------------

#if defined(WAVESHARE_RP2040_ZERO)
// Edge pins GP0-GP15 and bottom pads GP17-GP25. GP16 drives the RGB LED.
#define DIGITAL_INPUT_PINS  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, \
                              17, 18, 19, 20, 21, 22, 23, 24, 25 }
#define ANALOG_INPUT_PINS   { 26, 27, 28, 29 }

#else
// Raspberry Pi Pico, Pico W, Pico 2, Pico 2 W, and the default for other
// boards. GP23-GP25 and GP29 are used on the board itself.
#define DIGITAL_INPUT_PINS  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, \
                              16, 17, 18, 19, 20, 21, 22 }
#define ANALOG_INPUT_PINS   { 26, 27, 28 }
#endif

// ---- Optional input modes (off by default, as on a fresh BU0836X) ------------
// These match the options in Leo Bodnar's configuration utility. They refer
// to INPUT numbers (1 = first entry of DIGITAL_INPUT_PINS), not GPIOs.

// Rotary encoders: pairs of inputs {A, B}. The two buttons then give a short
// press per detent (A's button = one direction, B's = the other).
//   #define ENCODER_INPUTS  { {1, 2}, {3, 4} }
#define ENCODER_INPUTS      {}
// Quadrature steps per detent: 4 for most encoders (e.g. EC11), 2 or 1 for some.
#define ENCODER_STEPS_PER_DETENT  4
// How long each encoder "press" lasts, in milliseconds.
#define ENCODER_PULSE_MS          30

// Hat switch (POV): four inputs for up, right, down, left. Those inputs then
// drive the hat instead of their buttons.
//   #define HAT_INPUTS      { 1, 2, 3, 4 }

// Invert axes: bit 0 = X, bit 1 = Y, ... bit 7 = Dial. e.g. 0x03 flips X and Y.
#define AXIS_INVERT         0x00

// ---- Tuning -----------------------------------------------------------------

// Axis smoothing: 0 = none, higher = smoother but laggier (max 7).
#define AXIS_SMOOTHING      3
// Readings within this many counts (of 4095) of either end snap to the end.
#define AXIS_DEADZONE_EDGE  16
// Switch state must be stable for this many milliseconds to register.
#define DEBOUNCE_MS         5
// Light the on-board LED when the PC has enumerated the device.
#define STATUS_LED          1

// ---- USB identity -----------------------------------------------------------
// 0x1209/0x0001 is the pid.codes test PID, fine for personal use. Each board
// reports its unique flash ID as its serial number, so several boards on one
// PC stay separate.
#define USB_VID             0x1209
#define USB_PID             0x0001
#define USB_MANUFACTURER    "RP2040-HID"
#define USB_PRODUCT         "RP2040 Joystick"
