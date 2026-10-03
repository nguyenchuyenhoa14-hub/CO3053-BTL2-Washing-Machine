/**
 * @file hal_stm32_callbacks.c
 * @brief Implementation of STM32 output callbacks driving physical relays and LEDs
 */

#include "hal_stm32_callbacks.h"
#include "hal_stm32_gpio.h"

static hal_led_blinker_t g_stm32_rled;
static hal_led_blinker_t g_stm32_bled;

static void cb_set_rled(hal_led_state_t state) {
    hal_led_blinker_set_mode(&g_stm32_rled, state);
    stm32_gpio_write(GPIOB, STM32_PIN_RLED, hal_led_blinker_get_output(&g_stm32_rled));
}

static void cb_set_bled(hal_led_state_t state) {
    hal_led_blinker_set_mode(&g_stm32_bled, state);
    stm32_gpio_write(GPIOB, STM32_PIN_BLED, hal_led_blinker_get_output(&g_stm32_bled));
}

static void cb_set_motor(hal_motor_state_t state) {
    switch (state) {
        case HAL_MOTOR_OFF:
            stm32_gpio_write(GPIOB, STM32_PIN_MTR_AGITATE, false);
            stm32_gpio_write(GPIOB, STM32_PIN_MTR_SPIN, false);
            break;

        case HAL_MOTOR_AGITATE:
            stm32_gpio_write(GPIOB, STM32_PIN_MTR_SPIN, false); /* Interlock guard */
            stm32_gpio_write(GPIOB, STM32_PIN_MTR_AGITATE, true);
            break;

        case HAL_MOTOR_SPIN:
            stm32_gpio_write(GPIOB, STM32_PIN_MTR_AGITATE, false); /* Interlock guard */
            stm32_gpio_write(GPIOB, STM32_PIN_MTR_SPIN, true);
            break;

        default:
            /* Defensive fallback: de-energize all motor windings */
            stm32_gpio_write(GPIOB, STM32_PIN_MTR_AGITATE, false);
            stm32_gpio_write(GPIOB, STM32_PIN_MTR_SPIN, false);
            break;
    }
}

static void cb_set_water_valve(bool open) {
    (void)open; /* Optional auxiliary valve channel */
}

static void cb_set_drain_pump(bool on) {
    stm32_gpio_write(GPIOB, STM32_PIN_DRAIN_PUMP, on);
}

static void cb_set_door_lock(bool locked) {
    stm32_gpio_write(GPIOB, STM32_PIN_DOOR_LOCK, locked);
}

static void cb_on_cycle_complete(void) {
    cb_set_motor(HAL_MOTOR_OFF);
    cb_set_drain_pump(false);
    cb_set_door_lock(false);
}

void stm32_hal_init(void) {
    stm32_gpio_init();
    hal_led_blinker_init(&g_stm32_rled, true);
    hal_led_blinker_init(&g_stm32_bled, true);
}

void stm32_hal_tick_1ms(uint32_t delta_ms) {
    hal_led_blinker_tick_ms(&g_stm32_rled, delta_ms);
    hal_led_blinker_tick_ms(&g_stm32_bled, delta_ms);

    /* Write physical pins */
    stm32_gpio_write(GPIOB, STM32_PIN_RLED, hal_led_blinker_get_output(&g_stm32_rled));
    stm32_gpio_write(GPIOB, STM32_PIN_BLED, hal_led_blinker_get_output(&g_stm32_bled));
}

hal_output_callbacks_t stm32_hal_get_callbacks(void) {
    hal_output_callbacks_t cbs = {
        .set_rled = cb_set_rled,
        .set_bled = cb_set_bled,
        .set_motor = cb_set_motor,
        .set_water_valve = cb_set_water_valve,
        .set_drain_pump = cb_set_drain_pump,
        .set_door_lock = cb_set_door_lock,
        .on_cycle_complete = cb_on_cycle_complete
    };
    return cbs;
}
