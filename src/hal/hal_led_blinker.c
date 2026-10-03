/**
 * @file hal_led_blinker.c
 * @brief Implementation of non-blocking LED blinker engine and actuator interlock guard
 */

#include "hal_led_blinker.h"
#include <stddef.h>

void hal_led_blinker_init(hal_led_blinker_t *blinker, bool active_high) {
    if (!blinker) {
        return;
    }
    blinker->mode = HAL_LED_OFF;
    blinker->active_high = active_high;
    blinker->output_level = active_high ? false : true;
    blinker->phase_timer_ms = 0;
    blinker->half_period_ms = 0;
}

void hal_led_blinker_set_mode(hal_led_blinker_t *blinker, hal_led_state_t mode) {
    if (!blinker) {
        return;
    }

    if (blinker->mode == mode) {
        return;
    }

    blinker->mode = mode;
    blinker->phase_timer_ms = 0;

    switch (mode) {
        case HAL_LED_OFF:
            blinker->half_period_ms = 0;
            blinker->output_level = blinker->active_high ? false : true;
            break;

        case HAL_LED_ON:
            blinker->half_period_ms = 0;
            blinker->output_level = blinker->active_high ? true : false;
            break;

        case HAL_LED_BLINK_1HZ:
            blinker->half_period_ms = WM_BLED_BLINK_PERIOD_MS / 2U; /* 500 ms */
            blinker->output_level = blinker->active_high ? true : false;
            break;

        case HAL_LED_BLINK_2HZ:
            blinker->half_period_ms = WM_RLED_BLINK_PERIOD_MS / 2U; /* 250 ms */
            blinker->output_level = blinker->active_high ? true : false;
            break;

        default:
            blinker->half_period_ms = 0;
            blinker->output_level = blinker->active_high ? false : true;
            break;
    }
}

void hal_led_blinker_tick_ms(hal_led_blinker_t *blinker, uint32_t delta_ms) {
    if (!blinker || blinker->half_period_ms == 0U) {
        return;
    }

    if (blinker->mode == HAL_LED_BLINK_1HZ || blinker->mode == HAL_LED_BLINK_2HZ) {
        /* Guard against arithmetic wrap-around under extreme delta_ms */
        if (delta_ms > (UINT32_MAX - blinker->phase_timer_ms)) {
            delta_ms %= blinker->half_period_ms;
        }
        blinker->phase_timer_ms += delta_ms;
        if (blinker->phase_timer_ms >= blinker->half_period_ms) {
            uint32_t toggles = blinker->phase_timer_ms / blinker->half_period_ms;
            if ((toggles & 1U) != 0U) {
                blinker->output_level = !blinker->output_level;
            }
            blinker->phase_timer_ms %= blinker->half_period_ms;
        }
    }
}

bool hal_led_blinker_get_output(const hal_led_blinker_t *blinker) {
    return blinker ? blinker->output_level : false;
}

bool hal_actuator_is_safe(const hal_actuator_guard_t *guard) {
    if (!guard) {
        return false;
    }

    /* Safety Rule 1: Motor must NEVER run if door lock is unengaged */
    if (guard->motor != HAL_MOTOR_OFF && !guard->door_lock) {
        return false;
    }

    /* Safety Rule 2: High-speed spin mode requires drain pump to be active */
    if (guard->motor == HAL_MOTOR_SPIN && !guard->drain_pump) {
        return false;
    }

    /* Safety Rule 3: Water inlet valve must NEVER be open during high-speed spin dry */
    if (guard->motor == HAL_MOTOR_SPIN && guard->water_valve) {
        return false;
    }

    /* Safety Rule 4: Water inlet valve and drain pump must NEVER run simultaneously */
    if (guard->water_valve && guard->drain_pump) {
        return false;
    }

    return true;
}
