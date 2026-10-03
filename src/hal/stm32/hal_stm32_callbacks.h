/**
 * @file hal_stm32_callbacks.h
 * @brief STM32 HAL callback binding for the Washing Machine FSM
 */

#ifndef HAL_STM32_CALLBACKS_H
#define HAL_STM32_CALLBACKS_H

#include "hal_interfaces.h"
#include "hal_led_blinker.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize STM32 HAL output subsystem and blinker engines
 */
void stm32_hal_init(void);

/**
 * @brief Periodic 1ms update for physical LED outputs on STM32
 * @param delta_ms Elapsed milliseconds
 */
void stm32_hal_tick_1ms(uint32_t delta_ms);

/**
 * @brief Get the FSM-compliant callback table for STM32 hardware
 */
hal_output_callbacks_t stm32_hal_get_callbacks(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_STM32_CALLBACKS_H */
