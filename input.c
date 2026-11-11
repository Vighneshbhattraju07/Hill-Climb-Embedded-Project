#include "input.h"
#include "console_config.h"

typedef struct {
    uint pin;
    bool raw_high;
    bool stable_high;
    absolute_time_t last_change;
    absolute_time_t stable_since;
} DebouncedButton;

typedef struct {
    bool active;
    bool fired;
    absolute_time_t started;
} ChordTimer;

enum { BTN_UP, BTN_LEFT, BTN_DOWN, BTN_RIGHT, BUTTON_COUNT };
static DebouncedButton buttons[BUTTON_COUNT];
static ChordTimer pause_chord;
static ChordTimer restart_chord;

static void init_one_button(DebouncedButton *button, uint pin) {
    button->pin = pin;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    button->raw_high = gpio_get(pin);
    button->stable_high = button->raw_high;
    button->last_change = get_absolute_time();
    button->stable_since = button->last_change;
}

static void update_one_button(DebouncedButton *button, absolute_time_t now) {
    bool sample = gpio_get(button->pin);
    if (sample != button->raw_high) {
        button->raw_high = sample;
        button->last_change = now;
    }

    if (button->raw_high != button->stable_high &&
        absolute_time_diff_us(button->last_change, now) >= INPUT_DEBOUNCE_US) {
        button->stable_high = button->raw_high;
        button->stable_since = now;
    }
}

static bool chord_held(DebouncedButton *first, DebouncedButton *second) {
    return !first->stable_high && !second->stable_high;
}

static bool update_chord(ChordTimer *chord, bool held, absolute_time_t now) {
    if (!held) {
        chord->active = false;
        chord->fired = false;
        return false;
    }
    if (!chord->active) {
        chord->active = true;
        chord->fired = false;
        chord->started = now;
    }
    if (!chord->fired &&
        absolute_time_diff_us(chord->started, now) >= INPUT_CHORD_HOLD_US) {
        chord->fired = true;
        return true;
    }
    return false;
}

void input_init(void) {
    init_one_button(&buttons[BTN_UP], BUTTON_PIN_UP);
    init_one_button(&buttons[BTN_LEFT], BUTTON_PIN_LEFT);
    init_one_button(&buttons[BTN_DOWN], BUTTON_PIN_DOWN);
    init_one_button(&buttons[BTN_RIGHT], BUTTON_PIN_RIGHT);
    pause_chord = (ChordTimer){0};
    restart_chord = (ChordTimer){0};
}

void input_poll(InputFrame *frame) {
    absolute_time_t now = get_absolute_time();
    for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
        update_one_button(&buttons[i], now);
    }

    bool up_down = chord_held(&buttons[BTN_UP], &buttons[BTN_DOWN]);
    bool left_right = chord_held(&buttons[BTN_LEFT], &buttons[BTN_RIGHT]);
    frame->pause_toggle = update_chord(&pause_chord, up_down, now);
    frame->restart = update_chord(&restart_chord, left_right, now);

    // Keep opposite-button chords available for pause/restart while exposing
    // held controls continuously for throttle, brake, and vehicle pitch.
    frame->up = !up_down && !buttons[BTN_UP].stable_high &&
        absolute_time_diff_us(buttons[BTN_UP].stable_since, now) >= INPUT_CHORD_GRACE_US;
    frame->down = !up_down && !buttons[BTN_DOWN].stable_high &&
        absolute_time_diff_us(buttons[BTN_DOWN].stable_since, now) >= INPUT_CHORD_GRACE_US;
    frame->left = !left_right && !buttons[BTN_LEFT].stable_high &&
        absolute_time_diff_us(buttons[BTN_LEFT].stable_since, now) >= INPUT_CHORD_GRACE_US;
    frame->right = !left_right && !buttons[BTN_RIGHT].stable_high &&
        absolute_time_diff_us(buttons[BTN_RIGHT].stable_since, now) >= INPUT_CHORD_GRACE_US;
}
