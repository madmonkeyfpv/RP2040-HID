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

#define AXIS_MAX      4095
#define AXIS_CENTER   2048
#define BUTTON_COUNT  32
#define HAT_CENTERED  8
#define REPORT_ID     3

struct EncoderConfig { uint8_t a, b; };  // input numbers, 1-based

struct Debounce {
  bool pressed;
  uint8_t count;
};

struct Encoder {
  uint8_t prev;        // last AB state
  int8_t steps;        // quadrature steps since last detent
  uint8_t pending[2];  // queued detents: [0] = towards A, [1] = towards B
  int8_t active;       // direction currently being pulsed, -1 = none
  uint16_t timerMs;    // time left in current pulse phase
  bool released;       // true during the gap after a pulse
};

// Same layout as the RP2040 firmware and a Leo Bodnar BU0836X: 8 x 12-bit
// axes, one 8-way hat and 32 buttons.
struct __attribute__((packed)) JoystickReport {
  uint16_t axis[AXIS_COUNT];  // 0..AXIS_MAX
  uint8_t hat;                // 0..7 clockwise from up, HAT_CENTERED = released
  uint32_t buttons;           // bit 0 = button 1
};
