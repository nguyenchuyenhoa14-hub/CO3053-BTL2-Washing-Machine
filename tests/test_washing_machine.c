/**
 * @file test_washing_machine.c
 * @brief Automated Unit Test Suite for Washing Machine Control Unit (CO3053 - BTL 2)
 *        Covers 100% of functional requirements, state transitions, and edge cases.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "washing_machine_fsm.h"
#include "mock_hal.h"

/* Terminal formatting for test reporting */
#define ANSI_GREEN  "\033[1;32m"
#define ANSI_RED    "\033[1;31m"
#define ANSI_YELLOW "\033[1;33m"
#define ANSI_CYAN   "\033[1;36m"
#define ANSI_RESET  "\033[0m"

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf(ANSI_RED "  [FAIL] " ANSI_RESET "%s (Line %d): %s\n", __func__, __LINE__, msg); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(test_name) do { \
    printf(ANSI_GREEN "  [PASS] " ANSI_RESET "%s\n", test_name); \
    g_tests_passed++; \
} while(0)

static void helper_advance_ms(wm_context_t *ctx, uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        wm_fsm_tick_1ms(ctx);
    }
}

static void helper_advance_sec(wm_context_t *ctx, uint32_t sec) {
    for (uint32_t i = 0; i < sec; i++) {
        helper_advance_ms(ctx, 1000);
        wm_fsm_tick_1s(ctx);
    }
}

/* -------------------------------------------------------------------------- */
/* TC-01: Sub-threshold Deposit                                              */
/* -------------------------------------------------------------------------- */
static void test_tc01_sub_threshold_deposit(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Init must be STANDBY");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_ON, "Standby RLED must be ON");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "Standby BLED must be OFF");

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_10);
    TEST_ASSERT(ctx.coin_balance_cents == 10, "Balance must be 10¢");
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "10¢ must stay in STANDBY");

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    TEST_ASSERT(ctx.coin_balance_cents == 30, "Balance must be 30¢");
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "30¢ must stay in STANDBY");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_ON, "RLED must stay ON");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "BLED must stay OFF");

    TEST_PASS("TC-01: Sub-threshold Deposit (10¢ + 20¢ stays in STANDBY)");
}

/* -------------------------------------------------------------------------- */
/* TC-02: Exact Threshold Deposit                                            */
/* -------------------------------------------------------------------------- */
static void test_tc02_exact_threshold_deposit(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(ctx.coin_balance_cents == 50, "Balance must be 50¢");
    TEST_ASSERT(ctx.state == WM_STATE_READY, "50¢ must trigger transition to READY");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_OFF, "READY RLED must be OFF");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_ON, "READY BLED must be solid ON");

    TEST_PASS("TC-02: Exact Threshold Deposit (50¢ transitions to READY)");
}

/* -------------------------------------------------------------------------- */
/* TC-03: Surplus Deposit Accumulation                                       */
/* -------------------------------------------------------------------------- */
static void test_tc03_surplus_deposit_accumulation(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "40¢ remains STANDBY");

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    TEST_ASSERT(ctx.coin_balance_cents == 60, "Balance accumulated to 60¢");
    TEST_ASSERT(ctx.state == WM_STATE_READY, "60¢ transitions to READY");

    /* Insert extra coin in READY */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(ctx.coin_balance_cents == 110, "Balance accumulated to 110¢");
    TEST_ASSERT(ctx.state == WM_STATE_READY, "Remains in READY with surplus");

    TEST_PASS("TC-03: Surplus Deposit Accumulation (Accepts 60¢, 110¢ in READY)");
}

/* -------------------------------------------------------------------------- */
/* TC-04: Execution & Zero Refund Policy                                     */
/* -------------------------------------------------------------------------- */
static void test_tc04_execution_and_zero_refund(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* Deposit 70¢ */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    TEST_ASSERT(ctx.state == WM_STATE_READY, "Must be READY");
    TEST_ASSERT(ctx.coin_balance_cents == 70, "Balance is 70¢");

    /* Press RUN */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Must transition to RUNNING");
    TEST_ASSERT(ctx.coin_balance_cents == 0, "MONEY MUST BE CLEARED (Zero Refund)");
    TEST_ASSERT(ctx.remaining_cycle_sec == 1800, "Timer must be initialized to 1800s (30 min)");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_BLINK_1HZ, "BLED must blink at 1Hz in RUNNING");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_OFF, "RLED must be OFF");
    TEST_ASSERT(mock_hal_get_state()->door_locked == true, "Door must be locked during washing");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_AGITATE, "Motor must be active");

    TEST_PASS("TC-04: Execution & Zero Refund (70¢ cleared to 0¢, 30-min timer active)");
}

/* -------------------------------------------------------------------------- */
/* TC-05: Premature Run Attempt                                              */
/* -------------------------------------------------------------------------- */
static void test_tc05_premature_run_attempt(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Must be in STANDBY");

    /* User presses RUN without sufficient money */
    bool handled = wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(handled == false, "Premature RUN must be rejected");
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "State must remain STANDBY");
    TEST_ASSERT(ctx.remaining_cycle_sec == 0, "Timer must not start");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor must remain OFF");

    TEST_PASS("TC-05: Premature RUN Attempt (Ignored when balance < 50¢)");
}

