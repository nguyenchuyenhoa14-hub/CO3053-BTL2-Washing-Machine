/**
 * @file test_two_button.c
 * @brief Host unit tests for the K0/K1 front panel and coin staging
 */

#include <stdio.h>
#include "wm_two_button.h"

static int g_failed = 0;

#define CHECK(cond, name) do { \
    if (cond) { printf("  [PASS] %s\n", name); } \
    else { printf("  [FAIL] %s (line %d)\n", name, __LINE__); g_failed++; } \
} while (0)

static void fresh(wm_context_t *c, wm_coin_staging_t *s) {
    wm_fsm_init(c, NULL);
    wm_staging_init(s);
}

int main(void) {
    wm_context_t c;
    wm_coin_staging_t s;
    wm_notice_t n = WM_NOTICE_NONE;
    uint32_t v = 0U;

    printf("=== Two-button front panel tests ===\n");

    /* K1-only coin entry */
    fresh(&c, &s);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_CLICK, &n, &v);
    CHECK(s.pending_cents == 10U && c.coin_balance_cents == 0U, "K1 click: +10c staged, FSM untouched");
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_DOUBLE, &n, &v);
    CHECK(s.pending_cents == 30U, "K1 double-click: +20c");
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD1, &n, &v);
    CHECK(s.pending_cents == 30U, "K1 HOLD1 alone changes nothing (still deciding)");
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD1_END, &n, &v);
    CHECK(s.pending_cents == 80U && c.state == WM_STATE_STANDBY, "K1 hold+release: +50c, still waits for confirm");

    n = WM_NOTICE_NONE;
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD1, &n, &v);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD2, &n, &v);
    CHECK(c.state == WM_STATE_READY && n == WM_NOTICE_READY && v == 30U && s.pending_cents == 0U,
          "K1 hold to 1.2 s with 80c -> READY, 30c change returned");
    CHECK(c.coin_balance_cents == 50U, "FSM balance is exactly the fare");

    fresh(&c, &s);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD1_END, &n, &v);
    n = WM_NOTICE_NONE;
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD2, &n, &v);
    CHECK(c.state == WM_STATE_READY && n == WM_NOTICE_READY && v == 0U, "exactly 50c -> READY, no change");

    fresh(&c, &s);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_CLICK, &n, &v);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_DOUBLE, &n, &v);
    n = WM_NOTICE_NONE;
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD2, &n, &v);
    CHECK(c.state == WM_STATE_STANDBY && n == WM_NOTICE_NOT_ENOUGH && v == 30U && s.pending_cents == 0U,
          "30c confirm -> not enough, 30c refunded, stays STANDBY");

    fresh(&c, &s);
    n = WM_NOTICE_NONE;
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD2, &n, &v);
    CHECK(c.state == WM_STATE_STANDBY && n == WM_NOTICE_NOT_ENOUGH && v == 0U, "confirm with nothing inserted -> not enough");

    fresh(&c, &s);
    for (int i = 0; i < 40; i++) { (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD1_END, &n, &v); }
    CHECK(s.pending_cents == WM_STAGING_MAX_CENTS, "staged coins are capped");

    /* Optional external K0 shortcut */
    fresh(&c, &s);
    (void)wm_k0_apply(&c, &s, HAL_GESTURE_CLICK);
    (void)wm_k0_apply(&c, &s, HAL_GESTURE_HOLD1);
    CHECK(s.pending_cents == 30U, "K0 click +10c, K0 hold +20c");

    /* After READY: K1 click runs, K1 click pauses, K0 click resumes */
    fresh(&c, &s);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD1_END, &n, &v);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD2, &n, &v);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_CLICK, &n, &v);
    CHECK(c.state == WM_STATE_RUNNING, "READY: K1 click -> RUNNING");
    (void)wm_k0_apply(&c, &s, HAL_GESTURE_CLICK);
    CHECK(c.state == WM_STATE_PAUSED, "RUNNING: K0 click -> PAUSED");
    (void)wm_k0_apply(&c, &s, HAL_GESTURE_CLICK);
    CHECK(c.state == WM_STATE_RUNNING, "PAUSED: K0 click -> RUNNING");
    (void)wm_k0_apply(&c, &s, HAL_GESTURE_HOLD1);
    CHECK(c.state == WM_STATE_RUNNING && s.pending_cents == 0U, "K0 hold outside STANDBY adds no money");
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD1, &n, &v);
    (void)wm_k1_apply(&c, &s, HAL_GESTURE_HOLD2, &n, &v);
    CHECK(c.state == WM_STATE_STANDBY, "K1 hold 2 stages while RUNNING -> force STOP");

    /* Fault in STANDBY drops staged coins */
    fresh(&c, &s);
    (void)wm_k0_apply(&c, &s, HAL_GESTURE_CLICK);
    wm_fsm_trigger_fault(&c, WM_FAULT_DOOR_OPEN);
    (void)wm_k0_apply(&c, &s, HAL_GESTURE_HOLD1);
    CHECK(s.pending_cents == 0U && c.state == WM_STATE_ERROR, "ERROR clears staged coins");
    (void)wm_k0_apply(&c, &s, HAL_GESTURE_CLICK);
    CHECK(c.state == WM_STATE_STANDBY && s.pending_cents == 0U, "K0 click in ERROR clears the fault");

    printf(g_failed == 0 ? "ALL TWO-BUTTON TESTS PASSED\n" : "TWO-BUTTON TESTS FAILED\n");
    return g_failed == 0 ? 0 : 1;
}
