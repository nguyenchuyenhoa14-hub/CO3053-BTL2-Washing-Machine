/**
 * @file washing_machine_fsm.c
 * @brief Implementation of the Washing Machine Finite State Machine (FSM)
 *        CO3053 - Embedded Systems - Assignment 2 (BTL 2)
 */

#include "washing_machine_fsm.h"
#include <stddef.h>
#include <limits.h>

/* Helper macro for safe callback execution */
#define CALL_HAL(cb, func, ...) do { if ((cb)->func) { (cb)->func(__VA_ARGS__); } } while(0)
#define CALL_HAL_VOID(cb, func)  do { if ((cb)->func) { (cb)->func(); } } while(0)

static void enter_standby(wm_context_t *ctx) {
    ctx->state = WM_STATE_STANDBY;
    ctx->coin_balance_cents = 0;
    ctx->remaining_cycle_sec = 0;
    ctx->stop_press_count = 0;
    ctx->stop_window_timer_ms = 0;

    CALL_HAL(&ctx->callbacks, set_motor, HAL_MOTOR_OFF);
    CALL_HAL(&ctx->callbacks, set_water_valve, false);
    CALL_HAL(&ctx->callbacks, set_drain_pump, false);
    CALL_HAL(&ctx->callbacks, set_door_lock, false);
    CALL_HAL(&ctx->callbacks, set_bled, HAL_LED_OFF);
    CALL_HAL(&ctx->callbacks, set_rled, HAL_LED_ON);
}

static void enter_ready(wm_context_t *ctx) {
    ctx->state = WM_STATE_READY;
    CALL_HAL(&ctx->callbacks, set_rled, HAL_LED_OFF);
    CALL_HAL(&ctx->callbacks, set_bled, HAL_LED_ON);
}

static void update_running_actuators(wm_context_t *ctx) {
    uint32_t total = (ctx->cycle_duration_setting > 0) ?
                      ctx->cycle_duration_setting : WM_CYCLE_DURATION_SEC;
    uint32_t spin_threshold = total / 6; /* Final 1/6th of wash cycle is spin dry */

    CALL_HAL(&ctx->callbacks, set_door_lock, true);
    if (ctx->remaining_cycle_sec <= spin_threshold) {
        /* Final drain & high-speed spin dry */
        CALL_HAL(&ctx->callbacks, set_drain_pump, true);
        CALL_HAL(&ctx->callbacks, set_water_valve, false);
        CALL_HAL(&ctx->callbacks, set_motor, HAL_MOTOR_SPIN);
    } else {
        /* Main wash agitation */
        CALL_HAL(&ctx->callbacks, set_drain_pump, false);
        CALL_HAL(&ctx->callbacks, set_water_valve, false);
        CALL_HAL(&ctx->callbacks, set_motor, HAL_MOTOR_AGITATE);
    }
}

static void enter_running(wm_context_t *ctx, bool is_resuming) {
    ctx->state = WM_STATE_RUNNING;
    ctx->stop_press_count = 0;
    ctx->stop_window_timer_ms = 0;

    if (!is_resuming) {
        /* Unconditional clearance of money without returning redundancies */
        ctx->coin_balance_cents = 0;
        /* Activate 30-minute countdown timer */
        ctx->remaining_cycle_sec = (ctx->cycle_duration_setting > 0) ?
                                    ctx->cycle_duration_setting : WM_CYCLE_DURATION_SEC;
    }

    CALL_HAL(&ctx->callbacks, set_rled, HAL_LED_OFF);
    CALL_HAL(&ctx->callbacks, set_bled, HAL_LED_BLINK_1HZ);
    update_running_actuators(ctx);
}

static void enter_paused(wm_context_t *ctx) {
    ctx->state = WM_STATE_PAUSED;
    ctx->stop_press_count = 0;
    ctx->stop_window_timer_ms = 0;

    /* Actuators suspended, but remaining_cycle_sec continues counting down! */
    CALL_HAL(&ctx->callbacks, set_motor, HAL_MOTOR_OFF);
    CALL_HAL(&ctx->callbacks, set_water_valve, false);
    CALL_HAL(&ctx->callbacks, set_drain_pump, false);
    CALL_HAL(&ctx->callbacks, set_bled, HAL_LED_ON);
}