/* -------------------------------------------------------------------------- */
/* TC-06: Normal Pause and Resume                                            */
/* -------------------------------------------------------------------------- */
static void test_tc06_normal_pause_and_resume(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Must be RUNNING");

    /* Wash for 100 seconds */
    helper_advance_sec(&ctx, 100);
    TEST_ASSERT(ctx.remaining_cycle_sec == 1700, "Timer should be at 1700s");

    /* Press PAUSE */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    TEST_ASSERT(ctx.state == WM_STATE_PAUSED, "Must enter PAUSED state");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor must stop when paused");

    /* Press RUN to resume */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Must resume to RUNNING");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_AGITATE, "Motor must resume agitation");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_BLINK_1HZ, "BLED must blink again");

    TEST_PASS("TC-06: Normal Pause and Resume (Actuators safely suspended and resumed)");
}

/* -------------------------------------------------------------------------- */
/* TC-07: Persistent Timer During Pause (CRITICAL REQUIREMENT)               */
/* -------------------------------------------------------------------------- */
static void test_tc07_persistent_timer_during_pause(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);

    /* Wash for 200s */
    helper_advance_sec(&ctx, 200);
    TEST_ASSERT(ctx.remaining_cycle_sec == 1600, "Timer at 1600s");

    /* Enter Pause */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    TEST_ASSERT(ctx.state == WM_STATE_PAUSED, "Must be in PAUSED");

    /* Stay paused for 300 seconds (5 minutes) */
    helper_advance_sec(&ctx, 300);

    /* CRITICAL ASSERTION: The timer must have decremented by 300s during PAUSE! */
    TEST_ASSERT(ctx.remaining_cycle_sec == 1300,
                "Timer MUST continue counting down during pause (1600 - 300 = 1300s)");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF,
                "Motor must remain halted during pause countdown");

    TEST_PASS("TC-07: Persistent Timer in Pause (Timer ticked down from 1600s to 1300s while paused)");
}

/* -------------------------------------------------------------------------- */
/* TC-08: Pause Timeout Termination                                          */
/* -------------------------------------------------------------------------- */
static void test_tc08_pause_timeout_termination(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);
    ctx.cycle_duration_setting = 30; /* Accelerated 30s cycle for test */

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.remaining_cycle_sec == 30, "Timer set to 30s");

    /* Wash 10s then pause */
    helper_advance_sec(&ctx, 10);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    TEST_ASSERT(ctx.state == WM_STATE_PAUSED, "Paused at remaining=20s");

    /* Leave machine paused for 20s until timer expires */
    helper_advance_sec(&ctx, 20);

    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Expired pause timer must return to STANDBY");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_ON, "RLED must be solid ON in Standby");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "BLED must be OFF in Standby");

    TEST_PASS("TC-08: Pause Timeout Termination (Timer expiring in PAUSED resets to STANDBY)");
}

/* -------------------------------------------------------------------------- */
/* TC-09: Single STOP Rejection                                              */
/* -------------------------------------------------------------------------- */
static void test_tc09_single_stop_rejection(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Must be RUNNING");

    /* Single press on STOP */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Single STOP must NOT terminate execution!");
    TEST_ASSERT(ctx.stop_press_count == 1, "Stop count should record 1st press");

    /* Wait 2.0s (> 1.5s window) */
    helper_advance_ms(&ctx, 2000);
    TEST_ASSERT(ctx.stop_press_count == 0, "Double-press window must expire and clear count");
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Machine remains running undisturbed");

    TEST_PASS("TC-09: Single STOP Rejection (Single press does not force stop)");
}

/* -------------------------------------------------------------------------- */
/* TC-10: Force Stop on Double Press                                         */
/* -------------------------------------------------------------------------- */
static void test_tc10_force_stop_on_double_press(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Must be RUNNING");

    /* Press STOP 1st time */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Still running after 1st stop");

    /* Wait 300 ms (well within 1500 ms window) */
    helper_advance_ms(&ctx, 300);

    /* Press STOP 2nd time */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);

    /* MUST IMMEDIATELY FORCE STOP */
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Double STOP must immediately force stop to STANDBY");
    TEST_ASSERT(ctx.remaining_cycle_sec == 0, "Cycle timer must be canceled");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Actuators must be instantly cut off");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_ON, "RLED must be solid ON (Standby)");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "BLED must be OFF");

    TEST_PASS("TC-10: Force Stop on Double Press (2 presses within 300ms forces termination)");
}

/* -------------------------------------------------------------------------- */
/* TC-11: Force Stop from Paused State                                       */
/* -------------------------------------------------------------------------- */
static void test_tc11_force_stop_from_paused(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    TEST_ASSERT(ctx.state == WM_STATE_PAUSED, "Must be PAUSED");

    /* Double press STOP while paused */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);
    helper_advance_ms(&ctx, 400);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);

    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Double STOP in PAUSED must return to STANDBY");
    TEST_ASSERT(ctx.remaining_cycle_sec == 0, "Timer must be cleared");

    TEST_PASS("TC-11: Force Stop from Paused State (Double STOP terminates paused machine)");
}

/* -------------------------------------------------------------------------- */
/* TC-12: Normal 30-min Cycle Completion                                     */
/* -------------------------------------------------------------------------- */
static void test_tc12_normal_cycle_completion(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);
    ctx.cycle_duration_setting = 1800; /* Full 30 minutes */

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);

    /* Run full 1800 seconds */
    helper_advance_sec(&ctx, 1800);

    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Completed 1800s must return to STANDBY");
    TEST_ASSERT(ctx.remaining_cycle_sec == 0, "Timer must be 0");
    TEST_ASSERT(mock_hal_get_state()->cycle_complete_count == 1, "on_cycle_complete must be called once");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_ON, "RLED must be solid ON");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "BLED must be OFF");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor must be stopped");

    TEST_PASS("TC-12: Normal 30-min Cycle Completion (1800s expires naturally to STANDBY)");
}

