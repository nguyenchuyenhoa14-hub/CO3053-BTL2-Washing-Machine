/**
 * @file hal_button_gesture.h
 * @brief Single-button gesture decoder (click / double-click / two-stage hold)
 * @details Boards with only one user key (e.g. WeAct STM32H750 K1 on PC13) need
 *          several logical commands from one input. Feed this decoder the already
 *          debounced pressed level once per millisecond.
 */

#ifndef HAL_BUTTON_GESTURE_H
#define HAL_BUTTON_GESTURE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_GESTURE_DOUBLE_GAP_MS  (300U)   /**< Max release->press gap for a double-click */
#define HAL_GESTURE_HOLD1_MS       (600U)   /**< First hold threshold */
#define HAL_GESTURE_HOLD2_MS       (1200U)  /**< Second hold threshold (< 600 + 1500 STOP window) */

typedef enum {
    HAL_GESTURE_NONE = 0,
    HAL_GESTURE_CLICK,      /**< Short press, no second press within the gap */
    HAL_GESTURE_DOUBLE,     /**< Two short presses */
    HAL_GESTURE_HOLD1,      /**< Held for HAL_GESTURE_HOLD1_MS (fires while still pressed) */
    HAL_GESTURE_HOLD2,      /**< Held for HAL_GESTURE_HOLD2_MS (fires while still pressed) */
    HAL_GESTURE_HOLD1_END   /**< Released after HOLD1 but before HOLD2 (a "long press and release") */
} hal_gesture_t;

typedef enum {
    HAL_GST_IDLE = 0,
    HAL_GST_DOWN1,
    HAL_GST_WAIT,
    HAL_GST_DOWN2,
    HAL_GST_HELD
} hal_gesture_phase_t;

typedef struct {
    hal_gesture_phase_t phase;
    uint32_t timer_ms;
    bool hold2_fired;
} hal_gesture_ctx_t;

void hal_gesture_init(hal_gesture_ctx_t *g);

/**
 * @brief Advance the decoder
 * @param g Decoder context
 * @param pressed Debounced key level (true = held down)
 * @param delta_ms Elapsed milliseconds since previous call (typically 1)
 * @return At most one gesture per call; HAL_GESTURE_NONE otherwise
 */
hal_gesture_t hal_gesture_update(hal_gesture_ctx_t *g, bool pressed, uint32_t delta_ms);

/**
 * @brief Milliseconds the key has currently been held (0 when not held), for UI progress bars
 */
uint32_t hal_gesture_hold_ms(const hal_gesture_ctx_t *g);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BUTTON_GESTURE_H */