static void enter_error(wm_context_t *ctx) {
    ctx->state = WM_STATE_ERROR;
    ctx->stop_press_count = 0;
    ctx->stop_window_timer_ms = 0;

    CALL_HAL(&ctx->callbacks, set_motor, HAL_MOTOR_OFF);
    CALL_HAL(&ctx->callbacks, set_water_valve, false);
    CALL_HAL(&ctx->callbacks, set_drain_pump, false);
    CALL_HAL(&ctx->callbacks, set_door_lock, false);
    CALL_HAL(&ctx->callbacks, set_bled, HAL_LED_OFF);
    CALL_HAL(&ctx->callbacks, set_rled, HAL_LED_BLINK_2HZ);
}

void wm_fsm_init(wm_context_t *ctx, const hal_output_callbacks_t *callbacks) {
    if (!ctx) {
        return;
    }

    ctx->coin_balance_cents = 0;
    ctx->remaining_cycle_sec = 0;
    ctx->stop_press_count = 0;
    ctx->stop_window_timer_ms = 0;
    ctx->active_error_flags = WM_FAULT_NONE;
    ctx->cycle_duration_setting = WM_CYCLE_DURATION_SEC;

    if (callbacks) {
        ctx->callbacks = *callbacks;
    } else {
        /* Initialize empty callbacks */
        ctx->callbacks.set_rled = NULL;
        ctx->callbacks.set_bled = NULL;
        ctx->callbacks.set_motor = NULL;
        ctx->callbacks.set_water_valve = NULL;
        ctx->callbacks.set_drain_pump = NULL;
        ctx->callbacks.set_door_lock = NULL;
        ctx->callbacks.on_cycle_complete = NULL;
    }

    enter_standby(ctx);
}

static bool handle_coin_deposit(wm_context_t *ctx, uint32_t amount) {
    /* Strict adherence to REQ-08: Only 10¢, 20¢, 50¢ coins accepted */
    if (amount != 10U && amount != 20U && amount != 50U) {
        return false;
    }

    if (ctx->state == WM_STATE_STANDBY) {
        if (ctx->coin_balance_cents <= (UINT32_MAX - amount)) {
            ctx->coin_balance_cents += amount;
        }
        if (ctx->coin_balance_cents >= WM_COIN_THRESHOLD_CENTS) {
            enter_ready(ctx);
        }
        return true;
    } else if (ctx->state == WM_STATE_READY) {
        /* Surplus money is accepted, but will be cleared upon RUN without refund */
        if (ctx->coin_balance_cents <= (UINT32_MAX - amount)) {
            ctx->coin_balance_cents += amount;
        }
        return true;
    }
    /* Ignored in RUNNING, PAUSED, ERROR */
    return false;
}

static bool handle_stop_button(wm_context_t *ctx) {
    if (ctx->state == WM_STATE_STANDBY) {
        return false;
    }

    if (ctx->stop_press_count == 0) {
        /* First press: start double-click window */
        ctx->stop_press_count = 1;
        ctx->stop_window_timer_ms = WM_DOUBLE_PRESS_WINDOW_MS;
        return true;
    } else {
        /* Second press within window: Force Stop confirmed! */
        ctx->stop_press_count = 0;
        ctx->stop_window_timer_ms = 0;
        ctx->coin_balance_cents = 0;
        enter_standby(ctx);
        return true;
    }
}

static bool handle_1s_timer_tick(wm_context_t *ctx) {
    if (ctx->remaining_cycle_sec > 1) {
        ctx->remaining_cycle_sec--;
        if (ctx->state == WM_STATE_RUNNING) {
            update_running_actuators(ctx);
        }
        return true;
    } else if (ctx->remaining_cycle_sec == 1) {
        ctx->remaining_cycle_sec = 0;
        CALL_HAL_VOID(&ctx->callbacks, on_cycle_complete);
        enter_standby(ctx);
        return true;
    }
    return false;
}

