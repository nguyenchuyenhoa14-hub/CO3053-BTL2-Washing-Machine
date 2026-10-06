/**
 * @file wm_two_button.h
 * @brief Front panel with coin staging: K1 does everything, optional external K0 as a coin shortcut
 * @details In STANDBY, coins are only *staged* (pending) and are not given to the FSM until the
 *          user confirms with a K1 hold. The FSM and its spec are unchanged.
 *
 *            K1 (onboard, PC13)   STANDBY: click +10c, double-click +20c,
 *                                   hold 0.6 s then release +50c, hold to 1.2 s = confirm
 *                                   pending >= 50c -> READY, excess is "returned" (notice only)
 *                                   pending <  50c -> "not enough money", pending is refunded, stay STANDBY
 *                                 other states: same as wm_single_button_apply()
 *            K0 (optional external, PA0)   STANDBY: click +10c, double-click / hold +20c
 *                                 other states: click = RUN/PAUSE
 */

#ifndef WM_TWO_BUTTON_H
#define WM_TWO_BUTTON_H

#include <stdint.h>
#include <stdbool.h>
#include "washing_machine_fsm.h"
#include "hal_button_gesture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WM_STAGING_MAX_CENTS  (990U)

typedef struct {
    uint32_t pending_cents;     /**< Coins inserted but not yet confirmed */
} wm_coin_staging_t;

typedef enum {
    WM_NOTICE_NONE = 0,
    WM_NOTICE_READY,            /**< Confirmed; value = change returned (0 if exact) */
    WM_NOTICE_NOT_ENOUGH,       /**< Not enough money; value = amount refunded */
    WM_NOTICE_REFUND            /**< Deposit cancelled by STOP x2; value = amount returned (reported by the FSM callback) */
} wm_notice_t;

void wm_staging_init(wm_coin_staging_t *s);

/** @brief Drop staged coins whenever the machine is not in STANDBY (e.g. a fault) */
void wm_staging_sync(wm_coin_staging_t *s, const wm_context_t *ctx);

/**
 * @brief Apply a K1 gesture
 * @param notice Set to a notice type when the gesture produced one (never cleared otherwise)
 * @param value  Amount associated with the notice
 * @return true if the machine or staged coins changed
 */
bool wm_k1_apply(wm_context_t *ctx, wm_coin_staging_t *s, hal_gesture_t gesture,
                 wm_notice_t *notice, uint32_t *value);

/** @brief Apply a K0 gesture (see header comment) */
bool wm_k0_apply(wm_context_t *ctx, wm_coin_staging_t *s, hal_gesture_t gesture);

#ifdef __cplusplus
}
#endif

#endif /* WM_TWO_BUTTON_H */
