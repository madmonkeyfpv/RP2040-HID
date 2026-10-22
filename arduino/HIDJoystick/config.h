// =============================================================================
//  HIDJoystick configuration (ATmega32U4: Arduino Leonardo, Micro, Pro Micro)
//
//  Works like a Leo Bodnar BU0836X: every usable pin is an input, with no
//  wiring to configure.
//    - Analog pins (A0, A1, ...) are axes: X, Y, Z, Rx, Ry, Rz in pin order.
//      Wire a pot's ends to VCC and GND, wiper to the pin.
//    - Every other pin is a button. Wire a switch between the pin and GND
//      (internal pull-ups, no resistors needed). Input N = joystick button N.
// =============================================================================
#pragma once

// ---- Board ------------------------------------------------------------------
// A Pro Micro is usually uploaded as "Arduino Leonardo", so it can't be
// detected. If you have one, uncomment this line (not needed if you use the
// SparkFun board package and select "SparkFun Pro Micro").
// #define BOARD_PRO_MICRO

#if defined(BOARD_PRO_MICRO) || defined(ARDUINO_AVR_PROMICRO)
// Pro Micro: 14 buttons, 4 axes.
#define DIGITAL_INPUT_PINS  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 14, 15 }
#define ANALOG_INPUT_PINS   { A0, A1, A2, A3 }
#else
// Leonardo and Micro: D0-D13 plus MISO (14), SCK (15), MOSI (16), which are
// on the ICSP header of a Leonardo. 17 buttons, 6 axes.
#define DIGITAL_INPUT_PINS  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }
#define ANALOG_INPUT_PINS   { A0, A1, A2, A3, A4, A5 }
#endif

// ---- Optional input modes (off by default, as on a fresh BU0836X) ------------
// These match the options in Leo Bodnar's configuration utility. They refer
// to INPUT numbers (1 = first entry of DIGITAL_INPUT_PINS), not pins.

// Rotary encoders: pairs of inputs {A, B}. The two buttons then give a short
// press per detent (A's button = one direction, B's = the other).
//   #define ENCODER_INPUTS  { {1, 2}, {3, 4} }
#define ENCODER_INPUTS      {}
#define ENCODER_STEPS_PER_DETENT  4
#define ENCODER_PULSE_MS          30

// Hat switch (POV): four inputs for up, right, down, left. Those inputs then
// drive the hat instead of their buttons.
//   #define HAT_INPUTS      { 1, 2, 3, 4 }

// Invert axes: bit 0 = X, bit 1 = Y, ... e.g. 0x03 flips X and Y.
#define AXIS_INVERT         0x00

// ---- Tuning -----------------------------------------------------------------
// Axis smoothing: 0 = none, higher = smoother but laggier (max 7).
#define AXIS_SMOOTHING      2
// Readings within this many counts (of 4095) of either end snap to the end.
#define AXIS_DEADZONE_EDGE  16
// Switch state must be stable for this many milliseconds to register.
#define DEBOUNCE_MS         5
