/**
 * @file main_stm32.c
 * @brief Canonical STM32 Bare-Metal Super-Loop for Washing Machine Control Unit
 * @details Conforms to HCMUT CO3053 standards.
 *          Target: STM32F103C8T6 (ARM Cortex-M3) / STM32F401 (ARM Cortex-M4)
 */

#include <stdio.h>
#include <stdbool.h>
#include "washing_machine_fsm.h"
#include "hal_stm32_gpio.h"
#include "hal_stm32_callbacks.h"
#include "hal_button_engine.h"

static wm_context_t g_fsm_ctx;

static hal_button_t btn_run;
static hal_button_t btn_pause;
static hal_button_t btn_stop;
static hal_button_t btn_c10;
static hal_button_t btn_c20;
static hal_button_t btn_c50;
static hal_button_t sw_door_fault;

static volatile uint32_t g_ms_ticks = 0;
static volatile uint32_t g_sec_accumulator = 0;

/**
 * @brief Canonical SysTick ISR (Runs at 1 kHz / 1 ms period)
 */
void SysTick_Handler(void) {
    g_ms_ticks++;

    /* 1. Advance FSM internal timers */
    wm_fsm_tick_1ms(&g_fsm_ctx);

    /* 2. Advance physical LED blinker engines */
    stm32_hal_tick_1ms(1);

    /* 3. Sample digital inputs with 30ms software debouncers */
    hal_button_update(&btn_run,        stm32_gpio_read(GPIOA, STM32_PIN_RUN), 1);
    hal_button_update(&btn_pause,      stm32_gpio_read(GPIOA, STM32_PIN_PAUSE), 1);
    hal_button_update(&btn_stop,       stm32_gpio_read(GPIOA, STM32_PIN_STOP), 1);
    hal_button_update(&btn_c10,        stm32_gpio_read(GPIOA, STM32_PIN_COIN_10), 1);
    hal_button_update(&btn_c20,        stm32_gpio_read(GPIOA, STM32_PIN_COIN_20), 1);
    hal_button_update(&btn_c50,        stm32_gpio_read(GPIOA, STM32_PIN_COIN_50), 1);
    hal_button_update(&sw_door_fault,  stm32_gpio_read(GPIOA, STM32_PIN_FAULT_DOOR), 1);

    /* 4. 1-second system clock tick */
    g_sec_accumulator++;
    if (g_sec_accumulator >= 1000U) {
        g_sec_accumulator = 0;
        wm_fsm_tick_1s(&g_fsm_ctx);
    }
}

int main(void) {
    /* 1. Hardware initialization */
    stm32_hal_init();

    /* 2. Debounce engines initialization (Active-Low buttons with internal pull-up) */
    hal_button_init(&btn_run, true);
    hal_button_init(&btn_pause, true);
    hal_button_init(&btn_stop, true);
    hal_button_init(&btn_c10, true);
    hal_button_init(&btn_c20, true);
    hal_button_init(&btn_c50, true);
    hal_button_init(&sw_door_fault, true);

    /* 3. Core FSM initialization */
    hal_output_callbacks_t cbs = stm32_hal_get_callbacks();
    wm_fsm_init(&g_fsm_ctx, &cbs);

    printf("STM32 Bare-Metal Washing Machine Controller Initialized.\n");

    /* 4. Super-Loop Execution (Non-blocking event dispatcher) */
    while (1) {
        /* Check debounced button events */
        if (hal_button_was_pressed(&btn_run)) {
            wm_fsm_dispatch_event(&g_fsm_ctx, WM_EVT_BTN_RUN);
        }
        if (hal_button_was_pressed(&btn_pause)) {
            wm_fsm_dispatch_event(&g_fsm_ctx, WM_EVT_BTN_PAUSE);
        }
        if (hal_button_was_pressed(&btn_stop)) {
            wm_fsm_dispatch_event(&g_fsm_ctx, WM_EVT_BTN_STOP);
        }
        if (hal_button_was_pressed(&btn_c10)) {
            wm_fsm_dispatch_event(&g_fsm_ctx, WM_EVT_COIN_10);
        }
        if (hal_button_was_pressed(&btn_c20)) {
            wm_fsm_dispatch_event(&g_fsm_ctx, WM_EVT_COIN_20);
        }
        if (hal_button_was_pressed(&btn_c50)) {
            wm_fsm_dispatch_event(&g_fsm_ctx, WM_EVT_COIN_50);
        }

        /* Check door interlock safety sensor */
        if (hal_button_is_pressed(&sw_door_fault)) {
            wm_fsm_trigger_fault(&g_fsm_ctx, WM_FAULT_DOOR_OPEN);
        } else if (g_fsm_ctx.state == WM_STATE_ERROR) {
            wm_fsm_clear_fault(&g_fsm_ctx);
        }

#ifndef __arm__
        /* Desktop verification: single pass demonstration */
        break;
#endif
    }

    return 0;
}
