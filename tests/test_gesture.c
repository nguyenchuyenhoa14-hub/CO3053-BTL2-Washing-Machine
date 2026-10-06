/**
 * @file test_gesture.c
 * @brief Host unit tests for the single-button gesture decoder and its FSM mapping
 */

#include <stdio.h>
#include <stdbool.h>
#include "hal_button_gesture.h"
#include "wm_single_button.h"

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name) do { \
    if (cond) { printf("  [PASS] %s\n", name); g_passed++; } \
    else { printf("  [FAIL] %s (line %d)\n", name, __LINE__); g_failed++; } \
} while (0)

/** Hold the key level for ms; return the last non-NONE gesture and count of events */
static hal_gesture_t run(hal_gesture_ctx_t *g, bool pressed, uint32_t ms, int *events) {
    hal_gesture_t last = HAL_GESTURE_NONE;
    for (uint32_t i = 0U; i < ms; i++) {
        hal_gesture_t e = hal_gesture_update(g, pressed, 1U);
        if (e != HAL_GESTURE_NONE) {
            last = e;
            (*events)++;
        }
    }
    return last;
}

static void test_click(void) {
    hal_gesture_ctx_t g; int n = 0;
    hal_gesture_init(&g);
    (void)run(&g, true, 100U, &n);
    (void)run(&g, false, HAL_GESTURE_DOUBLE_GAP_MS - 10U, &n);
    CHECK(n == 0, "click is delayed until the double-click gap expires");
    hal_gesture_t e = run(&g, false, 20U, &n);
    CHECK(e == HAL_GESTURE_CLICK && n == 1, "short press -> exactly one CLICK");
}

static void test_double(void) {
    hal_gesture_ctx_t g; int n = 0;
    hal_gesture_init(&g);
    (void)run(&g, true, 80U, &n);
    (void)run(&g, false, 120U, &n);
    (void)run(&g, true, 80U, &n);
    hal_gesture_t e = run(&g, false, 500U, &n);
    CHECK(e == HAL_GESTURE_DOUBLE && n == 1, "two short presses -> exactly one DOUBLE (no CLICK)");
}

static void test_hold(void) {
    hal_gesture_ctx_t g; int n = 0;
    hal_gesture_init(&g);
    hal_gesture_t e = run(&g, true, HAL_GESTURE_HOLD1_MS + 5U, &n);
    CHECK(e == HAL_GESTURE_HOLD1 && n == 1, "hold 0.6 s -> HOLD1 while still pressed");
    e = run(&g, true, HAL_GESTURE_HOLD2_MS - HAL_GESTURE_HOLD1_MS, &n);
    CHECK(e == HAL_GESTURE_HOLD2 && n == 2, "keep holding to 1.2 s -> HOLD2");
    e = run(&g, true, 3000U, &n);
    CHECK(n == 2, "no repeat events while the key stays down");
    (void)run(&g, false, 600U, &n);
    CHECK(n == 2, "no CLICK after a hold is released");
}

static void test_hold_release(void) {
    hal_gesture_ctx_t g; int n = 0;
    hal_gesture_init(&g);
    (void)run(&g, true, HAL_GESTURE_HOLD1_MS + 50U, &n);
    hal_gesture_t e = run(&g, false, 10U, &n);
    CHECK(e == HAL_GESTURE_HOLD1_END && n == 2, "release between 0.6 s and 1.2 s -> HOLD1_END");
    hal_gesture_init(&g); n = 0;
    (void)run(&g, true, HAL_GESTURE_HOLD2_MS + 50U, &n);
    e = run(&g, false, 10U, &n);
    CHECK(n == 2 && e == HAL_GESTURE_NONE, "release after 1.2 s -> no HOLD1_END");
}

static void test_double_then_hold(void) {
    hal_gesture_ctx_t g; int n = 0;
    hal_gesture_init(&g);
    (void)run(&g, true, 80U, &n);
    (void)run(&g, false, 100U, &n);
    hal_gesture_t e = run(&g, true, HAL_GESTURE_HOLD1_MS + 5U, &n);
    CHECK(e == HAL_GESTURE_HOLD1 && n == 1, "press-release-hold -> HOLD1, pending click dropped");
}

static void test_hold_window_fits_stop(void) {
    CHECK((HAL_GESTURE_HOLD2_MS - HAL_GESTURE_HOLD1_MS) < WM_DOUBLE_PRESS_WINDOW_MS,
          "HOLD1->HOLD2 interval fits inside the FSM STOP double-press window");
}

