/**
 * @file ui_text.c
 * @brief Status line formatter (no printf / no heap)
 */

#include "ui_text.h"

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} out_t;

static void put_str(out_t *o, const char *s) {
    while (*s != '\0' && (o->len + 1U) < o->cap) {
        o->buf[o->len++] = *s++;
    }
    o->buf[o->len] = '\0';
}

static void put_num(out_t *o, uint32_t v, unsigned digits) {
    char tmp[11];
    unsigned n = 0U;
    do {
        tmp[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    } while (v != 0U && n < sizeof(tmp));
    while (n < digits && n < sizeof(tmp)) {
        tmp[n++] = '0';
    }
    char one[2] = {0, 0};
    while (n > 0U) {
        one[0] = tmp[--n];
        put_str(o, one);
    }
}

static const char *phase_str(wm_cycle_phase_t p) {
    switch (p) {
        case WM_PHASE_WASH_AGITATE: return "WASH";
        case WM_PHASE_FINAL_SPIN:   return "SPIN";
        default:                    return "IDLE";
    }
}

static const char *motor_str(hal_motor_state_t m) {
    switch (m) {
        case HAL_MOTOR_AGITATE: return "AGIT";
        case HAL_MOTOR_SPIN:    return "SPIN";
        default:                return "OFF";
    }
}

size_t ui_status_line(char *buf, size_t cap, const ui_view_t *v) {
    out_t o = {buf, cap, 0U};
    if (cap == 0U) {
        return 0U;
    }
    buf[0] = '\0';

    uint32_t sec = (v->state == WM_STATE_STANDBY || v->state == WM_STATE_READY) ?
                   v->total_sec : v->remaining_sec;

    put_str(&o, "STATE=");   put_str(&o, wm_state_to_str(v->state));
    put_str(&o, " PHASE=");  put_str(&o, phase_str(v->phase));
    put_str(&o, " TIME=");   put_num(&o, sec / 60U, 2U); put_str(&o, ":"); put_num(&o, sec % 60U, 2U);
    put_str(&o, " COIN=");   put_num(&o, v->balance_cents, 3U); put_str(&o, "C");
    put_str(&o, " RLED=");   put_str(&o, v->rled ? "1" : "0");
    put_str(&o, " BLED=");   put_str(&o, v->bled ? "1" : "0");
    put_str(&o, " MOTOR=");  put_str(&o, motor_str(v->motor));
    put_str(&o, " PUMP=");   put_str(&o, v->pump ? "1" : "0");
    put_str(&o, " LOCK=");   put_str(&o, v->lock ? "1" : "0");
    put_str(&o, " FAULT=");  put_str(&o, (v->fault_flags == WM_FAULT_NONE) ? "-" : wm_fault_to_str(v->fault_flags));
    return o.len;
}
