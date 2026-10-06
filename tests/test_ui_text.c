/**
 * @file test_ui_text.c
 * @brief Host unit test for the UART status line formatter
 */

#include <stdio.h>
#include <string.h>
#include "ui_text.h"

static int g_failed = 0;

#define CHECK(cond, name) do { \
    if (cond) { printf("  [PASS] %s\n", name); } \
    else { printf("  [FAIL] %s (line %d)\n", name, __LINE__); g_failed++; } \
} while (0)

int main(void) {
    char line[160];
    ui_view_t v;
    memset(&v, 0, sizeof(v));
    v.state = WM_STATE_RUNNING;
    v.phase = WM_PHASE_WASH_AGITATE;
    v.remaining_sec = 1785U;
    v.total_sec = 1800U;
    v.bled = true;
    v.motor = HAL_MOTOR_AGITATE;
    v.lock = true;

    size_t n = ui_status_line(line, sizeof(line), &v);
    CHECK(n == strlen(line), "returned length matches");
    CHECK(strcmp(line, "STATE=RUNNING PHASE=WASH TIME=29:45 COIN=000C RLED=0 BLED=1 MOTOR=AGIT PUMP=0 LOCK=1 FAULT=-") == 0,
          "RUNNING line");

    v.state = WM_STATE_STANDBY;
    v.phase = WM_PHASE_IDLE;
    v.balance_cents = 60U;
    v.motor = HAL_MOTOR_OFF;
    v.lock = false;
    v.fault_flags = WM_FAULT_DOOR_OPEN;
    (void)ui_status_line(line, sizeof(line), &v);
    CHECK(strstr(line, "STATE=STANDBY") && strstr(line, "TIME=30:00") && strstr(line, "COIN=060C") &&
          strstr(line, "FAULT=DOOR_LATCH_OPEN"), "STANDBY shows full cycle time, coin and fault");

    char tiny[12];
    n = ui_status_line(tiny, sizeof(tiny), &v);
    CHECK(n < sizeof(tiny) && tiny[n] == '\0', "truncation is safe and NUL-terminated");

    printf(g_failed == 0 ? "ALL UI TEXT TESTS PASSED\n" : "UI TEXT TESTS FAILED\n");
    return g_failed == 0 ? 0 : 1;
}
