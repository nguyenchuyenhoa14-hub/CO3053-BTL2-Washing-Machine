/**
 * @file ui_render.c
 * @brief Dashboard layout (landscape 160x80):
 *        y 0..9   title + coin balance
 *        y 12..26 state name (2x font)
 *        y 30..44 countdown MM:SS (2x font) + phase
 *        y 48..53 cycle progress bar
 *        y 57..67 RLED / BLED / MOTOR / PUMP / LOCK indicators
 *        y 72..78 button hint (replaced by a hold-progress bar while the key is held)
 */

#include "ui_render.h"
#include "gfx.h"
#include "hal_button_gesture.h"

bool ui_view_equal(const ui_view_t *a, const ui_view_t *b) {
    return a->state == b->state && a->phase == b->phase &&
           a->balance_cents == b->balance_cents && a->remaining_sec == b->remaining_sec &&
           a->total_sec == b->total_sec && a->fault_flags == b->fault_flags &&
           a->rled == b->rled && a->bled == b->bled && a->motor == b->motor &&
           a->pump == b->pump && a->lock == b->lock && a->hold_ms == b->hold_ms &&
           a->reset_cause == b->reset_cause && a->bl_mode == b->bl_mode &&
           a->notice == b->notice && a->notice_value == b->notice_value;
}

static uint16_t state_color(wm_state_t s) {
    switch (s) {
        case WM_STATE_STANDBY: return GFX_RED;
        case WM_STATE_COLLECTING: return GFX_RED;
        case WM_STATE_READY:   return GFX_BLUE;
        case WM_STATE_RUNNING: return GFX_GREEN;
        case WM_STATE_PAUSED:  return GFX_YELLOW;
        case WM_STATE_ERROR:   return GFX_ORANGE;
        default:               return GFX_WHITE;
    }
}

static const char *hint_for(wm_state_t s) {
    switch (s) {
        case WM_STATE_STANDBY:
        case WM_STATE_COLLECTING: return "K1:+10 2x:+20 H:+50 HH:OK";
        case WM_STATE_READY:   return "K1:RUN  2xK1:FAULT";
        case WM_STATE_RUNNING: return "K1:PAUSE HH:STOP";
        case WM_STATE_PAUSED:  return "K1:RESUME HH:STOP";
        case WM_STATE_ERROR:   return "K1:CLEAR FAULT";
        default:               return "";
    }
}

static void fmt_uint(char *dst, uint32_t v, int digits) {
    for (int i = digits - 1; i >= 0; i--) {
        dst[i] = (char)('0' + (v % 10U));
        v /= 10U;
    }
}

static void draw_indicator(int x, const char *label, bool on, uint16_t color) {
    gfx_rect(x, 57, 29, 11, on ? color : GFX_DARK);
    gfx_text(x + 2, 59, label, 1, on ? GFX_BLACK : GFX_GRAY);
}

