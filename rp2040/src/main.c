// RP2040-HID: turns an RP2040 board into a native USB HID joystick that
// behaves like a Leo Bodnar BU0836X. Every ADC pin is an axis, every other
// usable pin is a button. See config.h.

#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
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

#define NUM_INPUTS   COUNT_OF(digital_pins)
#define NUM_AXES     COUNT_OF(analog_pins)
#define NUM_ENCODERS COUNT_OF(encoder_cfg)
#define NUM_HAT      COUNT_OF(hat_inputs)

_Static_assert(NUM_AXES <= AXIS_COUNT, "At most 8 analog inputs");
_Static_assert(NUM_HAT == 0 || NUM_HAT == 4, "HAT_INPUTS needs exactly 4 inputs: up, right, down, left");

// What each digital input currently does.
enum { ROLE_BUTTON, ROLE_ENCODER, ROLE_HAT };
static uint8_t input_role[NUM_INPUTS + 1];  // +1 keeps the array non-empty

static inline bool valid_input(uint8_t n) {
    return n >= 1 && n <= NUM_INPUTS;
}

static inline bool is_low(uint gpio) {
    return !gpio_get(gpio);
}

// ---- Debounced inputs -------------------------------------------------------

typedef struct {
    bool    pressed;
    uint8_t count;
} debounce_t;

static debounce_t inputs[NUM_INPUTS + 1];

// Called once per millisecond.
static void scan_inputs(void) {
    for (size_t i = 0; i < NUM_INPUTS; i++) {
        debounce_t *d = &inputs[i];
        bool raw = is_low(digital_pins[i]);
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

static inline bool encoder_ok(size_t i) {
    return valid_input(encoder_cfg[i].a) && valid_input(encoder_cfg[i].b);
}

static uint8_t read_encoder_ab(size_t i) {
    return (uint8_t)((is_low(digital_pins[encoder_cfg[i].a - 1]) << 1) |
                     is_low(digital_pins[encoder_cfg[i].b - 1]));
}

// Called as often as possible so fast spins aren't missed.
static void poll_encoders(void) {
    static const int8_t qdec[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
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

static uint32_t axis_filtered[NUM_AXES + 1];  // value << 8 for sub-count precision

static uint16_t read_axis(size_t i) {
    adc_select_input(analog_pins[i] - ADC_BASE_PIN);
    uint32_t sum = 0;
    for (int n = 0; n < 16; n++) sum += adc_read();
    uint32_t sample = (sum / 16) << 8;

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
        r->axis[a] = a < NUM_AXES ? read_axis(a) : AXIS_CENTER;

    r->hat = read_hat();

    // Input N is button N, exactly like the BU0836X.
    uint32_t buttons = 0;
    for (size_t i = 0; i < NUM_INPUTS && i < BUTTON_COUNT; i++)
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

// ---- Status LED -------------------------------------------------------------

#if STATUS_LED && defined(PICO_DEFAULT_LED_PIN)
static bool led_available(void) {
    for (size_t i = 0; i < NUM_INPUTS; i++)
        if (digital_pins[i] == PICO_DEFAULT_LED_PIN) return false;
    return true;
}
#endif

// ---- Main -------------------------------------------------------------------

static void init_hardware(void) {
    for (size_t i = 0; i < NUM_INPUTS; i++) {
        gpio_init(digital_pins[i]);
        gpio_set_dir(digital_pins[i], GPIO_IN);
        gpio_pull_up(digital_pins[i]);
    }
    for (size_t i = 0; i < NUM_HAT; i++)
        if (valid_input(hat_inputs[i])) input_role[hat_inputs[i] - 1] = ROLE_HAT;
    for (size_t i = 0; i < NUM_ENCODERS; i++) {
        encoders[i].active = -1;
        if (!encoder_ok(i)) continue;
        input_role[encoder_cfg[i].a - 1] = ROLE_ENCODER;
        input_role[encoder_cfg[i].b - 1] = ROLE_ENCODER;
    }
    busy_wait_us_32(100);  // let pull-ups settle
    for (size_t i = 0; i < NUM_ENCODERS; i++)
        if (encoder_ok(i)) encoders[i].prev = read_encoder_ab(i);

    if (NUM_AXES) {
        adc_init();
        for (size_t i = 0; i < NUM_AXES; i++) adc_gpio_init(analog_pins[i]);
    }
}

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
