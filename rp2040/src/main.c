// RP2040-HID: turns an RP2040 board into a native USB HID joystick that
// behaves like a Leo Bodnar BU0836X. Every ADC pin is an axis, every other
// usable pin is a button, optionally extended with an MCP3208 ADC and 74HC165
// shift registers. See config.h.

#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "tusb.h"

#include "hid_report.h"
#include "config.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

typedef struct { uint8_t a, b; } encoder_cfg_t;

static const uint8_t digital_pins[] = DIGITAL_INPUT_PINS;
static const uint8_t analog_pins[] = ANALOG_INPUT_PINS;
static const encoder_cfg_t encoder_cfg[] = ENCODER_INPUTS;
#ifdef HAT_INPUTS
static const uint8_t hat_inputs[] = HAT_INPUTS;
#else
static const uint8_t hat_inputs[] = {};
#endif

#define NUM_ENCODERS COUNT_OF(encoder_cfg)
#define NUM_HAT      COUNT_OF(hat_inputs)

_Static_assert(COUNT_OF(analog_pins) <= AXIS_COUNT, "At most 8 analog inputs");
_Static_assert(NUM_HAT == 0 || NUM_HAT == 4, "HAT_INPUTS needs exactly 4 inputs: up, right, down, left");

#if MCP3208_ENABLED
#define MCP_AXES MCP3208_CHANNELS
_Static_assert(MCP3208_CHANNELS >= 1 && MCP3208_CHANNELS <= 8, "MCP3208_CHANNELS must be 1-8");
_Static_assert(MCP3208_MISO_PIN % 4 == 0, "MCP3208_MISO_PIN must be an SPI RX pin (GP0, 4, 8, 12, 16, 20, 24, 28)");
_Static_assert(MCP3208_SCK_PIN % 4 == 2, "MCP3208_SCK_PIN must be an SPI SCK pin (GP2, 6, 10, 14, 18, 22, 26)");
_Static_assert(MCP3208_MOSI_PIN % 4 == 3, "MCP3208_MOSI_PIN must be an SPI TX pin (GP3, 7, 11, 15, 19, 23, 27)");
_Static_assert(((MCP3208_MISO_PIN >> 3) & 1) == ((MCP3208_SCK_PIN >> 3) & 1) &&
               ((MCP3208_MOSI_PIN >> 3) & 1) == ((MCP3208_SCK_PIN >> 3) & 1),
               "MCP3208 MISO, SCK and MOSI must belong to the same SPI block");
#define MCP_SPI (((MCP3208_SCK_PIN >> 3) & 1) ? spi1 : spi0)
#else
#define MCP_AXES 0
#endif

#if SHIFT_REG_ENABLED
#define SR_INPUTS (SHIFT_REG_CHIPS * 8)
_Static_assert(SHIFT_REG_CHIPS >= 1 && SHIFT_REG_CHIPS <= 8, "SHIFT_REG_CHIPS must be 1-8");
#else
#define SR_INPUTS 0
#endif

// Inputs: shift register bits first, then the board's own digital pins, then
// any ADC pins left over once all 8 axes are taken.
#define MAX_INPUTS (SR_INPUTS + COUNT_OF(digital_pins) + COUNT_OF(analog_pins))

static uint8_t input_pin[MAX_INPUTS + 1];  // GPIO for inputs >= SR_INPUTS
static size_t  num_inputs;
static uint8_t axis_pin[AXIS_COUNT];       // GPIO for axes >= MCP_AXES
static size_t  num_axes;
static uint64_t sr_bits;                   // bit n = shift register input n+1 pressed

// What each input currently does.
enum { ROLE_BUTTON, ROLE_ENCODER, ROLE_HAT };
static uint8_t input_role[MAX_INPUTS + 1];

static inline bool valid_input(uint8_t n) {
    return n >= 1 && n <= num_inputs;
}

static inline bool is_low(uint gpio) {
    return !gpio_get(gpio);
}

// ---- 74HC165 shift registers ------------------------------------------------

