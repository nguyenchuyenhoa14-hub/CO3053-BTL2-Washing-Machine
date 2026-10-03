/**
 * @file test_hal_engines.c
 * @brief Automated Unit Tests for Hardware HAL Input & Output Engines
 *        Covers debouncing, coin pulse validation, LED blink timing, and actuator interlocks.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hal_button_engine.h"
#include "hal_led_blinker.h"

#define ANSI_GREEN  "\033[1;32m"
#define ANSI_RED    "\033[1;31m"
#define ANSI_CYAN   "\033[1;36m"
#define ANSI_RESET  "\033[0m"

static int g_hal_passed = 0;
static int g_hal_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf(ANSI_RED "  [FAIL] " ANSI_RESET "%s (Line %d): %s\n", __func__, __LINE__, msg); \
        g_hal_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(test_name) do { \
    printf(ANSI_GREEN "  [PASS] " ANSI_RESET "%s\n", test_name); \
    g_hal_passed++; \
} while(0)

/* -------------------------------------------------------------------------- */
/* Test 1: Button Debounce & Glitch Rejection                                 */
/* -------------------------------------------------------------------------- */
static void test_button_debouncer_glitch_rejection(void) {
    hal_button_t btn;
    hal_button_init(&btn, true); /* Active-low: GND = pressed */

    TEST_ASSERT(!hal_button_is_pressed(&btn), "Initially unpressed");
    TEST_ASSERT(!hal_button_was_pressed(&btn), "No initial press event");

    /* 1. Simulate electrical contact bounce/glitch: 10ms spike to GND */
    for (int i = 0; i < 10; i++) {
        hal_button_update(&btn, false, 1); /* false = LOW = spike */
    }
    /* Glitch goes away before 30ms threshold */
    hal_button_update(&btn, true, 1);
    TEST_ASSERT(!hal_button_is_pressed(&btn), "10ms glitch must be rejected");
    TEST_ASSERT(!hal_button_was_pressed(&btn), "No event generated for 10ms glitch");

    /* 2. Sustained press: held LOW for full 30ms */
    for (int i = 0; i < 30; i++) {
        hal_button_update(&btn, false, 1);
    }
    TEST_ASSERT(hal_button_is_pressed(&btn), "Sustained press recognized after 30ms");
    TEST_ASSERT(hal_button_was_pressed(&btn), "Press event latched");
    TEST_ASSERT(!hal_button_was_pressed(&btn), "Event cleared upon reading");

    /* 3. Button release */
    for (int i = 0; i < 30; i++) {
        hal_button_update(&btn, true, 1);
    }
    TEST_ASSERT(!hal_button_is_pressed(&btn), "Released recognized after 30ms");
    TEST_ASSERT(hal_button_was_released(&btn), "Release event latched");
    TEST_ASSERT(!hal_button_was_released(&btn), "Release event cleared on read");

    TEST_PASS("HAL-01: Button Debounce (30ms glitch rejection and edge detection verified)");
}

/* -------------------------------------------------------------------------- */
/* Test 2: Coin Pulse Train Validation (10¢, 20¢, 50¢, Invalid)               */
/* -------------------------------------------------------------------------- */
static void test_coin_pulse_detector(void) {
    hal_coin_pulse_detector_t det;
    hal_coin_pulse_init(&det, true, 150); /* 150ms inter-pulse silence timeout */

    /* Helper lambda/loop to simulate a single clean pulse (30ms LOW, 30ms HIGH) */
    #define SEND_PULSE() do { \
        for (int ms = 0; ms < 30; ms++) { hal_coin_pulse_update(&det, false, 1); } \
        for (int ms = 0; ms < 30; ms++) { hal_coin_pulse_update(&det, true, 1); } \
    } while(0)

    #define ADVANCE_SILENCE(ms_count) do { \
        for (int ms = 0; ms < ms_count; ms++) { hal_coin_pulse_update(&det, true, 1); } \
    } while(0)

    /* Test Case A: 1 pulse -> 10¢ */
    SEND_PULSE();
    ADVANCE_SILENCE(160);
    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 10, "1 pulse decodes to 10¢");
    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 0, "Coin latch cleared");

    /* Test Case B: 2 pulses -> 20¢ */
    SEND_PULSE();
    ADVANCE_SILENCE(40); /* Short gap between pulses (< 150ms) */
    SEND_PULSE();
    ADVANCE_SILENCE(160); /* Final silence */
    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 20, "2 pulses decode to 20¢");

    /* Test Case C: 5 pulses -> 50¢ */
    for (int p = 0; p < 5; p++) {
        SEND_PULSE();
        ADVANCE_SILENCE(30);
    }
    ADVANCE_SILENCE(160);
    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 50, "5 pulses decode to 50¢");

    /* Test Case D: 3 pulses -> Invalid denomination */
    for (int p = 0; p < 3; p++) {
        SEND_PULSE();
        ADVANCE_SILENCE(30);
    }
    ADVANCE_SILENCE(160);
    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 0, "3 pulses rejected as invalid coin");

    TEST_PASS("HAL-02: Coin Pulse Validation (Multi-pulse train decoding verified)");
}

