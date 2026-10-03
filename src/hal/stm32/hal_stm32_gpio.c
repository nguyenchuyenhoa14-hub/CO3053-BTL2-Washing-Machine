/**
 * @file hal_stm32_gpio.c
 * @brief Implementation of STM32 bare-metal GPIO register operations
 */

#include "hal_stm32_gpio.h"

#ifndef __arm__
/* Host simulation memory buffers */
GPIO_TypeDef g_sim_GPIOA;
GPIO_TypeDef g_sim_GPIOB;
RCC_TypeDef  g_sim_RCC;
#endif

void stm32_gpio_init(void) {
    /* 1. Enable clock for GPIOA (inputs) and GPIOB (outputs) */
    RCC->APB2ENR |= (RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN);

    /* 2. Configure GPIOA pins 0..7 as Input with Pull-up (Mode: 0x8 for CNF=10, MODE=00) */
    /* CRL covers pins 0 to 7 */
    GPIOA->CRL = 0x88888888U;
    /* Set ODR bits to activate internal pull-ups (active-low buttons) */
    GPIOA->ODR |= 0x00FFU;

    /* 3. Configure GPIOB pins:
     *    - PB0, PB1 (LEDs): General purpose output push-pull, 10MHz (0x1)
     *    - PB12..PB15 (Relays): General purpose output push-pull, 10MHz (0x1)
     */
    /* PB0, PB1 in CRL (lower nibbles) */
    GPIOB->CRL &= ~0x000000FFU;
    GPIOB->CRL |=  0x00000011U; /* 10MHz Push-Pull */

    /* PB12..PB15 in CRH (upper nibbles) */
    GPIOB->CRH &= ~0xFFFF0000U;
    GPIOB->CRH |=  0x11110000U; /* 10MHz Push-Pull */

    /* Initial state: All outputs OFF (LOW) */
    GPIOB->BRR = (1U << STM32_PIN_RLED) | (1U << STM32_PIN_BLED) |
                 (1U << STM32_PIN_MTR_AGITATE) | (1U << STM32_PIN_MTR_SPIN) |
                 (1U << STM32_PIN_DRAIN_PUMP) | (1U << STM32_PIN_DOOR_LOCK);
#ifndef __arm__
    GPIOA->IDR = 0xFFFFU; /* Initial state with pull-ups reads HIGH */
    GPIOB->ODR = 0x0000U; /* Initial outputs LOW */
#endif
}

bool stm32_gpio_read(GPIO_TypeDef *port, uint32_t pin) {
    if (!port || pin > 15U) {
        return false;
    }
    return (port->IDR & (1U << pin)) != 0U;
}

void stm32_gpio_write(GPIO_TypeDef *port, uint32_t pin, bool level) {
    if (!port || pin > 15U) {
        return;
    }
    if (level) {
        port->BSRR = (1U << pin);
#ifndef __arm__
        port->ODR |= (1U << pin);
#endif
    } else {
        port->BRR = (1U << pin);
#ifndef __arm__
        port->ODR &= ~(1U << pin);
#endif
    }
}
