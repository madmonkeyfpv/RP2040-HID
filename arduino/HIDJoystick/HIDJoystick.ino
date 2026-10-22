// HIDJoystick: turns an ATmega32U4 Arduino (Leonardo, Micro, Pro Micro) into a
// native USB HID joystick, in the spirit of the Leo Bodnar BU0836 boards.
// Configure pins in config.h. Uses only the HID library built into the
// Arduino AVR core, so there is nothing extra to install.

#if !defined(USBCON)
#error "This sketch needs a board with native USB (ATmega32U4: Leonardo, Micro, Pro Micro). An Uno/Nano cannot act as a USB joystick."
#endif

#include <HID.h>

#include "joystick.h"
#include "config.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static const uint8_t directPins[] = DIRECT_BUTTON_PINS;
static const uint8_t rowPins[] = MATRIX_ROW_PINS;
static const uint8_t colPins[] = MATRIX_COL_PINS;
static const EncoderConfig encoderPins[] = ENCODER_PINS;
static const AxisConfig axisCfg[] = ANALOG_AXES;
#ifdef HAT_PINS
static const uint8_t hatPins[] = HAT_PINS;
#else
static const uint8_t hatPins[] = {};
#endif

#define NUM_DIRECT   COUNT_OF(directPins)
#define NUM_MATRIX   (COUNT_OF(rowPins) * COUNT_OF(colPins))
#define NUM_HAT      COUNT_OF(hatPins)
#define NUM_SWITCHES (NUM_DIRECT + NUM_MATRIX + NUM_HAT)
#define NUM_ENCODERS COUNT_OF(encoderPins)
#define NUM_AXES     COUNT_OF(axisCfg)

static_assert(NUM_HAT == 0 || NUM_HAT == 4, "HAT_PINS needs exactly 4 pins: up, right, down, left");

static const uint8_t reportDescriptor[] PROGMEM = {
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x04,        // Usage (Joystick)
  0xA1, 0x01,        // Collection (Application)
  0x85, REPORT_ID,   //   Report ID
  // 8 axes, 16-bit fields holding 0..4095
  0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x33,  // X, Y, Z, Rx
  0x09, 0x34, 0x09, 0x35, 0x09, 0x36, 0x09, 0x37,  // Ry, Rz, Slider, Dial
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x0F,  //   Logical Maximum (4095)
  0x75, 0x10,        //   Report Size (16)
  0x95, 0x08,        //   Report Count (8)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  // Hat switch, 4 bits, out-of-range value = centred
  0x09, 0x39,        //   Usage (Hat switch)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x07,        //   Logical Maximum (7)
  0x35, 0x00,        //   Physical Minimum (0)
  0x46, 0x3B, 0x01,  //   Physical Maximum (315)
  0x65, 0x14,        //   Unit (Eng Rot: Degrees)
  0x75, 0x04,        //   Report Size (4)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x42,        //   Input (Data,Var,Abs,Null State)
  0x65, 0x00,        //   Unit (None)
  0x45, 0x00,        //   Physical Maximum (0)
  0x75, 0x04,        //   Report Size (4)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x03,        //   Input (Const) - padding
  // 32 buttons
  0x05, 0x09,        //   Usage Page (Button)
  0x19, 0x01,        //   Usage Minimum (1)
  0x29, 0x20,        //   Usage Maximum (32)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x20,        //   Report Count (32)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0xC0               // End Collection
};

static Debounce switches[NUM_SWITCHES + 1];  // +1 keeps the array non-empty
static Encoder encoders[NUM_ENCODERS + 1];
static uint16_t axisFiltered[NUM_AXES + 1];  // value << 4 for sub-count precision
static JoystickReport report, lastReport;

// ---- Debounced switches -----------------------------------------------------

static void debounce(Debounce &d, bool raw) {
  if (raw == d.pressed) {
    d.count = 0;
  } else if (++d.count >= DEBOUNCE_MS) {
    d.pressed = raw;
    d.count = 0;
  }
}

static inline bool isLow(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

// Called once per millisecond.
static void scanSwitches() {
  size_t s = 0;

  for (size_t i = 0; i < NUM_DIRECT; i++)
    debounce(switches[s++], isLow(directPins[i]));

  for (size_t r = 0; r < COUNT_OF(rowPins); r++) {
    // Drive one row low at a time; idle rows float so they can't fight.
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], LOW);
    delayMicroseconds(5);
    for (size_t c = 0; c < COUNT_OF(colPins); c++)
      debounce(switches[s++], isLow(colPins[c]));
    pinMode(rowPins[r], INPUT_PULLUP);
  }

  for (size_t i = 0; i < NUM_HAT; i++)
    debounce(switches[s++], isLow(hatPins[i]));
}

// ---- Hat switch -------------------------------------------------------------

static uint8_t readHat() {
  if (NUM_HAT == 0) return HAT_CENTERED;
  // Index: bit0 up, bit1 right, bit2 down, bit3 left. Opposing directions
  // cancel out.
  static const uint8_t lut[16] = {
    HAT_CENTERED, 0, 2, 1, 4, HAT_CENTERED, 3, 2,
    6, 7, HAT_CENTERED, 0, 5, 6, 4, HAT_CENTERED,
  };
  const Debounce *h = &switches[NUM_DIRECT + NUM_MATRIX];
  uint8_t idx = (h[0].pressed << 0) | (h[1].pressed << 1) |
                (h[2].pressed << 2) | (h[3].pressed << 3);
  return lut[idx];
}