/* -------------------------------------------------------------------------- */
/* TC-13: Fault Interruption and Recovery                                    */
/* -------------------------------------------------------------------------- */
static void test_tc13_fault_interruption_and_recovery(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Must be RUNNING");

    /* Inject hardware fault */
    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_OCCURRED);
    TEST_ASSERT(ctx.state == WM_STATE_ERROR, "Must transition to ERROR");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_BLINK_2HZ, "RLED must blink at 2Hz in ERROR");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "BLED must be OFF in ERROR");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Actuators must be instantly cut");

    /* Normal buttons must be ignored in ERROR */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_ERROR, "RUN must be rejected in ERROR");

    /* Clear fault */
    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_CLEARED);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Clearing fault must return to STANDBY");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_ON, "RLED returns to solid ON");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "BLED returns to OFF");

    TEST_PASS("TC-13: Fault Interruption and Recovery (Safety shutdown & error reset)");
}

/* -------------------------------------------------------------------------- */
/* TC-14: Rapid Alternating PAUSE / RUN Toggling                             */
/* -------------------------------------------------------------------------- */
static void test_tc14_rapid_pause_run_toggling(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_RUNNING, "Must be RUNNING");

    /* Rapidly toggle PAUSE and RUN 10 times with 5s intervals */
    for (int i = 0; i < 10; i++) {
        helper_advance_sec(&ctx, 5);
        wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
        TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_PAUSED, "Should enter PAUSED");
        TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor must stop in PAUSED");

        helper_advance_sec(&ctx, 5);
        wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
        TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_RUNNING, "Should resume RUNNING");
        TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_AGITATE, "Motor must resume in RUNNING");
    }

    /* 10 cycles * 10 seconds = 100 seconds elapsed total */
    TEST_ASSERT(wm_fsm_get_remaining_seconds(&ctx) == (1800 - 100),
                "Timer must reflect exact 100s elapsed despite 10 pause/run toggles");

    TEST_PASS("TC-14: Rapid Alternating PAUSE/RUN Toggling (Stress test on clock & motor)");
}

/* -------------------------------------------------------------------------- */
/* TC-15: Coin Rejection During Active Cycle                                  */
/* -------------------------------------------------------------------------- */
static void test_tc15_coin_rejection_during_active_cycle(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);

    /* Attempt coin insertion in RUNNING */
    bool rejected = !wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_10);
    TEST_ASSERT(rejected, "Coins in RUNNING must be rejected");
    rejected = !wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(rejected, "Coins in RUNNING must be rejected");
    TEST_ASSERT(wm_fsm_get_balance(&ctx) == 0, "Balance must remain 0¢");

    /* Attempt coin insertion in PAUSED */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    rejected = !wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    TEST_ASSERT(rejected, "Coins in PAUSED must be rejected");
    TEST_ASSERT(wm_fsm_get_balance(&ctx) == 0, "Balance must remain 0¢");

    /* Attempt coin insertion in ERROR */
    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_OCCURRED);
    rejected = !wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(rejected, "Coins in ERROR must be rejected");
    TEST_ASSERT(wm_fsm_get_balance(&ctx) == 0, "Balance must remain 0¢");

    TEST_PASS("TC-15: Coin Rejection During Active Cycle (Coins rejected in RUNNING, PAUSED, ERROR)");
}

/* -------------------------------------------------------------------------- */
/* TC-16: Fault in STANDBY and READY States                                  */
/* -------------------------------------------------------------------------- */
static void test_tc16_fault_in_standby_and_ready(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;

    /* Fault while in STANDBY */
    wm_fsm_init(&ctx, &cbs);
    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_OCCURRED);
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_ERROR, "Fault in STANDBY must go to ERROR");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_BLINK_2HZ, "RLED must blink at 2Hz");
    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_CLEARED);
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_STANDBY, "Recovered to STANDBY");

    /* Fault while in READY */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_READY, "Must be READY");
    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_OCCURRED);
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_ERROR, "Fault in READY must go to ERROR");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "BLED must turn OFF in ERROR");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_BLINK_2HZ, "RLED must blink at 2Hz");

    TEST_PASS("TC-16: Fault in STANDBY and READY States (Consistent error transition & LED signalling)");
}

/* -------------------------------------------------------------------------- */
/* TC-17: Complete Lockout During ERROR State                                */
/* -------------------------------------------------------------------------- */
static void test_tc17_complete_lockout_during_error(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_OCCURRED);
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_ERROR, "Must be in ERROR");

    /* All normal control events must be completely inert */
    TEST_ASSERT(wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN) == false, "RUN blocked in ERROR");
    TEST_ASSERT(wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE) == false, "PAUSE blocked in ERROR");
    TEST_ASSERT(wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP) == false, "STOP blocked in ERROR");
    TEST_ASSERT(wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_10) == false, "COIN_10 blocked in ERROR");
    TEST_ASSERT(wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50) == false, "COIN_50 blocked in ERROR");
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_ERROR, "Must remain locked in ERROR");

    TEST_PASS("TC-17: Complete Lockout During ERROR State (Buttons and coins strictly locked)");
}

