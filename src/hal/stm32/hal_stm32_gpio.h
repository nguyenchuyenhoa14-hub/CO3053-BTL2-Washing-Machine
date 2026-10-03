/**
 * @file hal_stm32_gpio.h
 * @brief STM32 GPIO driver interface for Washing Machine Control Unit
 */

#ifndef HAL_STM32_GPIO_H
#define HAL_STM32_GPIO_H

#include "stm32_compat.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all GPIO pins for buttons, sensors, LEDs, and relays
 */
void stm32_gpio_init(void);

/**
 * @brief Read digital electrical level from an STM32 GPIO pin
 * @param port GPIO port pointer (GPIOA, GPIOB)
 * @param pin Pin number (0..15)
 * @return true if pin is HIGH, false if pin is LOW
 */
bool stm32_gpio_read(GPIO_TypeDef *port, uint32_t pin);

/**
 * @brief Write digital electrical level to an STM32 GPIO pin
 * @param port GPIO port pointer (GPIOA, GPIOB)
 * @param pin Pin number (0..15)
 * @param level true for HIGH (3.3V), false for LOW (GND)
 */
void stm32_gpio_write(GPIO_TypeDef *port, uint32_t pin, bool level);

#ifdef __cplusplus
}
#endif

#endif /* HAL_STM32_GPIO_H */