// ---- Rotary encoders --------------------------------------------------------

static uint8_t readEncoderAB(size_t i) {
  return (isLow(encoderPins[i].a) << 1) | isLow(encoderPins[i].b);
}

// Called as often as possible so fast spins aren't missed.
static void pollEncoders() {
  static const int8_t qdec[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
  for (size_t i = 0; i < NUM_ENCODERS; i++) {
    Encoder &e = encoders[i];
    uint8_t ab = readEncoderAB(i);
    if (ab == e.prev) continue;
    e.steps += qdec[(e.prev << 2) | ab];
    e.prev = ab;
    if (e.steps >= ENCODER_STEPS_PER_DETENT) {
      e.steps = 0;
      if (e.pending[1] < 16) e.pending[1]++;
    } else if (e.steps <= -ENCODER_STEPS_PER_DETENT) {
      e.steps = 0;
      if (e.pending[0] < 16) e.pending[0]++;
    }
  }
}

// Called once per millisecond. Turns queued detents into press/release pulses
// so every click is seen by the PC as a separate button press.
static void tickEncoders() {
  for (size_t i = 0; i < NUM_ENCODERS; i++) {
    Encoder &e = encoders[i];
    if (e.timerMs > 0 && --e.timerMs > 0) continue;
    if (e.active >= 0 && !e.released) {
      e.released = true;
      e.timerMs = ENCODER_PULSE_MS;
      continue;
    }
    e.active = -1;
    e.released = false;
    for (int dir = 0; dir < 2; dir++) {
      if (e.pending[dir]) {
        e.pending[dir]--;
        e.active = dir;
        e.timerMs = ENCODER_PULSE_MS;
        break;
      }
    }
  }
}

// ---- Analog axes ------------------------------------------------------------

static uint16_t readAxis(size_t i) {
  uint16_t sample = (uint16_t)analogRead(axisCfg[i].pin) << 6;  // 10-bit -> 12-bit, << 4
  pollEncoders();  // each analogRead takes ~100 us

  if (axisFiltered[i] == 0) axisFiltered[i] = sample;
  axisFiltered[i] += ((int32_t)sample - (int32_t)axisFiltered[i]) >> AXIS_SMOOTHING;

  // Stretch [edge, max-edge] to [0, max] so the ends are reachable.
  int32_t v = (axisFiltered[i] + 8) >> 4;
  v = (v - AXIS_DEADZONE_EDGE) * AXIS_MAX / (AXIS_MAX - 2 * AXIS_DEADZONE_EDGE);
  if (v < 0) v = 0;
  if (v > AXIS_MAX) v = AXIS_MAX;
  return axisCfg[i].inverted ? AXIS_MAX - v : v;
}

// ---- Report -----------------------------------------------------------------

static void buildReport(JoystickReport &r) {
  for (int a = 0; a < AXIS_COUNT; a++) r.axis[a] = AXIS_CENTER;
  for (size_t i = 0; i < NUM_AXES; i++) r.axis[axisCfg[i].axis] = readAxis(i);

  r.hat = readHat();

  uint32_t buttons = 0;
  unsigned b = 0;
  for (size_t i = 0; i < NUM_DIRECT + NUM_MATRIX; i++, b++)
    if (switches[i].pressed && b < BUTTON_COUNT) buttons |= 1UL << b;
  for (size_t i = 0; i < NUM_ENCODERS; i++, b += 2) {
    const Encoder &e = encoders[i];
    if (e.active >= 0 && !e.released && b + e.active < BUTTON_COUNT)
      buttons |= 1UL << (b + e.active);
  }
  r.buttons = buttons;
}

// ---- Arduino entry points ---------------------------------------------------

void setup() {
  static HIDSubDescriptor node(reportDescriptor, sizeof(reportDescriptor));
  HID().AppendDescriptor(&node);

  for (size_t i = 0; i < NUM_DIRECT; i++) pinMode(directPins[i], INPUT_PULLUP);
  for (size_t i = 0; i < COUNT_OF(rowPins); i++) pinMode(rowPins[i], INPUT_PULLUP);
  for (size_t i = 0; i < COUNT_OF(colPins); i++) pinMode(colPins[i], INPUT_PULLUP);
  for (size_t i = 0; i < NUM_HAT; i++) pinMode(hatPins[i], INPUT_PULLUP);
  for (size_t i = 0; i < NUM_ENCODERS; i++) {
    pinMode(encoderPins[i].a, INPUT_PULLUP);
    pinMode(encoderPins[i].b, INPUT_PULLUP);
    encoders[i].active = -1;
  }
  delayMicroseconds(100);  // let pull-ups settle
  for (size_t i = 0; i < NUM_ENCODERS; i++) encoders[i].prev = readEncoderAB(i);

  memset(&lastReport, 0xFF, sizeof(lastReport));
}

void loop() {
  static uint32_t lastTick = micros();

  pollEncoders();

  if (micros() - lastTick < 1000) return;
  lastTick += 1000;
  if (micros() - lastTick > 10000) lastTick = micros();  // don't try to catch up after a stall

  scanSwitches();
  tickEncoders();
  buildReport(report);

  if (memcmp(&report, &lastReport, sizeof(report)) != 0) {
    if (HID().SendReport(REPORT_ID, &report, sizeof(report)) >= 0) lastReport = report;
  }
}
