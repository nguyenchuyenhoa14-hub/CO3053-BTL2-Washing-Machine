/**
 * @file wm_two_button.c
 * @brief K0/K1 front panel logic with coin staging
 */

#include <stddef.h>
#include "wm_two_button.h"
#include "wm_single_button.h"

static bool is_idle(const wm_context_t *ctx) {
    return ctx->state == WM_STATE_STANDBY || ctx->state == WM_STATE_COLLECTING;
}

void wm_staging_init(wm_coin_staging_t *s) {
    if (s != NULL) {
        s->pending_cents = 0U;
    }
}

void wm_staging_sync(wm_coin_staging_t *s, const wm_context_t *ctx) {
    if (s != NULL && ctx != NULL && !is_idle(ctx)) {
        s->pending_cents = 0U;
    }
}

static bool stage_coin(wm_coin_staging_t *s, const wm_context_t *ctx, uint32_t cents) {
    if (!is_idle(ctx)) {
        return false;
    }
    uint32_t room = WM_STAGING_MAX_CENTS - s->pending_cents;
    s->pending_cents += (cents > room) ? room : cents;
    return true;
}

static bool confirm(wm_context_t *ctx, wm_coin_staging_t *s, wm_notice_t *notice, uint32_t *value) {
    if (!is_idle(ctx)) {
        return false;
    }

    uint32_t pending = s->pending_cents;
    s->pending_cents = 0U;

    if (pending >= WM_COIN_THRESHOLD_CENTS) {
        /* The FSM sees exactly the fare; the excess is reported as returned change */
        (void)wm_fsm_dispatch_event(ctx, WM_EVT_COIN_50);
        if (notice != NULL) {
            *notice = WM_NOTICE_READY;
        }
        if (value != NULL) {
            *value = pending - WM_COIN_THRESHOLD_CENTS;
        }
    } else {
        if (notice != NULL) {
            *notice = WM_NOTICE_NOT_ENOUGH;
        }
        if (value != NULL) {
            *value = pending;
        }
    }
    return true;
}

bool wm_k1_apply(wm_context_t *ctx, wm_coin_staging_t *s, hal_gesture_t gesture,
                 wm_notice_t *notice, uint32_t *value) {
    if (ctx == NULL || s == NULL) {
        return false;
    }
    wm_staging_sync(s, ctx);

    if (is_idle(ctx)) {
        switch (gesture) {
            case HAL_GESTURE_CLICK:
                return stage_coin(s, ctx, 10U);
            case HAL_GESTURE_DOUBLE:
                return stage_coin(s, ctx, 20U);
            case HAL_GESTURE_HOLD1_END:
                return stage_coin(s, ctx, 50U);
            case HAL_GESTURE_HOLD2:
                return confirm(ctx, s, notice, value);
            default:
                return false;
        }
    }
    return wm_single_button_apply(ctx, gesture);
}

bool wm_k0_apply(wm_context_t *ctx, wm_coin_staging_t *s, hal_gesture_t gesture) {
    if (ctx == NULL || s == NULL) {
        return false;
    }
    wm_staging_sync(s, ctx);

    if (is_idle(ctx)) {
        switch (gesture) {
            case HAL_GESTURE_CLICK:
                return stage_coin(s, ctx, 10U);
            case HAL_GESTURE_DOUBLE:
            case HAL_GESTURE_HOLD1:
                return stage_coin(s, ctx, 20U);
            default:
                return false;
        }
    }
    return (gesture == HAL_GESTURE_CLICK) ? wm_ext_run_pause_apply(ctx) : false;
}
