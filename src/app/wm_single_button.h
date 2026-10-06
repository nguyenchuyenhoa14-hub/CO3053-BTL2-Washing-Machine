/**
 * @file wm_single_button.h
 * @brief Maps single-button gestures onto Washing Machine FSM events
 * @details Gesture map (state-aware):
 *            CLICK   STANDBY +50c | READY RUN | RUNNING PAUSE | PAUSED RESUME | ERROR clear
 *            DOUBLE  STANDBY +10c | READY/RUNNING/PAUSED inject door-open fault | ERROR clear
 *            HOLD1   STOP (1st press, arms the 1.5 s window)
 *            HOLD2   STOP (2nd press, force stop -> STANDBY)
 */

#ifndef WM_SINGLE_BUTTON_H
#define WM_SINGLE_BUTTON_H

#include "washing_machine_fsm.h"
#include "hal_button_gesture.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply a decoded gesture to the FSM
 * @param ctx FSM context
 * @param gesture Gesture produced by hal_gesture_update()
 * @return true if the FSM was affected
 */
bool wm_single_button_apply(wm_context_t *ctx, hal_gesture_t gesture);

/**
 * @brief External RUN/PAUSE push-button: READY/PAUSED -> RUN, RUNNING -> PAUSE, ERROR -> clear; ignored in STANDBY
 */
bool wm_ext_run_pause_apply(wm_context_t *ctx);

/**
 * @brief External STOP push-button: normal STOP event (double press within 1.5 s = force stop); clears ERROR
 */
bool wm_ext_stop_apply(wm_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WM_SINGLE_BUTTON_H */
