/**
 * @file hal_led_blinker.h
 * @brief Non-blocking hardware LED blinker engine and actuator interlock guard
 * @details Conforms to CO3053 real-time requirements.
 *          Generates exact 1.0 Hz (500ms ON / 500ms OFF) and 2.0 Hz (250ms ON / 250ms OFF)
 *          without blocking delays.
 */

#ifndef HAL_LED_BLINKER_H
#define HAL_LED_BLINKER_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_interfaces.h"
#include "washing_machine_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Independent LED blinker state controller
 */
typedef struct {
    hal_led_state_t mode;       /**< Commanded mode (OFF, ON, BLINK_1HZ, BLINK_2HZ) */
    bool active_high;           /**< true if HIGH illuminates LED */
    bool output_level;          /**< Current physical output electrical state */
    uint32_t phase_timer_ms;    /**< Millisecond accumulator for toggle phase */
    uint32_t half_period_ms;    /**< Half-period (500ms for 1Hz, 250ms for 2Hz) */
} hal_led_blinker_t;

/**
 * @brief Physical actuator state snapshot for safety interlocking
 */
typedef struct {
    hal_motor_state_t motor;
    bool drain_pump;
    bool door_lock;
    bool water_valve;
} hal_actuator_guard_t;

/**
 * @brief Initialize an LED blinker instance
 * @param blinker Pointer to blinker structure
 * @param active_high true if logic HIGH turns LED ON
 */
void hal_led_blinker_init(hal_led_blinker_t *blinker, bool active_high);

/**
 * @brief Set operational mode of the blinker
 * @param blinker Pointer to blinker structure
 * @param mode Target mode (HAL_LED_OFF, HAL_LED_ON, HAL_LED_BLINK_1HZ, HAL_LED_BLINK_2HZ)
 */
void hal_led_blinker_set_mode(hal_led_blinker_t *blinker, hal_led_state_t mode);

/**
 * @brief Advance blinker internal clock by delta milliseconds
 * @param blinker Pointer to blinker structure
 * @param delta_ms Elapsed milliseconds (typically 1ms)
 */
void hal_led_blinker_tick_ms(hal_led_blinker_t *blinker, uint32_t delta_ms);

/**
 * @brief Query current physical electrical output level
 * @param blinker Pointer to blinker structure
 * @return true if output pin should be driven HIGH; false for LOW
 */
bool hal_led_blinker_get_output(const hal_led_blinker_t *blinker);

/**
 * @brief Actuator interlock safety verification
 * @details Ensures motor agitation and high-speed spin cannot conflict,
 *          and door is locked during high-speed spinning.
 * @param guard Pointer to actuator state
 * @return true if state is 100% physically safe; false if safety violation detected
 */
bool hal_actuator_is_safe(const hal_actuator_guard_t *guard);

#ifdef __cplusplus
}
#endif

#endif /* HAL_LED_BLINKER_H */
