/**
 * @file hal_button_engine.c
 * @brief Implementation of software debouncer and coin pulse validator
 */

#include "hal_button_engine.h"
#include <stddef.h>

void hal_button_init(hal_button_t *btn, bool active_low) {
    if (!btn) {
        return;
    }
    btn->active_low = active_low;
    btn->debounced_state = false;
    btn->last_raw_state = active_low ? true : false;
    btn->debounce_counter = 0;
    btn->pressed_event = false;
    btn->released_event = false;
}

void hal_button_update(hal_button_t *btn, bool raw_pin_high, uint32_t delta_ms) {
    if (!btn) {
        return;
    }

    bool current_pressed = btn->active_low ? (!raw_pin_high) : (raw_pin_high);

    if (current_pressed != btn->debounced_state) {
        btn->debounce_counter += delta_ms;
        if (btn->debounce_counter >= HAL_DEBOUNCE_DEFAULT_MS) {
            btn->debounced_state = current_pressed;
            btn->debounce_counter = 0;
            if (btn->debounced_state) {
                btn->pressed_event = true;
            } else {
                btn->released_event = true;
            }
        }
    } else {
        btn->debounce_counter = 0;
    }

    btn->last_raw_state = raw_pin_high;
}

bool hal_button_was_pressed(hal_button_t *btn) {
    if (!btn) {
        return false;
    }
    if (btn->pressed_event) {
        btn->pressed_event = false;
        return true;
    }
    return false;
}

bool hal_button_was_released(hal_button_t *btn) {
    if (!btn) {
        return false;
    }
    if (btn->released_event) {
        btn->released_event = false;
        return true;
    }
    return false;
}

bool hal_button_is_pressed(const hal_button_t *btn) {
    return btn ? btn->debounced_state : false;
}

void hal_coin_pulse_init(hal_coin_pulse_detector_t *det, bool active_low, uint32_t timeout_ms) {
    if (!det) {
        return;
    }
    hal_button_init(&det->pulse_input, active_low);
    det->pulse_count = 0;
    det->silence_timer_ms = 0;
    det->inter_pulse_timeout_ms = (timeout_ms > 0) ? timeout_ms : 150U;
    det->decoded_cents = 0;
}

void hal_coin_pulse_update(hal_coin_pulse_detector_t *det, bool raw_pin_high, uint32_t delta_ms) {
    if (!det) {
        return;
    }

    hal_button_update(&det->pulse_input, raw_pin_high, delta_ms);

    if (hal_button_was_pressed(&det->pulse_input)) {
        det->pulse_count++;
        det->silence_timer_ms = 0;
    } else if (det->pulse_count > 0) {
        det->silence_timer_ms += delta_ms;
        if (det->silence_timer_ms >= det->inter_pulse_timeout_ms) {
            /* Pulse train finished: decode standard denomination */
            if (det->pulse_count == 1) {
                det->decoded_cents = 10;
            } else if (det->pulse_count == 2) {
                det->decoded_cents = 20;
            } else if (det->pulse_count == 5) {
                det->decoded_cents = 50;
            } else {
                /* Invalid pulse count rejected */
                det->decoded_cents = 0;
            }
            det->pulse_count = 0;
            det->silence_timer_ms = 0;
        }
    }
}

uint32_t hal_coin_pulse_get_coin(hal_coin_pulse_detector_t *det) {
    if (!det) {
        return 0;
    }
    if (det->decoded_cents > 0) {
        uint32_t val = det->decoded_cents;
        det->decoded_cents = 0;
        return val;
    }
    return 0;
}