/* -------------------------------------------------------------------------- */
/* TC-18: Ready State Double STOP Cancellation                               */
/* -------------------------------------------------------------------------- */
static void test_tc18_ready_state_double_stop_cancellation(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* Deposit 60¢ to reach READY */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_READY, "Must be READY");
    TEST_ASSERT(wm_fsm_get_balance(&ctx) == 60, "Balance is 60¢");

    /* User changes mind: double presses STOP */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);
    helper_advance_ms(&ctx, 250);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);

    /* Must cancel back to STANDBY, balance cleared */
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_STANDBY, "Must cancel back to STANDBY");
    TEST_ASSERT(wm_fsm_get_balance(&ctx) == 0, "Deposit cleared");
    TEST_ASSERT(mock_hal_get_state()->rled == HAL_LED_ON, "RLED must be ON");
    TEST_ASSERT(mock_hal_get_state()->bled == HAL_LED_OFF, "BLED must be OFF");

    TEST_PASS("TC-18: Ready State Double STOP Cancellation (User cancel before run)");
}

/* -------------------------------------------------------------------------- */
/* TC-19: Multiple Isolated Single STOPS Never Force Stop                     */
/* -------------------------------------------------------------------------- */
static void test_tc19_multiple_isolated_single_stops(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_RUNNING, "Must be RUNNING");

    /* 4 single presses, each separated by 2.0s (> 1.5s window) */
    for (int i = 0; i < 4; i++) {
        wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);
        TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_RUNNING, "Single press must not stop");
        helper_advance_ms(&ctx, 2000);
        TEST_ASSERT(ctx.stop_press_count == 0, "Window must expire cleanly");
    }

    TEST_ASSERT(wm_fsm_get_state(&ctx) == WM_STATE_RUNNING, "Machine remains running after 4 isolated stops");

    TEST_PASS("TC-19: Multiple Isolated Single STOPS (Spaced presses never falsely force stop)");
}

/* -------------------------------------------------------------------------- */
/* TC-20: Null Pointer and API Resilience                                    */
/* -------------------------------------------------------------------------- */
static void test_tc20_null_pointer_and_api_resilience(void) {
    /* Test passing NULL pointers to all public functions: must not crash */
    wm_fsm_init(NULL, NULL);
    TEST_ASSERT(wm_fsm_dispatch_event(NULL, WM_EVT_BTN_RUN) == false, "NULL dispatch returns false");
    wm_fsm_tick_1ms(NULL);
    wm_fsm_tick_1s(NULL);
    TEST_ASSERT(wm_fsm_get_state(NULL) == WM_STATE_STANDBY, "NULL state defaults to STANDBY");
    TEST_ASSERT(wm_fsm_get_balance(NULL) == 0, "NULL balance defaults to 0");
    TEST_ASSERT(wm_fsm_get_remaining_seconds(NULL) == 0, "NULL remaining seconds defaults to 0");
    TEST_ASSERT(wm_fsm_get_fault_flags(NULL) == 0, "NULL fault flags defaults to 0");
    TEST_ASSERT(wm_fsm_get_cycle_phase(NULL) == WM_PHASE_IDLE, "NULL cycle phase defaults to IDLE");
    TEST_ASSERT(wm_fsm_can_accept_event(NULL, WM_EVT_COIN_10) == false, "NULL can_accept_event returns false");

    /* Fault mutation on NULL context must safely no-op */
    wm_fsm_trigger_fault(NULL, WM_FAULT_DOOR_OPEN);
    wm_fsm_clear_fault(NULL);

    /* String converters */
    TEST_ASSERT(wm_state_to_str(WM_STATE_STANDBY) != NULL, "Valid state string");
    TEST_ASSERT(wm_event_to_str(WM_EVT_BTN_RUN) != NULL, "Valid event string");
    TEST_ASSERT(wm_fault_to_str(WM_FAULT_NONE) != NULL, "Valid fault string");
    TEST_ASSERT(wm_cycle_phase_to_str(WM_PHASE_IDLE) != NULL, "Valid cycle phase string");

    TEST_PASS("TC-20: Null Pointer and API Resilience (Zero segmentation faults, robust error handling)");
}

/* -------------------------------------------------------------------------- */
/* TC-21: Consecutive Multi-Cycle Sessions (Zero Residue / Deadlock)          */
/* -------------------------------------------------------------------------- */
static void test_tc21_consecutive_multi_cycle_sessions(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);
    ctx.cycle_duration_setting = 30; /* 30s accelerated */

    /* --- Session 1: Full Normal Completion --- */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    helper_advance_sec(&ctx, 30);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "S1: Finished cycle must be in STANDBY");
    TEST_ASSERT(mock_hal_get_state()->cycle_complete_count == 1, "S1: cycle_complete count is 1");

    /* --- Session 2: Immediate Deposit & Force Stop --- */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(ctx.state == WM_STATE_READY, "S2: Deposit reached READY");
    TEST_ASSERT(ctx.coin_balance_cents == 70, "S2: Balance is 70¢");
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "S2: Running");
    TEST_ASSERT(ctx.coin_balance_cents == 0, "S2: Money cleared on RUN");

    helper_advance_sec(&ctx, 5);
    /* Double stop */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);
    helper_advance_ms(&ctx, 200);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "S2: Force stopped to STANDBY");

    /* --- Session 3: Immediate Deposit, Fault & Recovery --- */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(ctx.state == WM_STATE_READY, "S3: Ready for 3rd user");
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "S3: Running 3rd cycle");

    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_OCCURRED);
    TEST_ASSERT(ctx.state == WM_STATE_ERROR, "S3: Fault caught");
    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_CLEARED);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "S3: Fault cleared to STANDBY");

    TEST_PASS("TC-21: Consecutive Multi-Cycle Sessions (Flawless back-to-back operations without leakage)");
}