static void test_fsm_flow(void) {
    wm_context_t c;
    wm_fsm_init(&c, NULL);
    CHECK(c.state == WM_STATE_STANDBY, "init STANDBY");

    (void)wm_single_button_apply(&c, HAL_GESTURE_DOUBLE);
    CHECK(c.state == WM_STATE_COLLECTING && c.coin_balance_cents == 10U, "DOUBLE in STANDBY -> +10c (COLLECTING)");
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    CHECK(c.state == WM_STATE_READY, "CLICK in STANDBY -> +50c -> READY");
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    CHECK(c.state == WM_STATE_RUNNING, "CLICK in READY -> RUNNING");
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    CHECK(c.state == WM_STATE_PAUSED, "CLICK in RUNNING -> PAUSED");
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    CHECK(c.state == WM_STATE_RUNNING, "CLICK in PAUSED -> RUNNING (resume)");
    (void)wm_single_button_apply(&c, HAL_GESTURE_DOUBLE);
    CHECK(c.state == WM_STATE_ERROR && c.active_error_flags == WM_FAULT_DOOR_OPEN,
          "DOUBLE while RUNNING -> door-open fault -> ERROR");
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
#if WM_ERROR_RESUMES_CYCLE
    CHECK(c.state == WM_STATE_PAUSED && c.active_error_flags == WM_FAULT_NONE, "CLICK in ERROR -> cleared, cycle resumes in PAUSED");
    (void)wm_single_button_apply(&c, HAL_GESTURE_HOLD1);
    (void)wm_single_button_apply(&c, HAL_GESTURE_HOLD2);
#endif
    CHECK(c.state == WM_STATE_STANDBY && c.active_error_flags == WM_FAULT_NONE, "STANDBY after clearing / force stop");

    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    CHECK(c.state == WM_STATE_RUNNING, "restart cycle");
    (void)wm_single_button_apply(&c, HAL_GESTURE_HOLD1);
    CHECK(c.state == WM_STATE_RUNNING, "HOLD1 only arms STOP");
    (void)wm_single_button_apply(&c, HAL_GESTURE_HOLD2);
    CHECK(c.state == WM_STATE_STANDBY, "HOLD2 completes force STOP -> STANDBY");

    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    (void)wm_single_button_apply(&c, HAL_GESTURE_DOUBLE);
    CHECK(c.state == WM_STATE_ERROR, "fault injected");
    (void)wm_single_button_apply(&c, HAL_GESTURE_HOLD1);
    (void)wm_single_button_apply(&c, HAL_GESTURE_HOLD2);
    CHECK(c.state != WM_STATE_ERROR && c.active_error_flags == WM_FAULT_NONE,
          "HOLD in ERROR clears the fault (deposit/cycle context restored by the FSM)");
}

static void test_external_buttons(void) {
    wm_context_t c;
    wm_fsm_init(&c, NULL);
    CHECK(!wm_ext_run_pause_apply(&c) && c.state == WM_STATE_STANDBY, "ext RUN/PAUSE ignored in STANDBY");
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    (void)wm_ext_run_pause_apply(&c);
    CHECK(c.state == WM_STATE_RUNNING, "ext RUN/PAUSE: READY -> RUNNING");
    (void)wm_ext_run_pause_apply(&c);
    CHECK(c.state == WM_STATE_PAUSED, "ext RUN/PAUSE: RUNNING -> PAUSED");
    (void)wm_ext_run_pause_apply(&c);
    CHECK(c.state == WM_STATE_RUNNING, "ext RUN/PAUSE: PAUSED -> RUNNING");
    (void)wm_ext_stop_apply(&c);
    CHECK(c.state == WM_STATE_RUNNING, "ext STOP: single press only arms");
    (void)wm_ext_stop_apply(&c);
    CHECK(c.state == WM_STATE_STANDBY, "ext STOP: double press -> STANDBY");
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    (void)wm_single_button_apply(&c, HAL_GESTURE_CLICK);
    (void)wm_single_button_apply(&c, HAL_GESTURE_DOUBLE);
    (void)wm_ext_stop_apply(&c);
    CHECK(c.state != WM_STATE_ERROR && c.active_error_flags == WM_FAULT_NONE, "ext STOP clears ERROR");
}

int main(void) {
    printf("=== Single-button gesture tests ===\n");
    test_click();
    test_double();
    test_hold();
    test_hold_release();
    test_double_then_hold();
    test_hold_window_fits_stop();
    test_fsm_flow();
    test_external_buttons();
    printf("Passed: %d, Failed: %d\n", g_passed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
