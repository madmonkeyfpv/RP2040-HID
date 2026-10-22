// RP2040-HID: turns an RP2040 board into a native USB HID joystick, in the
// spirit of the Leo Bodnar BU0836 boards. Configure pins in config.h.

#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "tusb.h"

#include "hid_report.h"
#include "config.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

typedef struct { uint8_t gpio; uint8_t axis; bool inverted; } axis_cfg_t;
typedef struct { uint8_t a, b; } encoder_cfg_t;

static const uint8_t direct_pins[] = DIRECT_BUTTON_PINS;
static const uint8_t row_pins[] = MATRIX_ROW_PINS;
static const uint8_t col_pins[] = MATRIX_COL_PINS;
static const encoder_cfg_t encoder_pins[] = ENCODER_PINS;
static const axis_cfg_t axis_cfg[] = ANALOG_AXES;
#ifdef HAT_PINS
static const uint8_t hat_pins[] = HAT_PINS;
#else
static const uint8_t hat_pins[] = {};
#endif

#define NUM_DIRECT   COUNT_OF(direct_pins)
#define NUM_MATRIX   (COUNT_OF(row_pins) * COUNT_OF(col_pins))
#define NUM_HAT      COUNT_OF(hat_pins)
#define NUM_SWITCHES (NUM_DIRECT + NUM_MATRIX + NUM_HAT)
#define NUM_ENCODERS COUNT_OF(encoder_pins)
#define NUM_AXES     COUNT_OF(axis_cfg)

_Static_assert(NUM_HAT == 0 || NUM_HAT == 4, "HAT_PINS needs exactly 4 pins: up, right, down, left");

// ---- Debounced switches -----------------------------------------------------

typedef struct {
    bool    pressed;
    uint8_t count;
} debounce_t;

static debounce_t switches[NUM_SWITCHES + 1];  // +1 keeps the array non-empty

static void debounce(debounce_t *d, bool raw) {
    if (raw == d->pressed) {
        d->count = 0;
    } else if (++d->count >= DEBOUNCE_MS) {
        d->pressed = raw;
        d->count = 0;
    }
}

static void init_input(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}

static inline bool is_low(uint gpio) {
    return !gpio_get(gpio);
}

// Called once per millisecond.
static void scan_switches(void) {
    size_t s = 0;

    for (size_t i = 0; i < NUM_DIRECT; i++)
        debounce(&switches[s++], is_low(direct_pins[i]));

    for (size_t r = 0; r < COUNT_OF(row_pins); r++) {
        // Drive one row low at a time; idle rows float so they can't fight.
        gpio_set_dir(row_pins[r], GPIO_OUT);
        gpio_put(row_pins[r], 0);
        busy_wait_us_32(5);
        for (size_t c = 0; c < COUNT_OF(col_pins); c++)
            debounce(&switches[s++], is_low(col_pins[c]));
        gpio_set_dir(row_pins[r], GPIO_IN);
    }

    for (size_t i = 0; i < NUM_HAT; i++)
        debounce(&switches[s++], is_low(hat_pins[i]));
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
    const debounce_t *h = &switches[NUM_DIRECT + NUM_MATRIX];
    uint8_t idx = (h[0].pressed << 0) | (h[1].pressed << 1) |
                  (h[2].pressed << 2) | (h[3].pressed << 3);
    return lut[idx];
}

// ---- Rotary encoders --------------------------------------------------------

typedef struct {
    uint8_t  prev;        // last AB state
    int8_t   steps;       // quadrature steps since last detent
    uint8_t  pending[2];  // queued detents: [0] = CCW, [1] = CW
    int8_t   active;      // direction currently being pulsed, -1 = none
    uint16_t timer_ms;    // time left in current pulse phase
    bool     released;    // true during the gap after a pulse
} encoder_t;

static encoder_t encoders[NUM_ENCODERS + 1];

static uint8_t read_encoder_ab(size_t i) {
    return (uint8_t)((is_low(encoder_pins[i].a) << 1) | is_low(encoder_pins[i].b));
}

// Called as often as possible so fast spins aren't missed.
static void poll_encoders(void) {
    static const int8_t qdec[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
    for (size_t i = 0; i < NUM_ENCODERS; i++) {
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
    adc_select_input(axis_cfg[i].gpio - ADC_BASE_PIN);
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
    return (uint16_t)(axis_cfg[i].inverted ? AXIS_MAX - v : v);
}

// ---- Report -----------------------------------------------------------------

static void build_report(joystick_report_t *r) {
    for (int a = 0; a < AXIS_COUNT; a++) r->axis[a] = AXIS_CENTER;
    for (size_t i = 0; i < NUM_AXES; i++) r->axis[axis_cfg[i].axis] = read_axis(i);

    r->hat = read_hat();

    uint32_t buttons = 0;
    unsigned b = 0;
    for (size_t i = 0; i < NUM_DIRECT + NUM_MATRIX; i++, b++)
        if (switches[i].pressed && b < BUTTON_COUNT) buttons |= 1u << b;
    for (size_t i = 0; i < NUM_ENCODERS; i++, b += 2) {
        const encoder_t *e = &encoders[i];
        if (e->active >= 0 && !e->released && b + e->active < BUTTON_COUNT)
            buttons |= 1u << (b + e->active);
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

// ---- Main -------------------------------------------------------------------

static void init_hardware(void) {
    for (size_t i = 0; i < NUM_DIRECT; i++) init_input(direct_pins[i]);
    for (size_t i = 0; i < COUNT_OF(row_pins); i++) init_input(row_pins[i]);
    for (size_t i = 0; i < COUNT_OF(col_pins); i++) init_input(col_pins[i]);
    for (size_t i = 0; i < NUM_HAT; i++) init_input(hat_pins[i]);
    for (size_t i = 0; i < NUM_ENCODERS; i++) {
        init_input(encoder_pins[i].a);
        init_input(encoder_pins[i].b);
        encoders[i].active = -1;
    }
    busy_wait_us_32(100);  // let pull-ups settle
    for (size_t i = 0; i < NUM_ENCODERS; i++) encoders[i].prev = read_encoder_ab(i);

    if (NUM_AXES) {
        adc_init();
        for (size_t i = 0; i < NUM_AXES; i++) adc_gpio_init(axis_cfg[i].gpio);
    }

#if STATUS_LED && defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

int main(void) {
    init_hardware();
    tusb_init();

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

        scan_switches();
        tick_encoders();
        build_report(&report);

#if STATUS_LED && defined(PICO_DEFAULT_LED_PIN)
        gpio_put(PICO_DEFAULT_LED_PIN, tud_mounted());
#endif

        if (tud_suspended()) {
            // Any button wakes the PC if it allowed remote wakeup.
            if (report.buttons) tud_remote_wakeup();
        } else if (tud_hid_ready() && memcmp(&report, &last, sizeof(report)) != 0) {
            if (tud_hid_report(0, &report, sizeof(report))) last = report;
        }
    }
}
