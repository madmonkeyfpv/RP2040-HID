// HIDJoystick: turns an ATmega32U4 Arduino (Leonardo, Micro, Pro Micro) into a
// native USB HID joystick that behaves like a Leo Bodnar BU0836X: every analog
// pin is an axis, every other usable pin is a button, optionally extended with
// an MCP3208 ADC and 74HC165 shift registers. See config.h. Uses only
// libraries built into the Arduino AVR core, so there is nothing to install.

#if !defined(USBCON)
#error "This sketch needs a board with native USB (ATmega32U4: Leonardo, Micro, Pro Micro). An Uno/Nano cannot act as a USB joystick."
#endif

#include <HID.h>

#include "joystick.h"
#include "config.h"

#if MCP3208_ENABLED
#include <SPI.h>
#endif

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static const uint8_t digitalPins[] = DIGITAL_INPUT_PINS;
static const uint8_t analogPins[] = ANALOG_INPUT_PINS;
static const EncoderConfig encoderCfg[] = ENCODER_INPUTS;
#ifdef HAT_INPUTS
static const uint8_t hatInputs[] = HAT_INPUTS;
#else
static const uint8_t hatInputs[] = {};
#endif

#define NUM_ENCODERS COUNT_OF(encoderCfg)
#define NUM_HAT      COUNT_OF(hatInputs)

static_assert(COUNT_OF(analogPins) <= AXIS_COUNT, "At most 8 analog inputs");
static_assert(NUM_HAT == 0 || NUM_HAT == 4, "HAT_INPUTS needs exactly 4 inputs: up, right, down, left");

#if MCP3208_ENABLED
#define MCP_AXES MCP3208_CHANNELS
static_assert(MCP3208_CHANNELS >= 1 && MCP3208_CHANNELS <= 8, "MCP3208_CHANNELS must be 1-8");
#else
#define MCP_AXES 0
#endif

#if SHIFT_REG_ENABLED
#define SR_INPUTS (SHIFT_REG_CHIPS * 8)
static_assert(SHIFT_REG_CHIPS >= 1 && SHIFT_REG_CHIPS <= 4, "SHIFT_REG_CHIPS must be 1-4");
#else
#define SR_INPUTS 0
#endif

// Inputs: shift register bits first, then the board's own digital pins, then
// any analog pins left over once all 8 axes are taken.
#define MAX_INPUTS (SR_INPUTS + COUNT_OF(digitalPins) + COUNT_OF(analogPins))

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

// What each input currently does.
enum { ROLE_BUTTON, ROLE_ENCODER, ROLE_HAT };

static uint8_t inputPin[MAX_INPUTS + 1];  // Arduino pin for inputs >= SR_INPUTS
static uint8_t numInputs;
static uint8_t axisPin[AXIS_COUNT];       // Arduino pin for axes >= MCP_AXES
static uint8_t numAxes;
static bool encodersOnShiftRegs;

static uint8_t inputRole[MAX_INPUTS + 1];
static Debounce inputs[MAX_INPUTS + 1];
static Encoder encoders[NUM_ENCODERS + 1];
static uint16_t axisFiltered[AXIS_COUNT];  // value << 4 for sub-count precision
static JoystickReport report, lastReport;

static inline bool validInput(uint8_t n) {
  return n >= 1 && n <= numInputs;
}