/* -------------------------------------------------------------------------- */
/* Test 3: LED Blinker Waveform Timing (1.0 Hz and 2.0 Hz)                    */
/* -------------------------------------------------------------------------- */
static void test_led_blinker_waveforms(void) {
    hal_led_blinker_t blinker;
    hal_led_blinker_init(&blinker, true); /* Active-high */

    /* 1. Solid OFF and Solid ON */
    hal_led_blinker_set_mode(&blinker, HAL_LED_OFF);
    TEST_ASSERT(!hal_led_blinker_get_output(&blinker), "Solid OFF is LOW");

    hal_led_blinker_set_mode(&blinker, HAL_LED_ON);
    TEST_ASSERT(hal_led_blinker_get_output(&blinker), "Solid ON is HIGH");

    /* 2. 1.0 Hz Blinking (500ms ON / 500ms OFF) */
    hal_led_blinker_set_mode(&blinker, HAL_LED_BLINK_1HZ);
    TEST_ASSERT(hal_led_blinker_get_output(&blinker) == true, "1Hz starts HIGH");

    /* Advance 499ms: Still HIGH */
    hal_led_blinker_tick_ms(&blinker, 499);
    TEST_ASSERT(hal_led_blinker_get_output(&blinker) == true, "1Hz at 499ms remains HIGH");

    /* Advance 1ms (reaching 500ms): Toggles to LOW */
    hal_led_blinker_tick_ms(&blinker, 1);
    TEST_ASSERT(hal_led_blinker_get_output(&blinker) == false, "1Hz at 500ms toggles to LOW");

    /* Advance 500ms: Toggles back to HIGH */
    hal_led_blinker_tick_ms(&blinker, 500);
    TEST_ASSERT(hal_led_blinker_get_output(&blinker) == true, "1Hz at 1000ms completes full cycle");

    /* 3. 2.0 Hz Blinking (250ms ON / 250ms OFF) */
    hal_led_blinker_set_mode(&blinker, HAL_LED_BLINK_2HZ);
    TEST_ASSERT(hal_led_blinker_get_output(&blinker) == true, "2Hz starts HIGH");

    /* Advance 250ms: Toggles to LOW */
    hal_led_blinker_tick_ms(&blinker, 250);
    TEST_ASSERT(hal_led_blinker_get_output(&blinker) == false, "2Hz at 250ms toggles to LOW");

    /* Advance 250ms: Toggles back to HIGH */
    hal_led_blinker_tick_ms(&blinker, 250);
    TEST_ASSERT(hal_led_blinker_get_output(&blinker) == true, "2Hz at 500ms completes full cycle");

    /* 4. Non-uniform jitter / modulo zero-drift stress test (100 cycles of 100ms) */
    hal_led_blinker_set_mode(&blinker, HAL_LED_BLINK_1HZ);
    const uint32_t jitter_pattern[7] = {7U, 13U, 11U, 19U, 5U, 25U, 20U}; /* Sum = 100ms */
    for (int rep = 0; rep < 100; rep++) {
        for (int step = 0; step < 7; step++) {
            hal_led_blinker_tick_ms(&blinker, jitter_pattern[step]);
        }
    }
    /* Total: 10,000ms = exactly 20 half-periods of 500ms */
    TEST_ASSERT(hal_led_blinker_get_output(&blinker) == true, "1Hz after 10,000ms jitter remains in initial phase");
    TEST_ASSERT(blinker.phase_timer_ms == 0U, "Phase accumulator has ZERO cumulative drift under CPU jitter");

    TEST_PASS("HAL-03: LED Blinker Waveforms (Accurate 1.0 Hz and 2.0 Hz non-blocking timing & zero-drift)");
}

