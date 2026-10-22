// =============================================================================
//  HIDJoystick configuration (ATmega32U4: Arduino Leonardo, Micro, Pro Micro)
//
//  Works like a Leo Bodnar BU0836X: every usable pin is an input, with no
//  wiring to configure.
//    - Analog pins (A0, A1, ...) are axes: X, Y, Z, Rx, Ry, Rz in pin order.
//      Wire a pot's ends to VCC and GND, wiper to the pin.
//    - Every other pin is a button. Wire a switch between the pin and GND
//      (internal pull-ups, no resistors needed). Input N = joystick button N.
//
//  Need the full BU0836X count (8 axes, 32 buttons)? Turn on the MCP3208 ADC
//  and/or the 74HC165 shift registers below. The pins they use are taken out
//  of the button list automatically.
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

// ---- MCP3208 external ADC: 8 more 12-bit axes ---------------------------------
// Uses the hardware SPI pins: MISO (14) to chip DOUT, SCK (15) to CLK, MOSI
// (16) to DIN, plus a chip-select pin of your choice. On a Leonardo those
// three are on the ICSP header. Power the chip from 5V (VDD and VREF),
// AGND/DGND to GND, 100 nF across VDD-GND, pot wipers to CH0-CH7.
// Its channels become the first axes (CH0 = X, CH1 = Y, ...); the board's own
// analog pins fill any axes left over, then become extra digital inputs.
// An MCP3204 works too: set MCP3208_CHANNELS to 4.
#define MCP3208_ENABLED     0
#define MCP3208_CHANNELS    8
#define MCP3208_CS_PIN      10

// ---- 74HC165 shift registers: 8 buttons per chip ------------------------------
// Chain the chips: chip 1's QH goes to DATA, chip 2's QH to chip 1's SER, and
// so on; tie the last chip's SER to VCC. All chips share CLK and SH/LD (LOAD);
// tie CLK INH to GND. Give every input a 10k pull-up to VCC, switch to GND.
// Shift register inputs come first: chip 1's A..H are inputs 1-8, chip 2's
// are 9-16, ... The board's own pins follow (they can still be used for the
// hat or encoders once all 32 buttons are taken).
#define SHIFT_REG_ENABLED   0
#define SHIFT_REG_CHIPS     4               // 1-4
#define SHIFT_REG_DATA_PIN  5               // to chip 1 QH
#define SHIFT_REG_CLK_PIN   6               // to every CLK
#define SHIFT_REG_LOAD_PIN  7               // to every SH/LD

// ---- Optional input modes (off by default, as on a fresh BU0836X) ------------
// These match the options in Leo Bodnar's configuration utility. They refer
// to INPUT numbers (= button numbers), not pins. Input 1 is the first entry
// of DIGITAL_INPUT_PINS, or shift register input 1 when those are enabled.

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
