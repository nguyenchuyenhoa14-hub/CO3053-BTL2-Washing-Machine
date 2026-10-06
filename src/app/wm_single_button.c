/**
 * @file wm_single_button.c
 * @brief Single-button gesture to FSM event mapping
 */

#include "wm_single_button.h"

static bool apply_click(wm_context_t *ctx) {
    switch (ctx->state) {
        case WM_STATE_STANDBY:
        case WM_STATE_COLLECTING:
            return wm_fsm_dispatch_event(ctx, WM_EVT_COIN_50);
        case WM_STATE_READY:
        case WM_STATE_PAUSED:
            return wm_fsm_dispatch_event(ctx, WM_EVT_BTN_RUN);
        case WM_STATE_RUNNING:
            return wm_fsm_dispatch_event(ctx, WM_EVT_BTN_PAUSE);
        case WM_STATE_ERROR:
            wm_fsm_clear_fault(ctx);
            return true;
        default:
            return false;
    }
}

static bool apply_double(wm_context_t *ctx) {
    switch (ctx->state) {
        case WM_STATE_STANDBY:
        case WM_STATE_COLLECTING:
            return wm_fsm_dispatch_event(ctx, WM_EVT_COIN_10);
        case WM_STATE_READY:
        case WM_STATE_RUNNING:
        case WM_STATE_PAUSED:
            wm_fsm_trigger_fault(ctx, WM_FAULT_DOOR_OPEN);
            return true;
        case WM_STATE_ERROR:
            wm_fsm_clear_fault(ctx);
            return true;
        default:
            return false;
    }
}

bool wm_single_button_apply(wm_context_t *ctx, hal_gesture_t gesture) {
    if (ctx == NULL) {
        return false;
    }

    switch (gesture) {
        case HAL_GESTURE_CLICK:
            return apply_click(ctx);
        case HAL_GESTURE_DOUBLE:
            return apply_double(ctx);
        case HAL_GESTURE_HOLD1:
            return wm_fsm_dispatch_event(ctx, WM_EVT_BTN_STOP);
        case HAL_GESTURE_HOLD2: {
            bool changed = wm_fsm_dispatch_event(ctx, WM_EVT_BTN_STOP);
            if (ctx->state == WM_STATE_ERROR) {
                /* STOP is not defined to leave ERROR; clear the fault so a long hold always recovers the panel */
                wm_fsm_clear_fault(ctx);
                changed = true;
            }
            return changed;
        }
        case HAL_GESTURE_NONE:
        default:
            return false;
    }
}

bool wm_ext_run_pause_apply(wm_context_t *ctx) {
    if (ctx == NULL || ctx->state == WM_STATE_STANDBY || ctx->state == WM_STATE_COLLECTING) {
        return false;
    }
    return apply_click(ctx);
}

bool wm_ext_stop_apply(wm_context_t *ctx) {
    if (ctx == NULL) {
        return false;
    }
    if (ctx->state == WM_STATE_ERROR) {
        wm_fsm_clear_fault(ctx);
        return true;
    }
    return wm_fsm_dispatch_event(ctx, WM_EVT_BTN_STOP);
}