static inline bool isLow(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

// ---- 74HC165 shift registers ------------------------------------------------

#if SHIFT_REG_ENABLED
static uint32_t srBits;  // bit n = shift register input n+1 pressed

// Direct port access: digitalRead/Write would make a 32-bit read take ~300 us.
static volatile uint8_t *srDataIn, *srClkOut, *srLoadOut;
static uint8_t srDataMask, srClkMask, srLoadMask;
#endif

static void readShiftRegisters() {
#if SHIFT_REG_ENABLED
  // The port writes below are read-modify-write; keep the USB interrupt (which
  // blinks the RX/TX LEDs) from changing the same port halfway through.
  uint8_t sreg = SREG;
  cli();

  // Pulse SH/LD low to latch all inputs; chip 1's H is then on QH.
  *srLoadOut &= ~srLoadMask;
  delayMicroseconds(1);
  *srLoadOut |= srLoadMask;

  uint32_t bits = 0;
  for (uint8_t chip = 0; chip < SHIFT_REG_CHIPS; chip++) {
    for (int8_t bit = 7; bit >= 0; bit--) {  // H first, A last
      if (!(*srDataIn & srDataMask)) bits |= 1UL << (chip * 8 + bit);
      *srClkOut |= srClkMask;
      *srClkOut &= ~srClkMask;
    }
  }
  SREG = sreg;
  srBits = bits;
#endif
}

// Raw (undebounced) state of input index i (0-based).
static inline bool readInput(uint8_t i) {
#if SHIFT_REG_ENABLED
  if (i < SR_INPUTS) return (srBits >> i) & 1;
#endif
  return isLow(inputPin[i]);
}

// ---- Debounced inputs -------------------------------------------------------

// Called once per millisecond.
static void scanInputs() {
  readShiftRegisters();
  for (uint8_t i = 0; i < numInputs; i++) {
    Debounce &d = inputs[i];
    bool raw = readInput(i);
    if (raw == d.pressed) {
      d.count = 0;
    } else if (++d.count >= DEBOUNCE_MS) {
      d.pressed = raw;
      d.count = 0;
    }
  }
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
  uint8_t idx = 0;
  for (size_t i = 0; i < NUM_HAT; i++)
    if (validInput(hatInputs[i]) && inputs[hatInputs[i] - 1].pressed) idx |= 1 << i;
  return lut[idx];
}

// ---- Rotary encoders --------------------------------------------------------

static inline bool encoderOk(size_t i) {
  return validInput(encoderCfg[i].a) && validInput(encoderCfg[i].b);
}

static uint8_t readEncoderAB(size_t i) {
  return (readInput(encoderCfg[i].a - 1) << 1) | readInput(encoderCfg[i].b - 1);
}

// Called as often as possible so fast spins aren't missed.
static void pollEncoders() {
  static const int8_t qdec[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
  if (encodersOnShiftRegs) readShiftRegisters();
  for (size_t i = 0; i < NUM_ENCODERS; i++) {
    if (!encoderOk(i)) continue;
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

#if MCP3208_ENABLED
static uint16_t readMCP3208(uint8_t ch) {
  // Start bit, single-ended, then the channel number; 12 result bits follow.
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));  // 2 MHz at 5 V
  digitalWrite(MCP3208_CS_PIN, LOW);
  SPI.transfer(0x06 | (ch >> 2));
  uint8_t hi = SPI.transfer((ch & 3) << 6);
  uint8_t lo = SPI.transfer(0);
  digitalWrite(MCP3208_CS_PIN, HIGH);
  SPI.endTransaction();
  return ((hi & 0x0F) << 8) | lo;
}
#endif

// Raw 12-bit sample for axis slot i.
static uint16_t sampleAxis(uint8_t i) {
#if MCP3208_ENABLED
  if (i < MCP_AXES) return readMCP3208(i);
#endif
  uint16_t v = analogRead(axisPin[i]) << 2;  // 10-bit -> 12-bit
  pollEncoders();  // each analogRead takes ~100 us
  return v;
}

static uint16_t readAxis(uint8_t i) {
  uint16_t sample = sampleAxis(i) << 4;

  if (axisFiltered[i] == 0) axisFiltered[i] = sample;
  axisFiltered[i] += ((int32_t)sample - (int32_t)axisFiltered[i]) >> AXIS_SMOOTHING;

  // Stretch [edge, max-edge] to [0, max] so the ends are reachable.
  int32_t v = (axisFiltered[i] + 8) >> 4;
  v = (v - AXIS_DEADZONE_EDGE) * AXIS_MAX / (AXIS_MAX - 2 * AXIS_DEADZONE_EDGE);
  if (v < 0) v = 0;
  if (v > AXIS_MAX) v = AXIS_MAX;
  return (AXIS_INVERT >> i) & 1 ? AXIS_MAX - v : v;
}

// ---- Report -----------------------------------------------------------------

static void buildReport(JoystickReport &r) {
  for (uint8_t a = 0; a < AXIS_COUNT; a++)
    r.axis[a] = a < numAxes ? readAxis(a) : AXIS_CENTER;

  r.hat = readHat();

  // Input N is button N, exactly like the BU0836X.
  uint32_t buttons = 0;
  for (uint8_t i = 0; i < numInputs && i < BUTTON_COUNT; i++)
    if (inputRole[i] == ROLE_BUTTON && inputs[i].pressed) buttons |= 1UL << i;
  for (size_t i = 0; i < NUM_ENCODERS; i++) {
    const Encoder &e = encoders[i];
    if (!encoderOk(i) || e.active < 0 || e.released) continue;
    uint8_t n = e.active ? encoderCfg[i].b : encoderCfg[i].a;
    if (n <= BUTTON_COUNT) buttons |= 1UL << (n - 1);
  }
  r.buttons = buttons;
}

// ---- Setup ------------------------------------------------------------------

// Pins taken by the expansion chips, so they aren't used as inputs.
static const uint8_t reservedPins[] = {
#if MCP3208_ENABLED
  MISO, SCK, MOSI, MCP3208_CS_PIN,
#endif
#if SHIFT_REG_ENABLED
  SHIFT_REG_DATA_PIN, SHIFT_REG_CLK_PIN, SHIFT_REG_LOAD_PIN,
#endif
};

static bool isReserved(uint8_t pin) {
  for (size_t i = 0; i < COUNT_OF(reservedPins); i++)
    if (reservedPins[i] == pin) return true;
  return false;
}

static void addInputPin(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  inputPin[numInputs++] = pin;
}

void setup() {
  static HIDSubDescriptor node(reportDescriptor, sizeof(reportDescriptor));
  HID().AppendDescriptor(&node);

#if SHIFT_REG_ENABLED
  pinMode(SHIFT_REG_DATA_PIN, INPUT);
  pinMode(SHIFT_REG_CLK_PIN, OUTPUT);
  digitalWrite(SHIFT_REG_CLK_PIN, LOW);
  pinMode(SHIFT_REG_LOAD_PIN, OUTPUT);
  digitalWrite(SHIFT_REG_LOAD_PIN, HIGH);
  srDataIn = portInputRegister(digitalPinToPort(SHIFT_REG_DATA_PIN));
  srDataMask = digitalPinToBitMask(SHIFT_REG_DATA_PIN);
  srClkOut = portOutputRegister(digitalPinToPort(SHIFT_REG_CLK_PIN));
  srClkMask = digitalPinToBitMask(SHIFT_REG_CLK_PIN);
  srLoadOut = portOutputRegister(digitalPinToPort(SHIFT_REG_LOAD_PIN));
  srLoadMask = digitalPinToBitMask(SHIFT_REG_LOAD_PIN);
#endif
#if MCP3208_ENABLED
  pinMode(MCP3208_CS_PIN, OUTPUT);
  digitalWrite(MCP3208_CS_PIN, HIGH);
  SPI.begin();
#endif

  numInputs = SR_INPUTS;
  numAxes = MCP_AXES;
  for (size_t i = 0; i < COUNT_OF(digitalPins); i++)
    if (!isReserved(digitalPins[i])) addInputPin(digitalPins[i]);
  for (size_t i = 0; i < COUNT_OF(analogPins); i++) {
    uint8_t pin = analogPins[i];
    if (isReserved(pin)) continue;
    if (numAxes < AXIS_COUNT) axisPin[numAxes++] = pin;
    else addInputPin(pin);  // no axis slot left: use it as a button
  }

  for (size_t i = 0; i < NUM_HAT; i++)
    if (validInput(hatInputs[i])) inputRole[hatInputs[i] - 1] = ROLE_HAT;
  for (size_t i = 0; i < NUM_ENCODERS; i++) {
    encoders[i].active = -1;
    if (!encoderOk(i)) continue;
    inputRole[encoderCfg[i].a - 1] = ROLE_ENCODER;
    inputRole[encoderCfg[i].b - 1] = ROLE_ENCODER;
    if (encoderCfg[i].a <= SR_INPUTS || encoderCfg[i].b <= SR_INPUTS) encodersOnShiftRegs = true;
  }
  delayMicroseconds(100);  // let pull-ups settle
  readShiftRegisters();
  for (size_t i = 0; i < NUM_ENCODERS; i++)
    if (encoderOk(i)) encoders[i].prev = readEncoderAB(i);

  memset(&lastReport, 0xFF, sizeof(lastReport));
}

void loop() {
  static uint32_t lastTick = micros();

  pollEncoders();

  if (micros() - lastTick < 1000) return;
  lastTick += 1000;
  if (micros() - lastTick > 10000) lastTick = micros();  // don't try to catch up after a stall

  scanInputs();
  tickEncoders();
  buildReport(report);

  if (memcmp(&report, &lastReport, sizeof(report)) != 0) {
    if (HID().SendReport(REPORT_ID, &report, sizeof(report)) >= 0) lastReport = report;
  }
}
