/**
 * @file hal_button_gesture.c
 * @brief Single-button gesture decoder implementation
 */

#include "hal_button_gesture.h"

void hal_gesture_init(hal_gesture_ctx_t *g) {
    if (g == NULL) {
        return;
    }
    g->phase = HAL_GST_IDLE;
    g->timer_ms = 0U;
    g->hold2_fired = false;
}

hal_gesture_t hal_gesture_update(hal_gesture_ctx_t *g, bool pressed, uint32_t delta_ms) {
    if (g == NULL) {
        return HAL_GESTURE_NONE;
    }

    switch (g->phase) {
        case HAL_GST_IDLE:
            if (pressed) {
                g->phase = HAL_GST_DOWN1;
                g->timer_ms = 0U;
                g->hold2_fired = false;
            }
            break;

        case HAL_GST_DOWN1:
            g->timer_ms += delta_ms;
            if (!pressed) {
                g->phase = HAL_GST_WAIT;
                g->timer_ms = 0U;
            } else if (g->timer_ms >= HAL_GESTURE_HOLD1_MS) {
                g->phase = HAL_GST_HELD;
                return HAL_GESTURE_HOLD1;
            }
            break;

        case HAL_GST_WAIT:
            g->timer_ms += delta_ms;
            if (pressed) {
                g->phase = HAL_GST_DOWN2;
                g->timer_ms = 0U;
            } else if (g->timer_ms >= HAL_GESTURE_DOUBLE_GAP_MS) {
                g->phase = HAL_GST_IDLE;
                return HAL_GESTURE_CLICK;
            }
            break;

        case HAL_GST_DOWN2:
            g->timer_ms += delta_ms;
            if (!pressed) {
                g->phase = HAL_GST_IDLE;
                return HAL_GESTURE_DOUBLE;
            }
            if (g->timer_ms >= HAL_GESTURE_HOLD1_MS) {
                g->phase = HAL_GST_HELD;
                return HAL_GESTURE_HOLD1;
            }
            break;

        case HAL_GST_HELD:
            g->timer_ms += delta_ms;
            if (!pressed) {
                g->phase = HAL_GST_IDLE;
                if (!g->hold2_fired) {
                    return HAL_GESTURE_HOLD1_END;
                }
            } else if (!g->hold2_fired && g->timer_ms >= HAL_GESTURE_HOLD2_MS) {
                g->hold2_fired = true;
                return HAL_GESTURE_HOLD2;
            }
            break;

        default:
            g->phase = HAL_GST_IDLE;
            break;
    }
    return HAL_GESTURE_NONE;
}

uint32_t hal_gesture_hold_ms(const hal_gesture_ctx_t *g) {
    if (g == NULL) {
        return 0U;
    }
    if (g->phase == HAL_GST_DOWN1 || g->phase == HAL_GST_DOWN2 || g->phase == HAL_GST_HELD) {
        return g->timer_ms;
    }
    return 0U;
}