bool wm_fsm_dispatch_event(wm_context_t *ctx, wm_event_t event) {
    if (!ctx) {
        return false;
    }

    /* Global emergency error transition */
    if (event == WM_EVT_FAULT_OCCURRED) {
        if (ctx->active_error_flags == WM_FAULT_NONE) {
            ctx->active_error_flags = WM_FAULT_DOOR_OPEN;
        }
        if (ctx->state != WM_STATE_ERROR) {
            enter_error(ctx);
            return true;
        }
        return false;
    }

    /* System 1ms tick handling */
    if (event == WM_EVT_TIMER_TICK_1MS) {
        wm_fsm_tick_1ms(ctx);
        return true;
    }

    /* Dispatch based on current state */
    switch (ctx->state) {
        case WM_STATE_STANDBY:
            switch (event) {
                case WM_EVT_COIN_10:
                    return handle_coin_deposit(ctx, 10);
                case WM_EVT_COIN_20:
                    return handle_coin_deposit(ctx, 20);
                case WM_EVT_COIN_50:
                    return handle_coin_deposit(ctx, 50);
                default:
                    /* All button presses ignored in Standby */
                    return false;
            }

        case WM_STATE_READY:
            switch (event) {
                case WM_EVT_COIN_10:
                    return handle_coin_deposit(ctx, 10);
                case WM_EVT_COIN_20:
                    return handle_coin_deposit(ctx, 20);
                case WM_EVT_COIN_50:
                    return handle_coin_deposit(ctx, 50);
                case WM_EVT_BTN_RUN:
                    enter_running(ctx, false);
                    return true;
                case WM_EVT_BTN_STOP:
                    return handle_stop_button(ctx);
                default:
                    return false;
            }

        case WM_STATE_RUNNING:
            switch (event) {
                case WM_EVT_BTN_PAUSE:
                    enter_paused(ctx);
                    return true;
                case WM_EVT_BTN_STOP:
                    return handle_stop_button(ctx);
                case WM_EVT_TIMER_TICK_1S:
                    return handle_1s_timer_tick(ctx);
                default:
                    return false;
            }

        case WM_STATE_PAUSED:
            switch (event) {
                case WM_EVT_BTN_RUN:
                    enter_running(ctx, true);
                    return true;
                case WM_EVT_BTN_STOP:
                    return handle_stop_button(ctx);
                case WM_EVT_TIMER_TICK_1S:
                    /* CRITICAL REQUIREMENT: Timer continues counting down in PAUSED state! */
                    return handle_1s_timer_tick(ctx);
                default:
                    return false;
            }

        case WM_STATE_ERROR:
            if (event == WM_EVT_FAULT_CLEARED) {
                ctx->active_error_flags = WM_FAULT_NONE;
                ctx->coin_balance_cents = 0;
                enter_standby(ctx);
                return true;
            }
            return false;

        default:
            return false;
    }
}

void wm_fsm_tick_1ms(wm_context_t *ctx) {
    if (!ctx) {
        return;
    }

    /* Decrement double-press sliding window */
    if (ctx->stop_window_timer_ms > 0) {
        ctx->stop_window_timer_ms--;
        if (ctx->stop_window_timer_ms == 0) {
            /* Double-press window expired: discard single press */
            ctx->stop_press_count = 0;
        }
    }
}

void wm_fsm_tick_1s(wm_context_t *ctx) {
    wm_fsm_dispatch_event(ctx, WM_EVT_TIMER_TICK_1S);
}

const char* wm_state_to_str(wm_state_t state) {
    switch (state) {
        case WM_STATE_STANDBY: return "STANDBY";
        case WM_STATE_READY:   return "READY";
        case WM_STATE_RUNNING: return "RUNNING";
        case WM_STATE_PAUSED:  return "PAUSED";
        case WM_STATE_ERROR:   return "ERROR";
        default:               return "UNKNOWN";
    }
}

const char* wm_event_to_str(wm_event_t event) {
    switch (event) {
        case WM_EVT_NONE:           return "EVT_NONE";
        case WM_EVT_COIN_10:        return "COIN_10c";
        case WM_EVT_COIN_20:        return "COIN_20c";
        case WM_EVT_COIN_50:        return "COIN_50c";
        case WM_EVT_BTN_RUN:        return "BTN_RUN";
        case WM_EVT_BTN_PAUSE:      return "BTN_PAUSE";
        case WM_EVT_BTN_STOP:       return "BTN_STOP";
        case WM_EVT_TIMER_TICK_1S:  return "TICK_1S";
        case WM_EVT_TIMER_TICK_1MS: return "TICK_1MS";
        case WM_EVT_FAULT_OCCURRED: return "FAULT_OCCURRED";
        case WM_EVT_FAULT_CLEARED:  return "FAULT_CLEARED";
        default:                    return "UNKNOWN_EVENT";
    }
}