/* -------------------------------------------------------------------------- */
/* TC-22: Precise Boundary Double-Stop Window Timing (1499ms vs 1501ms)       */
/* -------------------------------------------------------------------------- */
static void test_tc22_boundary_double_stop_timing(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* Part A: 1499ms gap (within 1500ms window) -> MUST FORCE STOP */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Part A: Running");

    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP); /* 1st press */
    helper_advance_ms(&ctx, 1499);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP); /* 2nd press at 1499ms */
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "1499ms press must trigger double-stop force shutdown");

    /* Part B: 1501ms gap (exceeding 1500ms window) -> MUST NOT FORCE STOP */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Part B: Running");

    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP); /* 1st press */
    helper_advance_ms(&ctx, 1501); /* Window expires! */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP); /* Treated as new 1st press */
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "1501ms press must NOT force stop (window expired)");
    TEST_ASSERT(ctx.stop_press_count == 1, "Must be recorded as fresh 1st press");

    TEST_PASS("TC-22: Boundary Double-Stop Timing (Exact 1499ms hit vs 1501ms expiration verified)");
}

/* -------------------------------------------------------------------------- */
/* TC-23: Single STOP Rejection in READY State (Deposit Preserved)            */
/* -------------------------------------------------------------------------- */
static void test_tc23_single_stop_in_ready_preserves_deposit(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* Deposit 60¢ */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    TEST_ASSERT(ctx.state == WM_STATE_READY, "Must be READY");
    TEST_ASSERT(ctx.coin_balance_cents == 60, "Balance must be 60¢");

    /* Single STOP press */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_STOP);
    TEST_ASSERT(ctx.state == WM_STATE_READY, "Single STOP must NOT cancel READY state!");
    TEST_ASSERT(ctx.coin_balance_cents == 60, "Balance must remain 60¢");

    /* Wait 2.0s (> 1.5s window) */
    helper_advance_ms(&ctx, 2000);
    TEST_ASSERT(ctx.stop_press_count == 0, "Stop window expired");
    TEST_ASSERT(ctx.state == WM_STATE_READY, "Still READY after window expiry");
    TEST_ASSERT(ctx.coin_balance_cents == 60, "Balance still preserved at 60¢");

    /* User can now press RUN normally */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "RUN succeeds after rejected single STOP");
    TEST_ASSERT(ctx.coin_balance_cents == 0, "Cleared on RUN");

    TEST_PASS("TC-23: Single STOP in READY (Preserves accumulated deposit against accidental touch)");
}

/* -------------------------------------------------------------------------- */
/* TC-24: Granular Fault Diagnostics & Bitmask Tracking                       */
/* -------------------------------------------------------------------------- */
static void test_tc24_granular_fault_diagnostics(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    TEST_ASSERT(wm_fsm_get_fault_flags(&ctx) == WM_FAULT_NONE, "No fault on init");
    TEST_ASSERT(strcmp(wm_fault_to_str(WM_FAULT_NONE), "NO_FAULT") == 0, "Correct NO_FAULT string");

    /* Trigger Door Latch Open Fault */
    wm_fsm_trigger_fault(&ctx, WM_FAULT_DOOR_OPEN);
    TEST_ASSERT(ctx.state == WM_STATE_ERROR, "State must be ERROR");
    TEST_ASSERT(wm_fsm_get_fault_flags(&ctx) == WM_FAULT_DOOR_OPEN, "Door fault bitmask recorded");
    TEST_ASSERT(strcmp(wm_fault_to_str(wm_fsm_get_fault_flags(&ctx)), "DOOR_LATCH_OPEN") == 0, "Door open string");

    /* Add Motor Overcurrent fault */
    wm_fsm_trigger_fault(&ctx, WM_FAULT_MOTOR_OVERCURRENT);
    TEST_ASSERT(wm_fsm_get_fault_flags(&ctx) == (WM_FAULT_DOOR_OPEN | WM_FAULT_MOTOR_OVERCURRENT), "Both faults recorded");

    /* Clear all faults */
    wm_fsm_clear_fault(&ctx);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Must recover to STANDBY");
    TEST_ASSERT(wm_fsm_get_fault_flags(&ctx) == WM_FAULT_NONE, "Fault flags cleared");

    TEST_PASS("TC-24: Granular Fault Diagnostics (Multi-sensor bitmask tracking and string reports)");
}

/* -------------------------------------------------------------------------- */
/* TC-25: Arithmetic Overflow Resilience in Coin Accumulation                 */
/* -------------------------------------------------------------------------- */
static void test_tc25_arithmetic_overflow_resilience(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* Artificially simulate boundary near UINT32_MAX */
    ctx.coin_balance_cents = UINT32_MAX - 10;
    ctx.state = WM_STATE_READY;

    /* Deposit 50¢: Must safely guard against integer wrap-around */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(ctx.coin_balance_cents >= (UINT32_MAX - 10), "Balance must never wrap around to 0 on overflow");
    TEST_ASSERT(ctx.state == WM_STATE_READY, "Must stay READY");

    TEST_PASS("TC-25: Arithmetic Overflow Resilience (MISRA-C Rule 12.4 wrap-around defense)");
}