static void read_shift_registers(void) {
#if SHIFT_REG_ENABLED
    // Pulse SH/LD low to latch all inputs; chip 1's H is then on QH.
    gpio_put(SHIFT_REG_LOAD_PIN, 0);
    busy_wait_at_least_cycles(40);
    gpio_put(SHIFT_REG_LOAD_PIN, 1);
    busy_wait_at_least_cycles(40);

    uint64_t bits = 0;
    for (int chip = 0; chip < SHIFT_REG_CHIPS; chip++) {
        for (int bit = 7; bit >= 0; bit--) {  // H first, A last
            if (is_low(SHIFT_REG_DATA_PIN)) bits |= 1ull << (chip * 8 + bit);
            gpio_put(SHIFT_REG_CLK_PIN, 1);
            busy_wait_at_least_cycles(40);
            gpio_put(SHIFT_REG_CLK_PIN, 0);
            busy_wait_at_least_cycles(40);
        }
    }
    sr_bits = bits;
#endif
}

// Raw (undebounced) state of input index i (0-based).
static inline bool read_input(size_t i) {
    if (i < SR_INPUTS) return (sr_bits >> i) & 1;
    return is_low(input_pin[i]);
}

// ---- Debounced inputs -------------------------------------------------------

typedef struct {
    bool    pressed;
    uint8_t count;
} debounce_t;

static debounce_t inputs[MAX_INPUTS + 1];

// Called once per millisecond.
static void scan_inputs(void) {
    read_shift_registers();
    for (size_t i = 0; i < num_inputs; i++) {
        debounce_t *d = &inputs[i];
        bool raw = read_input(i);
        if (raw == d->pressed) {
            d->count = 0;
        } else if (++d->count >= DEBOUNCE_MS) {
            d->pressed = raw;
            d->count = 0;
        }
    }
}

// ---- Hat switch -------------------------------------------------------------

static uint8_t read_hat(void) {
    if (NUM_HAT == 0) return HAT_CENTERED;
    // Index: bit0 up, bit1 right, bit2 down, bit3 left. Opposing directions
    // cancel out.
    static const uint8_t lut[16] = {
        HAT_CENTERED, 0, 2, 1, 4, HAT_CENTERED, 3, 2,
        6, 7, HAT_CENTERED, 0, 5, 6, 4, HAT_CENTERED,
    };
    uint8_t idx = 0;
    for (size_t i = 0; i < NUM_HAT; i++)
        if (valid_input(hat_inputs[i]) && inputs[hat_inputs[i] - 1].pressed) idx |= 1u << i;
    return lut[idx];
}

// ---- Rotary encoders --------------------------------------------------------

typedef struct {
    uint8_t  prev;        // last AB state
    int8_t   steps;       // quadrature steps since last detent
    uint8_t  pending[2];  // queued detents: [0] = towards A, [1] = towards B
    int8_t   active;      // direction currently being pulsed, -1 = none
    uint16_t timer_ms;    // time left in current pulse phase
    bool     released;    // true during the gap after a pulse
} encoder_t;

static encoder_t encoders[NUM_ENCODERS + 1];
static bool encoders_on_shift_regs;

static inline bool encoder_ok(size_t i) {
    return valid_input(encoder_cfg[i].a) && valid_input(encoder_cfg[i].b);
}

static uint8_t read_encoder_ab(size_t i) {
    return (uint8_t)((read_input(encoder_cfg[i].a - 1) << 1) | read_input(encoder_cfg[i].b - 1));
}