wm_state_t wm_fsm_get_state(const wm_context_t *ctx) {
    return ctx ? ctx->state : WM_STATE_STANDBY;
}

uint32_t wm_fsm_get_balance(const wm_context_t *ctx) {
    return ctx ? ctx->coin_balance_cents : 0;
}

uint32_t wm_fsm_get_remaining_seconds(const wm_context_t *ctx) {
    return ctx ? ctx->remaining_cycle_sec : 0;
}

uint32_t wm_fsm_get_fault_flags(const wm_context_t *ctx) {
    return ctx ? ctx->active_error_flags : 0;
}

void wm_fsm_trigger_fault(wm_context_t *ctx, uint32_t fault_mask) {
    if (!ctx) {
        return;
    }
    ctx->active_error_flags |= fault_mask;
    enter_error(ctx);
}

void wm_fsm_clear_fault(wm_context_t *ctx) {
    if (!ctx) {
        return;
    }
    ctx->active_error_flags = WM_FAULT_NONE;
    wm_fsm_dispatch_event(ctx, WM_EVT_FAULT_CLEARED);
}

const char* wm_fault_to_str(uint32_t fault_mask) {
    if (fault_mask == WM_FAULT_NONE) return "NO_FAULT";
    if (fault_mask & WM_FAULT_DOOR_OPEN) return "DOOR_LATCH_OPEN";
    if (fault_mask & WM_FAULT_WATER_TIMEOUT) return "WATER_INLET_TIMEOUT";
    if (fault_mask & WM_FAULT_MOTOR_OVERCURRENT) return "MOTOR_OVERCURRENT";
    return "MULTIPLE_FAULTS";
}

bool wm_fsm_can_accept_event(const wm_context_t *ctx, wm_event_t event) {
    if (!ctx) {
        return false;
    }

    if (event == WM_EVT_FAULT_OCCURRED) {
        return ctx->state != WM_STATE_ERROR;
    }
    if (event == WM_EVT_TIMER_TICK_1MS) {
        return true;
    }

    switch (ctx->state) {
        case WM_STATE_STANDBY:
            return (event == WM_EVT_COIN_10 || event == WM_EVT_COIN_20 || event == WM_EVT_COIN_50);

        case WM_STATE_READY:
            return (event == WM_EVT_COIN_10 || event == WM_EVT_COIN_20 || event == WM_EVT_COIN_50 ||
                    event == WM_EVT_BTN_RUN || event == WM_EVT_BTN_STOP);

        case WM_STATE_RUNNING:
            return (event == WM_EVT_BTN_PAUSE || event == WM_EVT_BTN_STOP ||
                    (event == WM_EVT_TIMER_TICK_1S && ctx->remaining_cycle_sec > 0));

        case WM_STATE_PAUSED:
            return (event == WM_EVT_BTN_RUN || event == WM_EVT_BTN_STOP ||
                    (event == WM_EVT_TIMER_TICK_1S && ctx->remaining_cycle_sec > 0));

        case WM_STATE_ERROR:
            return (event == WM_EVT_FAULT_CLEARED);

        default:
            return false;
    }
}

wm_cycle_phase_t wm_fsm_get_cycle_phase(const wm_context_t *ctx) {
    if (!ctx) {
        return WM_PHASE_IDLE;
    }

    if (ctx->state == WM_STATE_RUNNING || ctx->state == WM_STATE_PAUSED) {
        uint32_t total = (ctx->cycle_duration_setting > 0) ?
                          ctx->cycle_duration_setting : WM_CYCLE_DURATION_SEC;
        uint32_t spin_threshold = total / 6;
        if (ctx->remaining_cycle_sec <= spin_threshold && ctx->remaining_cycle_sec > 0) {
            return WM_PHASE_FINAL_SPIN;
        }
        return WM_PHASE_WASH_AGITATE;
    }

    return WM_PHASE_IDLE;
}

const char* wm_cycle_phase_to_str(wm_cycle_phase_t phase) {
    switch (phase) {
        case WM_PHASE_IDLE:         return "IDLE";
        case WM_PHASE_WASH_AGITATE: return "WASH_AGITATE";
        case WM_PHASE_FINAL_SPIN:   return "FINAL_SPIN";
        default:                    return "UNKNOWN_PHASE";
    }
}