/* -------------------------------------------------------------------------- */
/* TC-26: Multi-Phase Wash Profile (Agitate -> Spin & Drain -> Complete)      */
/* -------------------------------------------------------------------------- */
static void test_tc26_multi_phase_wash_profile(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);
    ctx.cycle_duration_setting = 60; /* 60s cycle: spin threshold is at 60/6 = 10s */

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Must be RUNNING");

    /* During first 49s (remaining 60 -> 11): Agitation phase */
    helper_advance_sec(&ctx, 49);
    TEST_ASSERT(ctx.remaining_cycle_sec == 11, "At 11s");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_AGITATE, "Agitate phase before 10s threshold");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == false, "Drain pump OFF during agitation");
    TEST_ASSERT(mock_hal_get_state()->door_locked == true, "Door locked");

    /* Advance 1 more second: reaches 10s -> Enters Spin & Drain phase! */
    helper_advance_sec(&ctx, 1);
    TEST_ASSERT(ctx.remaining_cycle_sec == 10, "Reached 10s spin threshold");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_SPIN, "Motor transitioned to high-speed SPIN");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == true, "Drain pump active during spin");

    /* Pause during spin */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    TEST_ASSERT(ctx.state == WM_STATE_PAUSED, "Paused during spin");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor cut in pause");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == false, "Pump cut in pause");

    /* Resume from pause: must restore spin and drain! */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Resumed");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_SPIN, "Resumed back to SPIN");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == true, "Resumed drain pump");

    /* Complete remaining 10 seconds */
    helper_advance_sec(&ctx, 10);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Completed to STANDBY");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor shut off");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == false, "Pump shut off");
    TEST_ASSERT(mock_hal_get_state()->door_locked == false, "Door unlocked");
    TEST_ASSERT(mock_hal_get_state()->cycle_complete_count == 1, "Completed callback");

    TEST_PASS("TC-26: Multi-Phase Wash Profile (Agitate -> Spin/Drain -> Complete verified)");
}

/* -------------------------------------------------------------------------- */
/* TC-27: Event Acceptance Query Protocol (wm_fsm_can_accept_event)           */
/* -------------------------------------------------------------------------- */
static void test_tc27_event_acceptance_query_protocol(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* NULL context safety */
    TEST_ASSERT(!wm_fsm_can_accept_event(NULL, WM_EVT_COIN_10), "NULL context rejects events");

    /* 1. STANDBY State */
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "State is STANDBY");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_10), "STANDBY accepts COIN_10");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_20), "STANDBY accepts COIN_20");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_50), "STANDBY accepts COIN_50");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_RUN), "STANDBY rejects RUN");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_PAUSE), "STANDBY rejects PAUSE");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_STOP), "STANDBY rejects STOP");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_FAULT_OCCURRED), "STANDBY accepts FAULT");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_FAULT_CLEARED), "STANDBY rejects FAULT_CLEARED");

    /* 2. READY State */
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(ctx.state == WM_STATE_READY, "State is READY");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_10), "READY accepts additional coins");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_RUN), "READY accepts RUN");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_STOP), "READY accepts STOP");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_PAUSE), "READY rejects PAUSE");

    /* 3. RUNNING State */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "State is RUNNING");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_10), "RUNNING rejects coins");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_RUN), "RUNNING rejects RUN");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_PAUSE), "RUNNING accepts PAUSE");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_STOP), "RUNNING accepts STOP");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_TIMER_TICK_1S), "RUNNING accepts 1S tick");

    /* 4. PAUSED State */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    TEST_ASSERT(ctx.state == WM_STATE_PAUSED, "State is PAUSED");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_10), "PAUSED rejects coins");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_RUN), "PAUSED accepts RUN (resume)");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_PAUSE), "PAUSED rejects redundant PAUSE");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_STOP), "PAUSED accepts STOP");

    /* 5. ERROR State */
    wm_fsm_dispatch_event(&ctx, WM_EVT_FAULT_OCCURRED);
    TEST_ASSERT(ctx.state == WM_STATE_ERROR, "State is ERROR");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_10), "ERROR rejects coins");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_RUN), "ERROR rejects RUN");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_PAUSE), "ERROR rejects PAUSE");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_BTN_STOP), "ERROR rejects STOP");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_FAULT_OCCURRED), "ERROR rejects redundant FAULT");
    TEST_ASSERT(wm_fsm_can_accept_event(&ctx, WM_EVT_FAULT_CLEARED), "ERROR accepts FAULT_CLEARED");

    TEST_PASS("TC-27: Event Acceptance Query Protocol (Deterministic event filtering across all 5 states)");
}

