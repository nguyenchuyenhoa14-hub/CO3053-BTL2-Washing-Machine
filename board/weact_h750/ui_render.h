/**
 * @file ui_render.h
 * @brief Washing machine dashboard drawn into the 160x80 framebuffer
 */

#ifndef UI_RENDER_H
#define UI_RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include "washing_machine_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Everything the dashboard shows; compare with ui_view_equal() to skip redundant redraws */
typedef struct {
    wm_state_t state;
    wm_cycle_phase_t phase;
    uint32_t balance_cents;
    uint32_t remaining_sec;
    uint32_t total_sec;
    uint32_t fault_flags;
    bool rled;
    bool bled;
    hal_motor_state_t motor;
    bool pump;
    bool lock;
    uint32_t bl_mode;       /**< Backlight experiment mode (0 = hidden) */
    const char *reset_cause; /**< Last reset cause string (diagnostic) */
    uint32_t notice;        /**< 0 none, 1 READY (change returned), 2 NOT ENOUGH (refund), 3 CANCELLED (deposit returned) */
    uint32_t notice_value;
    uint32_t hold_ms;       /**< Current button hold time (0 when released) */
} ui_view_t;

bool ui_view_equal(const ui_view_t *a, const ui_view_t *b);

/** @brief Render the full dashboard into g_gfx_fb */
void ui_render(const ui_view_t *v);

#ifdef __cplusplus
}
#endif

#endif /* UI_RENDER_H */
