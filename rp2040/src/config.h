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
//
//  Need the full BU0836X count (8 axes, 32 buttons)? Turn on the MCP3208 ADC
//  and/or the 74HC165 shift registers below. The pins they use are taken out
//  of the button list automatically.
// =============================================================================
#pragma once

// ---- Pin layout -------------------------------------------------------------

#if defined(WAVESHARE_RP2040_ZERO)
// Edge pins GP0-GP15 and bottom pads GP17-GP25. GP16 drives the RGB LED.
#define DIGITAL_INPUT_PINS  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, \
                              17, 18, 19, 20, 21, 22, 23, 24, 25 }
#define ANALOG_INPUT_PINS   { 26, 27, 28, 29 }
#define DEFAULT_MCP3208_MISO 4
#define DEFAULT_MCP3208_CS   5
#define DEFAULT_MCP3208_SCK  6
#define DEFAULT_MCP3208_MOSI 7
#define DEFAULT_SR_DATA      8
#define DEFAULT_SR_CLK       9
#define DEFAULT_SR_LOAD      10

#else
// Raspberry Pi Pico, Pico W, Pico 2, Pico 2 W, and the default for other
// boards. GP23-GP25 and GP29 are used on the board itself.
#define DIGITAL_INPUT_PINS  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, \
                              16, 17, 18, 19, 20, 21, 22 }
#define ANALOG_INPUT_PINS   { 26, 27, 28 }
#define DEFAULT_MCP3208_MISO 16
#define DEFAULT_MCP3208_CS   17
#define DEFAULT_MCP3208_SCK  18
#define DEFAULT_MCP3208_MOSI 19
#define DEFAULT_SR_DATA      20
#define DEFAULT_SR_CLK       21
#define DEFAULT_SR_LOAD      22
#endif

// ---- MCP3208 external ADC: 8 more 12-bit axes ---------------------------------
// Power the MCP3208 from 3V3 (VDD and VREF), AGND/DGND to GND, 100 nF across
// VDD-GND, CLK/DOUT/DIN/CS to the pins below, pot wipers to CH0-CH7.
// Its channels become the first axes (CH0 = X, CH1 = Y, ...); the board's own
// ADC pins fill any axes left over, then become extra digital inputs.
// An MCP3204 works too: set MCP3208_CHANNELS to 4.
#define MCP3208_ENABLED     0
#define MCP3208_CHANNELS    8
// MISO, SCK and MOSI must be pins of one hardware SPI block: MISO = GPn with
// n%4 == 0, SCK n%4 == 2, MOSI n%4 == 3 (the build checks this). CS can be any.
#define MCP3208_MISO_PIN    DEFAULT_MCP3208_MISO    // to chip DOUT
#define MCP3208_CS_PIN      DEFAULT_MCP3208_CS      // to chip CS/SHDN
#define MCP3208_SCK_PIN     DEFAULT_MCP3208_SCK     // to chip CLK
#define MCP3208_MOSI_PIN    DEFAULT_MCP3208_MOSI    // to chip DIN

// ---- 74HC165 shift registers: 8 buttons per chip ------------------------------
// Chain the chips: chip 1's QH goes to DATA, chip 2's QH to chip 1's SER, and
// so on; tie the last chip's SER to 3V3. All chips share CLK and SH/LD (LOAD);
// tie CLK INH to GND. Power them from 3V3 (not 5 V) and give every input a
// 10k pull-up to 3V3, with the switch to GND.
// Shift register inputs come first: chip 1's A..H are inputs 1-8, chip 2's
// are 9-16, ... The board's own pins follow (they can still be used for the
// hat or encoders once all 32 buttons are taken).
#define SHIFT_REG_ENABLED   0
#define SHIFT_REG_CHIPS     4
#define SHIFT_REG_DATA_PIN  DEFAULT_SR_DATA         // to chip 1 QH
#define SHIFT_REG_CLK_PIN   DEFAULT_SR_CLK          // to every CLK
#define SHIFT_REG_LOAD_PIN  DEFAULT_SR_LOAD         // to every SH/LD

// ---- Optional input modes (off by default, as on a fresh BU0836X) ------------
// These match the options in Leo Bodnar's configuration utility. They refer
// to INPUT numbers (= button numbers), not GPIOs. Input 1 is the first entry
// of DIGITAL_INPUT_PINS, or shift register input 1 when those are enabled.

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