/* -------------------------------------------------------------------------- */
/* TC-28: Cycle Sub-Phase Query & Enum Decoders (wm_fsm_get_cycle_phase)       */
/* -------------------------------------------------------------------------- */
static void test_tc28_cycle_sub_phase_query(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);
    ctx.cycle_duration_setting = 60; /* 60s total, spin at <= 10s */

    /* NULL context safety */
    TEST_ASSERT(wm_fsm_get_cycle_phase(NULL) == WM_PHASE_IDLE, "NULL ctx yields IDLE phase");

    /* Standby & Ready phase */
    TEST_ASSERT(wm_fsm_get_cycle_phase(&ctx) == WM_PHASE_IDLE, "STANDBY yields IDLE phase");
    TEST_ASSERT(strcmp(wm_cycle_phase_to_str(WM_PHASE_IDLE), "IDLE") == 0, "IDLE phase string");

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    TEST_ASSERT(wm_fsm_get_cycle_phase(&ctx) == WM_PHASE_IDLE, "READY yields IDLE phase");

    /* Start washing: Agitate phase */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(wm_fsm_get_cycle_phase(&ctx) == WM_PHASE_WASH_AGITATE, "Active washing starts in AGITATE");
    TEST_ASSERT(strcmp(wm_cycle_phase_to_str(WM_PHASE_WASH_AGITATE), "WASH_AGITATE") == 0, "AGITATE string");

    /* Advance to remaining 11s (> 10s): Still AGITATE */
    helper_advance_sec(&ctx, 49);
    TEST_ASSERT(wm_fsm_get_cycle_phase(&ctx) == WM_PHASE_WASH_AGITATE, "11s remaining is AGITATE");

    /* Advance to 10s: Enters FINAL_SPIN */
    helper_advance_sec(&ctx, 1);
    TEST_ASSERT(wm_fsm_get_cycle_phase(&ctx) == WM_PHASE_FINAL_SPIN, "10s remaining is FINAL_SPIN");
    TEST_ASSERT(strcmp(wm_cycle_phase_to_str(WM_PHASE_FINAL_SPIN), "FINAL_SPIN") == 0, "FINAL_SPIN string");

    /* Pause during final spin retains FINAL_SPIN phase */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    TEST_ASSERT(wm_fsm_get_cycle_phase(&ctx) == WM_PHASE_FINAL_SPIN, "PAUSED retains FINAL_SPIN phase");

    /* Complete cycle */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    helper_advance_sec(&ctx, 10);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Cycle finished");
    TEST_ASSERT(wm_fsm_get_cycle_phase(&ctx) == WM_PHASE_IDLE, "Back to IDLE phase");

    /* Unknown phase string fallback */
    TEST_ASSERT(strcmp(wm_cycle_phase_to_str((wm_cycle_phase_t)99), "UNKNOWN_PHASE") == 0, "Default unknown phase string");

    TEST_PASS("TC-28: Cycle Sub-Phase Query & Enum Decoders (Correct phase detection throughout cycle)");
}

/* -------------------------------------------------------------------------- */
/* TC-29: Pause Across Phase Boundary (Continuous timer crosses into Spin)    */
/* -------------------------------------------------------------------------- */
static void test_tc29_pause_across_phase_boundary_transition(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* 60s scaled cycle: spin threshold is <= 10s */
    ctx.cycle_duration_setting = 60;

    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);

    /* 1. Advance to 15s remaining (Main Agitation phase) */
    helper_advance_sec(&ctx, 45);
    TEST_ASSERT(ctx.remaining_cycle_sec == 15, "15s remaining");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_AGITATE, "Agitating drum");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == false, "Pump off during agitate");

    /* 2. Pause machine during Agitation */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_PAUSE);
    TEST_ASSERT(ctx.state == WM_STATE_PAUSED, "Machine in PAUSED");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor cut in pause");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == false, "Pump remains off in pause");

    /* 3. Advance timer by 7s WHILE PAUSED (15s - 7s = 8s <= 10s spin threshold) */
    helper_advance_sec(&ctx, 7);
    TEST_ASSERT(ctx.state == WM_STATE_PAUSED, "Still in PAUSED state");
    TEST_ASSERT(ctx.remaining_cycle_sec == 8, "Timer decremented to 8s during pause");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor still halted");

    /* 4. Resume execution: must recognize new phase and start SPIN + DRAIN! */
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    TEST_ASSERT(ctx.state == WM_STATE_RUNNING, "Resumed to RUNNING");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_SPIN, "Dynamically resumed in SPIN mode");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == true, "Drain pump active for spin dry");

    /* 5. Complete remaining 8s */
    helper_advance_sec(&ctx, 8);
    TEST_ASSERT(ctx.state == WM_STATE_STANDBY, "Cycle completed cleanly to STANDBY");
    TEST_ASSERT(mock_hal_get_state()->motor == HAL_MOTOR_OFF, "Motor off");
    TEST_ASSERT(mock_hal_get_state()->drain_pump_on == false, "Pump off");
    TEST_ASSERT(mock_hal_get_state()->door_locked == false, "Door unlocked");

    TEST_PASS("TC-29: Pause Across Phase Boundary (Continuous timer crosses into Spin)");
}

