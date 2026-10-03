/**
 * @file hal_button_engine.h
 * @brief Non-blocking software debounce filter and coin pulse detector
 * @details Conforms to MISRA-C and embedded systems real-time constraints.
 *          Provides glitch rejection, active-low edge detection, and multi-pulse coin validation.
 */

#ifndef HAL_BUTTON_ENGINE_H
#define HAL_BUTTON_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "washing_machine_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_DEBOUNCE_DEFAULT_MS  (WM_DEBOUNCE_TIME_MS)

/**
 * @brief Software debounce state machine for a single digital input
 */
typedef struct {
    bool active_low;            /**< true if pressed = electrical LOW (pull-up) */
    bool debounced_state;       /**< Filtered logic state (true = pressed, false = released) */
    bool last_raw_state;        /**< Previous raw reading */
    uint32_t debounce_counter;  /**< Counter in milliseconds */
    bool pressed_event;         /**< Latch for edge transition (cleared on read) */
    bool released_event;        /**< Latch for release transition */
} hal_button_t;

/**
 * @brief Pulse-train coin validator decoder
 * @details Decodes multi-pulse signals from commercial coin acceptors:
 *          1 pulse = 10¢, 2 pulses = 20¢, 5 pulses = 50¢.
 */
typedef struct {
    hal_button_t pulse_input;   /**< Debounced pulse pin */
    uint32_t pulse_count;       /**< Accumulated pulses in current train */
    uint32_t silence_timer_ms;  /**< Milliseconds since last pulse */
    uint32_t inter_pulse_timeout_ms; /**< Timeout window to finalize coin (e.g. 150ms) */
    uint32_t decoded_cents;     /**< Latch of decoded coin amount in cents (10, 20, 50) */
} hal_coin_pulse_detector_t;

/**
 * @brief Initialize a digital button debouncer
 * @param btn Pointer to button structure
 * @param active_low Set true for active-low buttons (internal pull-up)
 */
void hal_button_init(hal_button_t *btn, bool active_low);

/**
 * @brief Update button state by feeding the raw digital electrical level
 * @param btn Pointer to button structure
 * @param raw_pin_high Current electrical reading (true = VCC, false = GND)
 * @param delta_ms Elapsed time since last update (typically 1ms)
 */
void hal_button_update(hal_button_t *btn, bool raw_pin_high, uint32_t delta_ms);

/**
 * @brief Check if a press transition (falling edge for active-low) occurred
 * @param btn Pointer to button structure
 * @return true if pressed since last call; clears event latch
 */
bool hal_button_was_pressed(hal_button_t *btn);

/**
 * @brief Check current steady debounced state
 * @param btn Pointer to button structure
 * @return true if currently held down
 */
bool hal_button_is_pressed(const hal_button_t *btn);

/**
 * @brief Initialize a coin pulse train decoder
 * @param det Pointer to detector structure
 * @param active_low true if coin pulse is active-low
 * @param timeout_ms Silence timeout in milliseconds (e.g. 150 ms)
 */
void hal_coin_pulse_init(hal_coin_pulse_detector_t *det, bool active_low, uint32_t timeout_ms);

/**
 * @brief Update coin pulse decoder with raw pin sample
 * @param det Pointer to detector structure
 * @param raw_pin_high Current electrical reading
 * @param delta_ms Elapsed time since last update
 */
void hal_coin_pulse_update(hal_coin_pulse_detector_t *det, bool raw_pin_high, uint32_t delta_ms);

/**
 * @brief Read and clear any newly decoded coin denomination
 * @param det Pointer to detector structure
 * @return 10, 20, 50 if coin completed; 0 if none
 */
uint32_t hal_coin_pulse_get_coin(hal_coin_pulse_detector_t *det);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BUTTON_ENGINE_H */