/* -------------------------------------------------------------------------- */
/* Test 4: Actuator Safety Interlock Guard                                    */
/* -------------------------------------------------------------------------- */
static void test_actuator_interlock_guard(void) {
    hal_actuator_guard_t guard;
    memset(&guard, 0, sizeof(guard));

    /* Case A: Motor OFF, door unlocked -> Safe */
    guard.motor = HAL_MOTOR_OFF;
    guard.door_lock = false;
    TEST_ASSERT(hal_actuator_is_safe(&guard), "Safe when all off");

    /* Case B: Motor AGITATE with door UNLOCKED -> DANGEROUS */
    guard.motor = HAL_MOTOR_AGITATE;
    guard.door_lock = false;
    TEST_ASSERT(!hal_actuator_is_safe(&guard), "Motor with unlocked door is UNSAFE");

    /* Case C: Motor AGITATE with door LOCKED -> Safe */
    guard.door_lock = true;
    TEST_ASSERT(hal_actuator_is_safe(&guard), "Motor agitate with locked door is safe");

    /* Case D: Motor SPIN with door LOCKED but Drain Pump OFF -> DANGEROUS */
    guard.motor = HAL_MOTOR_SPIN;
    guard.drain_pump = false;
    TEST_ASSERT(!hal_actuator_is_safe(&guard), "Spin dry without drain pump is UNSAFE");

    /* Case E: Motor SPIN with door LOCKED and Drain Pump ON -> Safe */
    guard.drain_pump = true;
    guard.water_valve = false;
    TEST_ASSERT(hal_actuator_is_safe(&guard), "Spin dry with door locked and pump on is safe");

    /* Case F: Water valve open during high-speed spin -> DANGEROUS */
    guard.water_valve = true;
    TEST_ASSERT(!hal_actuator_is_safe(&guard), "Water valve open during high-speed spin is UNSAFE");

    /* Case G: Water valve and Drain Pump concurrently active -> DANGEROUS */
    guard.motor = HAL_MOTOR_OFF;
    guard.door_lock = false;
    guard.water_valve = true;
    guard.drain_pump = true;
    TEST_ASSERT(!hal_actuator_is_safe(&guard), "Water valve and drain pump simultaneous active is UNSAFE");

    /* NULL check */
    TEST_ASSERT(!hal_actuator_is_safe(NULL), "NULL guard returns false");

    TEST_PASS("HAL-04: Actuator Interlock Guard (Strict hardware protection rules verified)");
}

/* -------------------------------------------------------------------------- */
/* Test 5: Coin Pulse FIFO Queue & Burst Insertion Resilience                 */
/* -------------------------------------------------------------------------- */
static void test_coin_pulse_fifo_burst_queue(void) {
    hal_coin_pulse_detector_t det;
    hal_coin_pulse_init(&det, true, 150);

    #define SEND_PULSE_FAST() do { \
        for (int ms = 0; ms < 30; ms++) { hal_coin_pulse_update(&det, false, 1); } \
        for (int ms = 0; ms < 30; ms++) { hal_coin_pulse_update(&det, true, 1); } \
    } while(0)

    #define ADVANCE_SILENCE_FAST(ms_count) do { \
        for (int ms = 0; ms < ms_count; ms++) { hal_coin_pulse_update(&det, true, 1); } \
    } while(0)

    TEST_ASSERT(hal_coin_pulse_available(&det) == 0, "Initial FIFO is empty");

    /* Rapid Burst Coin 1: 10¢ (1 pulse) */
    SEND_PULSE_FAST();
    ADVANCE_SILENCE_FAST(160);

    /* Rapid Burst Coin 2: 20¢ (2 pulses) */
    SEND_PULSE_FAST();
    ADVANCE_SILENCE_FAST(40);
    SEND_PULSE_FAST();
    ADVANCE_SILENCE_FAST(160);

    /* Rapid Burst Coin 3: 50¢ (5 pulses) */
    for (int p = 0; p < 5; p++) {
        SEND_PULSE_FAST();
        ADVANCE_SILENCE_FAST(30);
    }
    ADVANCE_SILENCE_FAST(160);

    /* Verify all 3 coins queued in FIFO without drops */
    TEST_ASSERT(hal_coin_pulse_available(&det) == 3, "3 coins queued in FIFO");

    /* Dequeue sequentially */
    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 10, "1st queued coin is 10¢");
    TEST_ASSERT(hal_coin_pulse_available(&det) == 2, "2 coins remain in FIFO");

    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 20, "2nd queued coin is 20¢");
    TEST_ASSERT(hal_coin_pulse_available(&det) == 1, "1 coin remains in FIFO");

    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 50, "3rd queued coin is 50¢");
    TEST_ASSERT(hal_coin_pulse_available(&det) == 0, "FIFO is now completely drained");

    TEST_ASSERT(hal_coin_pulse_get_coin(&det) == 0, "Empty FIFO returns 0");

    TEST_PASS("HAL-05: Coin Pulse FIFO Queue (Burst coin insertion without event drops)");
}