/* -------------------------------------------------------------------------- */
/* TC-30: MISRA-C Boundary & Corrupted Enum Resilience                       */
/* -------------------------------------------------------------------------- */
static void test_tc30_misra_c_boundary_and_corrupted_enum_resilience(void) {
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* 1. Invalid event enum dispatched: must safely return false */
    TEST_ASSERT(!wm_fsm_dispatch_event(&ctx, (wm_event_t)999), "Invalid event returns false");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, (wm_event_t)999), "Invalid event cannot be accepted");

    /* 2. Corrupted state enum in context: must hit default branch safely */
    ctx.state = (wm_state_t)999;
    TEST_ASSERT(!wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN), "Corrupted state rejects event");
    TEST_ASSERT(!wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_10), "Corrupted state rejects acceptance");
    TEST_ASSERT(strcmp(wm_state_to_str(ctx.state), "UNKNOWN") == 0, "Corrupted state decodes to UNKNOWN");
    TEST_ASSERT(strcmp(wm_event_to_str((wm_event_t)999), "UNKNOWN_EVENT") == 0, "Corrupted event decodes to UNKNOWN_EVENT");
    TEST_ASSERT(strcmp(wm_cycle_phase_to_str((wm_cycle_phase_t)999), "UNKNOWN_PHASE") == 0, "Corrupted phase decodes to UNKNOWN_PHASE");

    /* 3. Full operational cycle with 100% NULL callbacks (Zero hardware dependency) */
    wm_context_t ctx_null_cb;
    wm_fsm_init(&ctx_null_cb, NULL);
    TEST_ASSERT(ctx_null_cb.state == WM_STATE_STANDBY, "Null callbacks init to STANDBY");

    /* STANDBY -> READY */
    TEST_ASSERT(wm_fsm_dispatch_event(&ctx_null_cb, WM_EVT_COIN_50), "Accepts 50c with null callbacks");
    TEST_ASSERT(ctx_null_cb.state == WM_STATE_READY, "Transitions to READY with null callbacks");

    /* READY -> RUNNING */
    TEST_ASSERT(wm_fsm_dispatch_event(&ctx_null_cb, WM_EVT_BTN_RUN), "Runs with null callbacks");
    TEST_ASSERT(ctx_null_cb.state == WM_STATE_RUNNING, "Transitions to RUNNING with null callbacks");
    wm_fsm_tick_1s(&ctx_null_cb);

    /* RUNNING -> PAUSED */
    TEST_ASSERT(wm_fsm_dispatch_event(&ctx_null_cb, WM_EVT_BTN_PAUSE), "Pauses with null callbacks");
    TEST_ASSERT(ctx_null_cb.state == WM_STATE_PAUSED, "Transitions to PAUSED with null callbacks");

    /* Double STOP in PAUSED -> STANDBY */
    wm_fsm_dispatch_event(&ctx_null_cb, WM_EVT_BTN_STOP);
    wm_fsm_dispatch_event(&ctx_null_cb, WM_EVT_BTN_STOP);
    TEST_ASSERT(ctx_null_cb.state == WM_STATE_STANDBY, "Force stops with null callbacks");

    /* Fault trigger & recovery with null callbacks */
    wm_fsm_trigger_fault(&ctx_null_cb, WM_FAULT_DOOR_OPEN);
    TEST_ASSERT(ctx_null_cb.state == WM_STATE_ERROR, "Faults with null callbacks");
    wm_fsm_clear_fault(&ctx_null_cb);
    TEST_ASSERT(ctx_null_cb.state == WM_STATE_STANDBY, "Recovers with null callbacks");

    TEST_PASS("TC-30: MISRA-C Boundary & Corrupted Enum Resilience (100% defensive branch safety)");
}

/* -------------------------------------------------------------------------- */
/* Main Test Runner                                                          */
/* -------------------------------------------------------------------------- */
int main(void) {
    printf(ANSI_CYAN "============================================================\n" ANSI_RESET);
    printf(ANSI_CYAN " BTL 2: Washing Machine Control Unit - Verification Suite\n" ANSI_RESET);
    printf(ANSI_CYAN " CO3053 Embedded Systems - HCMUT\n" ANSI_RESET);
    printf(ANSI_CYAN "============================================================\n\n" ANSI_RESET);

    test_tc01_sub_threshold_deposit();
    test_tc02_exact_threshold_deposit();
    test_tc03_surplus_deposit_accumulation();
    test_tc04_execution_and_zero_refund();
    test_tc05_premature_run_attempt();
    test_tc06_normal_pause_and_resume();
    test_tc07_persistent_timer_during_pause();
    test_tc08_pause_timeout_termination();
    test_tc09_single_stop_rejection();
    test_tc10_force_stop_on_double_press();
    test_tc11_force_stop_from_paused();
    test_tc12_normal_cycle_completion();
    test_tc13_fault_interruption_and_recovery();
    test_tc14_rapid_pause_run_toggling();
    test_tc15_coin_rejection_during_active_cycle();
    test_tc16_fault_in_standby_and_ready();
    test_tc17_complete_lockout_during_error();
    test_tc18_ready_state_double_stop_cancellation();
    test_tc19_multiple_isolated_single_stops();
    test_tc20_null_pointer_and_api_resilience();
    test_tc21_consecutive_multi_cycle_sessions();
    test_tc22_boundary_double_stop_timing();
    test_tc23_single_stop_in_ready_preserves_deposit();
    test_tc24_granular_fault_diagnostics();
    test_tc25_arithmetic_overflow_resilience();
    test_tc26_multi_phase_wash_profile();
    test_tc27_event_acceptance_query_protocol();
    test_tc28_cycle_sub_phase_query();
    test_tc29_pause_across_phase_boundary_transition();
    test_tc30_misra_c_boundary_and_corrupted_enum_resilience();

    printf("\n" ANSI_CYAN "============================================================\n" ANSI_RESET);
    if (g_tests_failed == 0) {
        printf(ANSI_GREEN " ALL %d TESTS PASSED SUCCESSFULLY! (100%% Specification & Transition Coverage)\n" ANSI_RESET, g_tests_passed);
        printf(ANSI_CYAN "============================================================\n" ANSI_RESET);
        return 0;
    } else {
        printf(ANSI_RED " TEST SUITE FAILED: %d Passed, %d Failed\n" ANSI_RESET, g_tests_passed, g_tests_failed);
        printf(ANSI_CYAN "============================================================\n" ANSI_RESET);
        return 1;
    }
}
