/**
 * @file stm32_compat.h
 * @brief CMSIS & STM32 HAL register definitions for portable cross-compilation
 * @details Allows compiling STM32 bare-metal drivers on both desktop GCC (for static analysis/verification)
 *          and ARM GCC / STM32CubeIDE / Keil MDK for actual hardware flashing.
 */

#ifndef STM32_COMPAT_H
#define STM32_COMPAT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __arm__
/* Target ARM toolchain: include vendor CMSIS headers */
#include "stm32f1xx.h"
#else

/* Desktop host simulation: define standard STM32 memory-mapped registers */
typedef struct {
    volatile uint32_t CRL;
    volatile uint32_t CRH;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t BRR;
    volatile uint32_t LCKR;
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
} RCC_TypeDef;

/* Simulated hardware peripheral instances */
extern GPIO_TypeDef g_sim_GPIOA;
extern GPIO_TypeDef g_sim_GPIOB;
extern RCC_TypeDef  g_sim_RCC;

#define GPIOA ((GPIO_TypeDef *)&g_sim_GPIOA)
#define GPIOB ((GPIO_TypeDef *)&g_sim_GPIOB)
#define RCC   ((RCC_TypeDef  *)&g_sim_RCC)

#define RCC_APB2ENR_IOPAEN (1U << 2)
#define RCC_APB2ENR_IOPBEN (1U << 3)

#endif /* __arm__ */

/* STM32 Pin definitions for BTL 2 */
#define STM32_PIN_RUN            (0U)   /* PA0 */
#define STM32_PIN_PAUSE          (1U)   /* PA1 */
#define STM32_PIN_STOP           (2U)   /* PA2 */
#define STM32_PIN_COIN_10        (3U)   /* PA3 */
#define STM32_PIN_COIN_20        (4U)   /* PA4 */
#define STM32_PIN_COIN_50        (5U)   /* PA5 */
#define STM32_PIN_FAULT_DOOR     (6U)   /* PA6 */
#define STM32_PIN_FAULT_WATER    (7U)   /* PA7 */

#define STM32_PIN_RLED           (0U)   /* PB0 */
#define STM32_PIN_BLED           (1U)   /* PB1 */
#define STM32_PIN_MTR_AGITATE    (12U)  /* PB12 */
#define STM32_PIN_MTR_SPIN       (13U)  /* PB13 */
#define STM32_PIN_DRAIN_PUMP     (14U)  /* PB14 */
#define STM32_PIN_DOOR_LOCK      (15U)  /* PB15 */

#endif /* STM32_COMPAT_H */