/* -------------------------------------------------------------------------- */
/* Test 6: Comprehensive NULL Pointer Resilience across all HAL APIs          */
/* -------------------------------------------------------------------------- */
static void test_hal_null_pointer_resilience(void) {
    /* Button engine NULL safety */
    hal_button_init(NULL, true);
    hal_button_update(NULL, true, 1);
    TEST_ASSERT(!hal_button_was_pressed(NULL), "NULL was_pressed returns false");
    TEST_ASSERT(!hal_button_was_released(NULL), "NULL was_released returns false");
    TEST_ASSERT(!hal_button_is_pressed(NULL), "NULL is_pressed returns false");

    /* Coin pulse detector NULL safety */
    hal_coin_pulse_init(NULL, true, 150);
    hal_coin_pulse_update(NULL, true, 1);
    TEST_ASSERT(hal_coin_pulse_get_coin(NULL) == 0, "NULL get_coin returns 0");
    TEST_ASSERT(hal_coin_pulse_available(NULL) == 0, "NULL available returns 0");

    /* LED blinker NULL safety */
    hal_led_blinker_init(NULL, true);
    hal_led_blinker_set_mode(NULL, HAL_LED_ON);
    hal_led_blinker_tick_ms(NULL, 10);
    TEST_ASSERT(!hal_led_blinker_get_output(NULL), "NULL get_output returns false");

    /* Actuator guard NULL safety */
    TEST_ASSERT(!hal_actuator_is_safe(NULL), "NULL actuator guard returns false");

    TEST_PASS("HAL-06: NULL Pointer Resilience (Zero crash across all HAL API functions)");
}

/* -------------------------------------------------------------------------- */
/* Runner                                                                     */
/* -------------------------------------------------------------------------- */
int main(void) {
    printf(ANSI_CYAN "============================================================\n" ANSI_RESET);
    printf(ANSI_CYAN " BTL 2: Hardware HAL Engines - Unit Verification\n" ANSI_RESET);
    printf(ANSI_CYAN " CO3053 Embedded Systems - HCMUT\n" ANSI_RESET);
    printf(ANSI_CYAN "============================================================\n\n" ANSI_RESET);

    test_button_debouncer_glitch_rejection();
    test_coin_pulse_detector();
    test_led_blinker_waveforms();
    test_actuator_interlock_guard();
    test_coin_pulse_fifo_burst_queue();
    test_hal_null_pointer_resilience();

    printf("\n" ANSI_CYAN "============================================================\n" ANSI_RESET);
    if (g_hal_failed == 0) {
        printf(ANSI_GREEN " ALL %d HARDWARE HAL ENGINE TESTS PASSED!\n" ANSI_RESET, g_hal_passed);
        printf(ANSI_CYAN "============================================================\n" ANSI_RESET);
        return 0;
    } else {
        printf(ANSI_RED " FAILED: %d Passed, %d Failed\n" ANSI_RESET, g_hal_passed, g_hal_failed);
        printf(ANSI_CYAN "============================================================\n" ANSI_RESET);
        return 1;
    }
}

