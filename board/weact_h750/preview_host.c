/**
 * @file preview_host.c
 * @brief Host tool: drives the real FSM + single-button mapping and dumps the dashboard as PPM frames
 * @details Usage: ./preview_wm OUT_PREFIX  -> writes OUT_PREFIX_<n>_<label>.ppm for each UI state
 */

#include <stdio.h>
#include <stdint.h>
#include "washing_machine_fsm.h"
#include "wm_single_button.h"
#include "gfx.h"
#include "ui_render.h"

static wm_context_t g_ctx;
static ui_view_t g_view;

static void snapshot(void) {
    g_view.state = g_ctx.state;
    g_view.phase = wm_fsm_get_cycle_phase(&g_ctx);
    g_view.balance_cents = g_ctx.coin_balance_cents;
    g_view.remaining_sec = g_ctx.remaining_cycle_sec;
    g_view.total_sec = g_ctx.cycle_duration_setting;
    g_view.fault_flags = g_ctx.active_error_flags;
}

static void cb_rled(hal_led_state_t s) { g_view.rled = (s != HAL_LED_OFF); }
static void cb_bled(hal_led_state_t s) { g_view.bled = (s != HAL_LED_OFF); }
static void cb_motor(hal_motor_state_t m) { g_view.motor = m; }
static void cb_valve(bool o) { (void)o; }
static void cb_pump(bool o) { g_view.pump = o; }
static void cb_lock(bool o) { g_view.lock = o; }
static void cb_done(void) { }

static void dump(const char *prefix, int n, const char *label) {
    char path[256];
    snprintf(path, sizeof(path), "%s_%d_%s.ppm", prefix, n, label);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", GFX_WIDTH, GFX_HEIGHT);
    for (int i = 0; i < (GFX_WIDTH * GFX_HEIGHT); i++) {
        uint16_t c = g_gfx_fb[i];
        uint8_t px[3] = {(uint8_t)(((c >> 11) & 0x1FU) << 3), (uint8_t)(((c >> 5) & 0x3FU) << 2),
                         (uint8_t)((c & 0x1FU) << 3)};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static void shot(const char *prefix, int *n, const char *label) {
    snapshot();
    ui_render(&g_view);
    dump(prefix, (*n)++, label);
}

int main(int argc, char **argv) {
    const char *prefix = (argc > 1) ? argv[1] : "frame";
    int n = 0;
    hal_output_callbacks_t cbs = {cb_rled, cb_bled, cb_motor, cb_valve, cb_pump, cb_lock, cb_done, NULL};

    wm_fsm_init(&g_ctx, &cbs);
    shot(prefix, &n, "standby");
    snapshot();
    g_view.balance_cents = 60U;
    g_view.notice = 2U;
    g_view.notice_value = 30U;
    ui_render(&g_view);
    dump(prefix, n++, "notenough");
    g_view.notice = 1U;
    g_view.notice_value = 50U;
    g_view.balance_cents = 0U;
    ui_render(&g_view);
    dump(prefix, n++, "readychange");
    g_view.notice = 0U;
    (void)wm_single_button_apply(&g_ctx, HAL_GESTURE_DOUBLE);
    shot(prefix, &n, "standby10");
    (void)wm_single_button_apply(&g_ctx, HAL_GESTURE_CLICK);
    shot(prefix, &n, "ready");
    (void)wm_single_button_apply(&g_ctx, HAL_GESTURE_CLICK);
    for (int i = 0; i < 400; i++) { wm_fsm_tick_1s(&g_ctx); }
    shot(prefix, &n, "running_wash");
    for (int i = 0; i < 1100; i++) { wm_fsm_tick_1s(&g_ctx); }
    g_view.hold_ms = 800U;
    shot(prefix, &n, "running_spin_hold");
    g_view.hold_ms = 0U;
    (void)wm_single_button_apply(&g_ctx, HAL_GESTURE_CLICK);
    shot(prefix, &n, "paused");
    (void)wm_single_button_apply(&g_ctx, HAL_GESTURE_DOUBLE);
    shot(prefix, &n, "error");
    return 0;
}
