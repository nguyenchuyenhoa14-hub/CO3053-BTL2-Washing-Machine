/**
 * @file main_h750.c
 * @brief Washing Machine Control Unit on the WeAct STM32H750VBT6 board
 * @details Input : onboard key K1 (PC13) decoded into click / double-click / hold gestures,
 *                  plus optional external RUN/PAUSE (PA0) and STOP (PA1) push-buttons
 *          Output: dashboard on the 0.96" ST7735 LCD + onboard LED PE3 (mirrors RLED|BLED)
 *          The FSM, debouncer, blinker and gesture decoder are the same portable code that is
 *          unit-tested on the host; only the board drivers in this folder are hardware specific.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "board_h750.h"
#include "washing_machine_fsm.h"
#include "hal_button_engine.h"
#include "hal_button_gesture.h"
#include "hal_led_blinker.h"
#include "wm_single_button.h"
#include "wm_two_button.h"
#include "ui_render.h"
#include "ui_text.h"

#define HOLD_UI_STEP_MS  (50U)

/* Demo build: a "30 minute" cycle is shortened to 30 s so it can be watched live.
 * Build with FULL_CYCLE=1 (make h750 FULL_CYCLE=1) for the real 1800 s cycle. */
#ifndef WM_DEMO_CYCLE_SEC
#define WM_DEMO_CYCLE_SEC  (WM_CYCLE_DURATION_TEST_SEC)
#endif

static wm_context_t g_fsm;
static hal_button_t g_key;
static hal_button_t g_ext_run;
static hal_button_t g_ext_stop;
static hal_gesture_ctx_t g_gesture;      /* K1 (PC13) */
static hal_gesture_ctx_t g_k0_gesture;   /* K0 (external, PA0) */
static wm_coin_staging_t g_staging;

/* Transient message ("READY! CHANGE 050C" / "NOT ENOUGH! REFUND 030C") */
#define NOTICE_MS  (2500U)
static wm_notice_t g_notice = WM_NOTICE_NONE;
static uint32_t g_notice_value = 0U;
static uint32_t g_notice_until_ms = 0U;
static hal_led_blinker_t g_rled;
static hal_led_blinker_t g_bled;

/* Output snapshot mirrored on the LCD (relays have no pins on this board) */
static hal_motor_state_t g_motor = HAL_MOTOR_OFF;
static bool g_pump = false;
static bool g_lock = false;

static void cb_set_rled(hal_led_state_t s) { hal_led_blinker_set_mode(&g_rled, s); }
static void cb_set_bled(hal_led_state_t s) { hal_led_blinker_set_mode(&g_bled, s); }
static void cb_set_motor(hal_motor_state_t m) { g_motor = m; }
static void cb_set_valve(bool open) { (void)open; }
static void cb_set_pump(bool on) { g_pump = on; }
static void cb_set_lock(bool locked) { g_lock = locked; }
static void set_notice(wm_notice_t type, uint32_t value);

static void cb_return_coins(uint32_t cents) {
    set_notice(WM_NOTICE_REFUND, cents);
}

static void cb_cycle_complete(void) {
    g_motor = HAL_MOTOR_OFF;
    g_pump = false;
    g_lock = false;
}

/* ---- UART console: status log + keyboard control (115200 8N1 on PA9/PA10) ---- */
static const char k_help[] =
    "K1 keys: c=click(+10c)  d=double(+20c)  p=hold+release(+50c)  o=hold 1.2s(OK/confirm)\r\n"
    "         a=K0 click(+10c)  b=K0 hold(+20c)  s=STOP (twice)  f=force stop  ?=help\r\n";