// Called as often as possible so fast spins aren't missed.
static void poll_encoders(void) {
    static const int8_t qdec[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
    if (encoders_on_shift_regs) read_shift_registers();
    for (size_t i = 0; i < NUM_ENCODERS; i++) {
        if (!encoder_ok(i)) continue;
        encoder_t *e = &encoders[i];
        uint8_t ab = read_encoder_ab(i);
        if (ab == e->prev) continue;
        e->steps += qdec[(e->prev << 2) | ab];
        e->prev = ab;
        if (e->steps >= ENCODER_STEPS_PER_DETENT) {
            e->steps = 0;
            if (e->pending[1] < 16) e->pending[1]++;
        } else if (e->steps <= -ENCODER_STEPS_PER_DETENT) {
            e->steps = 0;
            if (e->pending[0] < 16) e->pending[0]++;
        }
    }
}

// Called once per millisecond. Turns queued detents into press/release pulses
// so every click is seen by the PC as a separate button press.
static void tick_encoders(void) {
    for (size_t i = 0; i < NUM_ENCODERS; i++) {
        encoder_t *e = &encoders[i];
        if (e->timer_ms > 0 && --e->timer_ms > 0) continue;
        if (e->active >= 0 && !e->released) {
            e->released = true;
            e->timer_ms = ENCODER_PULSE_MS;
            continue;
        }
        e->active = -1;
        e->released = false;
        for (int dir = 0; dir < 2; dir++) {
            if (e->pending[dir]) {
                e->pending[dir]--;
                e->active = (int8_t)dir;
                e->timer_ms = ENCODER_PULSE_MS;
                break;
            }
        }
    }
}

// ---- Analog axes ------------------------------------------------------------

#if MCP3208_ENABLED
static uint16_t read_mcp3208(uint8_t ch) {
    // Start bit, single-ended, then the channel number; 12 result bits follow.
    uint8_t tx[3] = { (uint8_t)(0x06 | (ch >> 2)), (uint8_t)((ch & 3) << 6), 0 };
    uint8_t rx[3];
    gpio_put(MCP3208_CS_PIN, 0);
    spi_write_read_blocking(MCP_SPI, tx, rx, 3);
    gpio_put(MCP3208_CS_PIN, 1);
    return (uint16_t)(((rx[1] & 0x0F) << 8) | rx[2]);
}
#endif

static uint32_t axis_filtered[AXIS_COUNT];  // value << 8 for sub-count precision

// Raw 12-bit sample for axis slot i.
static uint32_t sample_axis(size_t i) {
#if MCP3208_ENABLED
    if (i < MCP_AXES) return read_mcp3208((uint8_t)i);
#endif
    adc_select_input(axis_pin[i] - ADC_BASE_PIN);
    uint32_t sum = 0;
    for (int n = 0; n < 16; n++) sum += adc_read();
    return sum / 16;
}

static uint16_t read_axis(size_t i) {
    uint32_t sample = sample_axis(i) << 8;

    if (axis_filtered[i] == 0) axis_filtered[i] = sample;
    axis_filtered[i] += ((int32_t)sample - (int32_t)axis_filtered[i]) >> AXIS_SMOOTHING;

    // Stretch [edge, max-edge] to [0, max] so the ends are reachable.
    int32_t v = (int32_t)((axis_filtered[i] + 128) >> 8);
    v = (v - AXIS_DEADZONE_EDGE) * AXIS_MAX / (AXIS_MAX - 2 * AXIS_DEADZONE_EDGE);
    if (v < 0) v = 0;
    if (v > AXIS_MAX) v = AXIS_MAX;
    return (uint16_t)((AXIS_INVERT >> i) & 1 ? AXIS_MAX - v : v);
}

// ---- Report -----------------------------------------------------------------

static void build_report(joystick_report_t *r) {
    for (size_t a = 0; a < AXIS_COUNT; a++)
        r->axis[a] = a < num_axes ? read_axis(a) : AXIS_CENTER;

    r->hat = read_hat();

    // Input N is button N, exactly like the BU0836X.
    uint32_t buttons = 0;
    for (size_t i = 0; i < num_inputs && i < BUTTON_COUNT; i++)
        if (input_role[i] == ROLE_BUTTON && inputs[i].pressed) buttons |= 1u << i;
    for (size_t i = 0; i < NUM_ENCODERS; i++) {
        const encoder_t *e = &encoders[i];
        if (!encoder_ok(i) || e->active < 0 || e->released) continue;
        uint8_t n = e->active ? encoder_cfg[i].b : encoder_cfg[i].a;
        if (n <= BUTTON_COUNT) buttons |= 1u << (n - 1);
    }
    r->buttons = buttons;
}

static joystick_report_t report;

// ---- TinyUSB HID callbacks --------------------------------------------------

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type;
    uint16_t len = sizeof(report) < reqlen ? sizeof(report) : reqlen;
    memcpy(buffer, &report, len);
    return len;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

// ---- Setup ------------------------------------------------------------------

// Pins taken by the expansion chips, so they aren't used as inputs.
static const uint8_t reserved_pins[] = {
#if MCP3208_ENABLED
    MCP3208_MISO_PIN, MCP3208_CS_PIN, MCP3208_SCK_PIN, MCP3208_MOSI_PIN,
#endif
#if SHIFT_REG_ENABLED
    SHIFT_REG_DATA_PIN, SHIFT_REG_CLK_PIN, SHIFT_REG_LOAD_PIN,
#endif
};

static bool is_reserved(uint8_t pin) {
    for (size_t i = 0; i < COUNT_OF(reserved_pins); i++)
        if (reserved_pins[i] == pin) return true;
    return false;
}

static void init_input_pin(uint8_t pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    input_pin[num_inputs++] = pin;
}

static void init_hardware(void) {
#if SHIFT_REG_ENABLED
    gpio_init(SHIFT_REG_DATA_PIN);
    gpio_set_dir(SHIFT_REG_DATA_PIN, GPIO_IN);
    gpio_init(SHIFT_REG_CLK_PIN);
    gpio_set_dir(SHIFT_REG_CLK_PIN, GPIO_OUT);
    gpio_put(SHIFT_REG_CLK_PIN, 0);
    gpio_init(SHIFT_REG_LOAD_PIN);
    gpio_set_dir(SHIFT_REG_LOAD_PIN, GPIO_OUT);
    gpio_put(SHIFT_REG_LOAD_PIN, 1);
#endif
#if MCP3208_ENABLED
    spi_init(MCP_SPI, 1000 * 1000);  // MCP3208 is rated 1 MHz at 2.7 V
    gpio_set_function(MCP3208_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MCP3208_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MCP3208_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_init(MCP3208_CS_PIN);
    gpio_set_dir(MCP3208_CS_PIN, GPIO_OUT);
    gpio_put(MCP3208_CS_PIN, 1);
#endif

    num_inputs = SR_INPUTS;
    num_axes = MCP_AXES;
    for (size_t i = 0; i < COUNT_OF(digital_pins); i++)
        if (!is_reserved(digital_pins[i])) init_input_pin(digital_pins[i]);
    for (size_t i = 0; i < COUNT_OF(analog_pins); i++) {
        uint8_t pin = analog_pins[i];
        if (is_reserved(pin)) continue;
        if (num_axes < AXIS_COUNT) {
            axis_pin[num_axes++] = pin;
            adc_gpio_init(pin);
        } else {
            init_input_pin(pin);  // no axis slot left: use it as a button
        }
    }
    if (num_axes > MCP_AXES) adc_init();

    for (size_t i = 0; i < NUM_HAT; i++)
        if (valid_input(hat_inputs[i])) input_role[hat_inputs[i] - 1] = ROLE_HAT;
    for (size_t i = 0; i < NUM_ENCODERS; i++) {
        encoders[i].active = -1;
        if (!encoder_ok(i)) continue;
        input_role[encoder_cfg[i].a - 1] = ROLE_ENCODER;
        input_role[encoder_cfg[i].b - 1] = ROLE_ENCODER;
        if (encoder_cfg[i].a <= SR_INPUTS || encoder_cfg[i].b <= SR_INPUTS)
            encoders_on_shift_regs = true;
    }
    busy_wait_us_32(100);  // let pull-ups settle
    read_shift_registers();
    for (size_t i = 0; i < NUM_ENCODERS; i++)
        if (encoder_ok(i)) encoders[i].prev = read_encoder_ab(i);
}

// ---- Status LED -------------------------------------------------------------

#if STATUS_LED && defined(PICO_DEFAULT_LED_PIN)
static bool led_available(void) {
    for (size_t i = SR_INPUTS; i < num_inputs; i++)
        if (input_pin[i] == PICO_DEFAULT_LED_PIN) return false;
    return !is_reserved(PICO_DEFAULT_LED_PIN);
}
#endif

// ---- Main -------------------------------------------------------------------

int main(void) {
    init_hardware();
    tusb_init();

#if STATUS_LED && defined(PICO_DEFAULT_LED_PIN)
    bool use_led = led_available();
    if (use_led) {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    }
#endif

    joystick_report_t last;
    memset(&last, 0xFF, sizeof(last));
    uint64_t next_tick = time_us_64();

    while (true) {
        tud_task();
        poll_encoders();

        uint64_t now = time_us_64();
        if (now < next_tick) continue;
        next_tick += 1000;
        if (now > next_tick + 10000) next_tick = now;  // don't try to catch up after a stall

        scan_inputs();
        tick_encoders();
        build_report(&report);

#if STATUS_LED && defined(PICO_DEFAULT_LED_PIN)
        if (use_led) gpio_put(PICO_DEFAULT_LED_PIN, tud_mounted());
#endif

        if (tud_suspended()) {
            // Any button wakes the PC if it allowed remote wakeup.
            if (report.buttons) tud_remote_wakeup();
        } else if (tud_hid_ready() && memcmp(&report, &last, sizeof(report)) != 0) {
            if (tud_hid_report(0, &report, sizeof(report))) last = report;
        }
    }
}