void ui_render(const ui_view_t *v) {
    char buf[24];

    gfx_fill(GFX_BLACK);

    /* Header */
    gfx_text(2, 1, "WASHER", 1, GFX_GRAY);
    if (v->reset_cause != 0) {
        int x = gfx_text(52, 1, "RST ", 1, GFX_GRAY);
        (void)gfx_text(x, 1, v->reset_cause, 1, GFX_GRAY);
    }
    {
        uint32_t bal = (v->balance_cents > 999U) ? 999U : v->balance_cents;
        fmt_uint(&buf[0], bal, 3);
        buf[3] = 'C';
        buf[4] = '\0';
        gfx_text(GFX_WIDTH - 2 - gfx_text_width(buf, 1), 1, buf, 1, GFX_WHITE);
    }
    if (v->bl_mode != 0U) {
        buf[0] = 'B'; buf[1] = 'L'; buf[2] = (char)('0' + v->bl_mode); buf[3] = '\0';
        gfx_text(98, 1, buf, 1, GFX_YELLOW);
    }
    gfx_rect(0, 10, GFX_WIDTH, 1, GFX_DARK);

    /* State name */
    gfx_text(2, 13, wm_state_to_str(v->state), 2, state_color(v->state));

    /* Countdown / fault text */
    if (v->state == WM_STATE_ERROR) {
        if ((v->fault_flags & WM_FAULT_DOOR_OPEN) != 0U) {
            gfx_text(2, 32, "DOOR OPEN", 2, GFX_ORANGE);
        } else {
            gfx_text(2, 36, wm_fault_to_str(v->fault_flags), 1, GFX_ORANGE);
        }
    } else {
        uint32_t sec = (v->state == WM_STATE_STANDBY || v->state == WM_STATE_READY) ?
                       v->total_sec : v->remaining_sec;
        fmt_uint(&buf[0], sec / 60U, 2);
        buf[2] = ':';
        fmt_uint(&buf[3], sec % 60U, 2);
        buf[5] = '\0';
        gfx_text(2, 32, buf, 2, GFX_WHITE);
        if (v->phase != WM_PHASE_IDLE) {
            gfx_text(GFX_WIDTH - 2 - gfx_text_width("SPIN", 1), 36,
                     (v->phase == WM_PHASE_FINAL_SPIN) ? "SPIN" : "WASH", 1, GFX_BLUE);
        }
    }

    /* Progress bar */
    gfx_frame(2, 48, GFX_WIDTH - 4, 6, GFX_GRAY);
    if (v->total_sec > 0U && (v->state == WM_STATE_RUNNING || v->state == WM_STATE_PAUSED)) {
        uint32_t done = v->total_sec - ((v->remaining_sec > v->total_sec) ? v->total_sec : v->remaining_sec);
        int w = (int)((done * (uint32_t)(GFX_WIDTH - 6)) / v->total_sec);
        gfx_rect(3, 49, w, 4, state_color(v->state));
    }

    /* Output indicators (what the relays/LEDs would be doing) */
    draw_indicator(2,   "RLED", v->rled, GFX_RED);
    draw_indicator(33,  "BLED", v->bled, GFX_BLUE);
    draw_indicator(64,  (v->motor == HAL_MOTOR_SPIN) ? "SPIN" : "AGIT", v->motor != HAL_MOTOR_OFF, GFX_GREEN);
    draw_indicator(95,  "PUMP", v->pump, GFX_YELLOW);
    draw_indicator(126, "LOCK", v->lock, GFX_ORANGE);

    /* Footer: hint or hold progress */
    if (v->hold_ms >= 150U) {
        uint32_t h = (v->hold_ms > HAL_GESTURE_HOLD2_MS) ? HAL_GESTURE_HOLD2_MS : v->hold_ms;
        int w = (int)((h * (uint32_t)(GFX_WIDTH - 4)) / HAL_GESTURE_HOLD2_MS);
        gfx_frame(2, 72, GFX_WIDTH - 4, 7, GFX_GRAY);
        gfx_rect(3, 73, w - 2 > 0 ? w - 2 : 0, 5, (v->hold_ms >= HAL_GESTURE_HOLD2_MS) ? GFX_RED : GFX_YELLOW);
        gfx_rect(2 + ((HAL_GESTURE_HOLD1_MS * (GFX_WIDTH - 4)) / HAL_GESTURE_HOLD2_MS), 71, 1, 9, GFX_WHITE);
    } else if (v->notice != 0U) {
        uint32_t amount = (v->notice_value > 999U) ? 999U : v->notice_value;
        int x;
        if (v->notice == 1U) {
            x = gfx_text(2, 72, "READY! CHANGE ", 1, GFX_BLUE);
        } else if (v->notice == 3U) {
            x = gfx_text(2, 72, "CANCELLED! RETURN ", 1, GFX_YELLOW);
        } else {
            x = gfx_text(2, 72, "NOT ENOUGH! REFUND ", 1, GFX_ORANGE);
        }
        fmt_uint(&buf[0], amount, 3);
        buf[3] = 'C';
        buf[4] = '\0';
        (void)gfx_text(x, 72, buf, 1, (v->notice == 1U) ? GFX_BLUE : (v->notice == 3U) ? GFX_YELLOW : GFX_ORANGE);
    } else {
        gfx_text(2, 72, hint_for(v->state), 1, GFX_GRAY);
    }
}