static void uart_print_uint(uint32_t v) {
    char tmp[11];
    unsigned n = 0U;
    do {
        tmp[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    } while (v != 0U);
    char one[2] = {0, 0};
    while (n > 0U) {
        one[0] = tmp[--n];
        board_uart_puts(one);
    }
}

static bool log_changed(const ui_view_t *a, const ui_view_t *b) {
    return a->state != b->state || a->phase != b->phase || a->remaining_sec != b->remaining_sec ||
           a->balance_cents != b->balance_cents || a->fault_flags != b->fault_flags ||
           a->motor != b->motor || a->pump != b->pump || a->lock != b->lock;
}

static void log_status(const ui_view_t *v) {
    char line[128];
    board_uart_puts("[");
    uart_print_uint(board_millis() / 1000U);
    board_uart_puts("s] ");
    (void)ui_status_line(line, sizeof(line), v);
    board_uart_puts(line);
    board_uart_puts("\r\n");
}

static void set_notice(wm_notice_t type, uint32_t value) {
    g_notice = type;
    g_notice_value = value;
    g_notice_until_ms = board_millis() + NOTICE_MS;
    board_uart_puts(type == WM_NOTICE_READY ? "NOTICE: READY, change returned " :
                    type == WM_NOTICE_REFUND ? "NOTICE: CANCELLED, deposit returned " :
                                               "NOTICE: NOT ENOUGH MONEY, refund ");
    uart_print_uint(value);
    board_uart_puts("c\r\n");
}

static void k1_event(hal_gesture_t g) {
    wm_notice_t n = WM_NOTICE_NONE;
    uint32_t value = 0U;
    (void)wm_k1_apply(&g_fsm, &g_staging, g, &n, &value);
    if (n != WM_NOTICE_NONE) {
        set_notice(n, value);
    }
}

static void handle_uart_key(int c) {
    switch (c) {
        case 'a': board_uart_puts("> K0 click\r\n");  (void)wm_k0_apply(&g_fsm, &g_staging, HAL_GESTURE_CLICK); break;
        case 'b': board_uart_puts("> K0 hold\r\n");   (void)wm_k0_apply(&g_fsm, &g_staging, HAL_GESTURE_HOLD1); break;
        case 'c': board_uart_puts("> K1 click\r\n");  k1_event(HAL_GESTURE_CLICK); break;
        case 'p': board_uart_puts("> K1 hold+release\r\n"); k1_event(HAL_GESTURE_HOLD1_END); break;
        case 'd': board_uart_puts("> K1 double-click\r\n"); k1_event(HAL_GESTURE_DOUBLE); break;
        case 'o': board_uart_puts("> K1 hold 1.2 s (confirm)\r\n"); k1_event(HAL_GESTURE_HOLD2); break;
        case 's': board_uart_puts("> STOP\r\n");      (void)wm_ext_stop_apply(&g_fsm); break;
        case 'f':
            board_uart_puts("> force stop\r\n");
            (void)wm_single_button_apply(&g_fsm, HAL_GESTURE_HOLD1);
            (void)wm_single_button_apply(&g_fsm, HAL_GESTURE_HOLD2);
            break;
        case '?':
        case 'h': board_uart_puts(k_help); break;
        default: break;
    }
}

static void build_view(ui_view_t *v) {
    v->state = g_fsm.state;
    v->phase = wm_fsm_get_cycle_phase(&g_fsm);
    wm_staging_sync(&g_staging, &g_fsm);
    v->balance_cents = (g_fsm.state == WM_STATE_STANDBY || g_fsm.state == WM_STATE_COLLECTING) ? g_staging.pending_cents : g_fsm.coin_balance_cents;
    v->remaining_sec = g_fsm.remaining_cycle_sec;
    v->total_sec = g_fsm.cycle_duration_setting;
    v->fault_flags = g_fsm.active_error_flags;
    v->rled = hal_led_blinker_get_output(&g_rled);
    v->bled = hal_led_blinker_get_output(&g_bled);
    v->motor = g_motor;
    v->pump = g_pump;
    v->lock = g_lock;
    v->reset_cause = board_reset_cause();
    v->bl_mode = 0U;
    if (g_notice != WM_NOTICE_NONE && (int32_t)(board_millis() - g_notice_until_ms) >= 0) {
        g_notice = WM_NOTICE_NONE;
    }
    v->notice = (uint32_t)g_notice;
    v->notice_value = g_notice_value;
    v->hold_ms = (hal_gesture_hold_ms(&g_gesture) / HOLD_UI_STEP_MS) * HOLD_UI_STEP_MS;
}

/** @brief Deterministic 1 ms housekeeping */
static void process_1ms(uint32_t *sec_acc_ms) {
    wm_fsm_tick_1ms(&g_fsm);
    hal_led_blinker_tick_ms(&g_rled, 1U);
    hal_led_blinker_tick_ms(&g_bled, 1U);

    hal_button_update(&g_key, board_key_pressed(), 1U);
    hal_gesture_t g = hal_gesture_update(&g_gesture, hal_button_is_pressed(&g_key), 1U);
    if (g != HAL_GESTURE_NONE) {
        k1_event(g);
    }

    /* K0 = external button on PA0 (to GND): coin button in STANDBY, RUN/PAUSE otherwise */
    hal_button_update(&g_ext_run, board_ext_run_pressed(), 1U);
    hal_gesture_t g0 = hal_gesture_update(&g_k0_gesture, hal_button_is_pressed(&g_ext_run), 1U);
    if (g0 != HAL_GESTURE_NONE) {
        (void)wm_k0_apply(&g_fsm, &g_staging, g0);
    }

    hal_button_update(&g_ext_stop, board_ext_stop_pressed(), 1U);
    if (hal_button_was_pressed(&g_ext_stop)) {
        (void)wm_ext_stop_apply(&g_fsm);
    }

    (*sec_acc_ms)++;
    if (*sec_acc_ms >= 1000U) {
        *sec_acc_ms = 0U;
        wm_fsm_tick_1s(&g_fsm);
    }
}

int main(void) {
    board_init();
    board_led_set(true);          /* boot stage 1: clocks + GPIO OK -> LED on during LCD bring-up */

    hal_led_blinker_init(&g_rled, true);
    hal_led_blinker_init(&g_bled, true);
    hal_button_init(&g_key, false); /* K1 is active HIGH */
    hal_button_init(&g_ext_run, true);  /* external buttons are active LOW */
    hal_button_init(&g_ext_stop, true);
    hal_gesture_init(&g_gesture);
    hal_gesture_init(&g_k0_gesture);
    wm_staging_init(&g_staging);

    hal_output_callbacks_t cbs = {
        .set_rled = cb_set_rled,
        .set_bled = cb_set_bled,
        .set_motor = cb_set_motor,
        .set_water_valve = cb_set_valve,
        .set_drain_pump = cb_set_pump,
        .set_door_lock = cb_set_lock,
        .on_cycle_complete = cb_cycle_complete,
        .return_coins = cb_return_coins
    };
    wm_fsm_init(&g_fsm, &cbs);
    g_fsm.cycle_duration_setting = WM_DEMO_CYCLE_SEC;

    board_uart_init();
    board_uart_puts("\r\n=== Washing Machine Control Unit - WeAct STM32H750 ===\r\n");
    board_uart_puts("Reset cause: ");
    board_uart_puts(board_reset_cause());
    board_uart_puts("\r\n");
    board_uart_puts(k_help);

    lcd_init();
    board_led_set(false);

    ui_view_t shown;
    ui_view_t now;
    memset(&shown, 0, sizeof(shown));
    shown.state = (wm_state_t)0xFF; /* force first draw */

    ui_view_t logged;
    memset(&logged, 0, sizeof(logged));
    logged.state = (wm_state_t)0xFF;

    uint32_t processed = board_millis();
    uint32_t sec_acc_ms = 0U;
#ifdef BL_EXPERIMENT
    uint32_t bl_mode = 1U;
    uint32_t bl_since = board_millis();
    lcd_backlight_mode(bl_mode);
#endif

    for (;;) {
        uint32_t t = board_millis();
        while (processed != t) {
            processed++;
            process_1ms(&sec_acc_ms);
        }

        build_view(&now);
#ifdef BL_EXPERIMENT
        if ((t - bl_since) >= 4000U) {
            bl_since = t;
            bl_mode = (bl_mode % 4U) + 1U;
            lcd_backlight_mode(bl_mode);
        }
        now.bl_mode = bl_mode;
#endif
        int key;
        while ((key = board_uart_getc()) >= 0) {
            handle_uart_key(key);
        }
        build_view(&now);
        if (log_changed(&now, &logged)) {
            log_status(&now);
            logged = now;
        }
        board_led_set(now.rled || now.bled);
        if (!ui_view_equal(&now, &shown)) {
            ui_render(&now);
            lcd_flush();
            shown = now;
        }
    }
}
