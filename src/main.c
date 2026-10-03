/**
 * @file main.c
 * @brief Canonical Bare-Metal Super-Loop Demonstration for Washing Machine Control Unit
 * @details Conforms to CO3053 Embedded Systems Architecture and MISRA-C guidelines.
 *          Demonstrates hardware abstraction layer (HAL) binding, deterministic FSM
 *          dispatching, and non-blocking event scheduling in an embedded super-loop.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "washing_machine_fsm.h"
#include "mock_hal.h"

/* ANSI Color formatting for terminal display */
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31;1m"
#define ANSI_GREEN   "\033[32;1m"
#define ANSI_YELLOW  "\033[33;1m"
#define ANSI_BLUE    "\033[34;1m"
#define ANSI_CYAN    "\033[36;1m"
#define ANSI_WHITE   "\033[37;1m"

static void print_step_banner(const char *step_num, const char *step_desc) {
    printf(ANSI_CYAN "\n[%s] %s\n" ANSI_RESET, step_num, step_desc);
    printf("------------------------------------------------------------\n");
}

static void log_system_status(const wm_context_t *ctx) {
    const mock_hal_state_t *hal = mock_hal_get_state();
    printf("  State: " ANSI_YELLOW "%-8s" ANSI_RESET 
           " | Balance: %3u\xC2\xA2"
           " | Timer: %3us [%-12s]"
           " | Door: %s"
           " | Motor: %-8s"
           " | Drain: %s\n",
           wm_state_to_str(ctx->state),
           ctx->coin_balance_cents,
           ctx->remaining_cycle_sec,
           wm_cycle_phase_to_str(wm_fsm_get_cycle_phase(ctx)),
           hal->door_locked ? ANSI_RED "LOCKED" ANSI_RESET : ANSI_GREEN "UNLOCKED" ANSI_RESET,
           (hal->motor == HAL_MOTOR_SPIN) ? ANSI_RED "SPIN" ANSI_RESET :
           (hal->motor == HAL_MOTOR_AGITATE) ? ANSI_BLUE "AGITATE" ANSI_RESET : "OFF",
           hal->drain_pump_on ? ANSI_BLUE "ON" ANSI_RESET : "OFF");
}

/**
 * @brief Helper to advance simulated time in milliseconds and seconds
 */
static void advance_super_loop_time(wm_context_t *ctx, uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        wm_fsm_dispatch_event(ctx, WM_EVT_TIMER_TICK_1MS);
        if ((i + 1) % 1000 == 0) {
            wm_fsm_dispatch_event(ctx, WM_EVT_TIMER_TICK_1S);
        }
    }
}

int main(void) {
    printf(ANSI_CYAN "============================================================\n" ANSI_RESET);
    printf(ANSI_CYAN " HCMUT CO3053 - Embedded Systems Assignment 2 (BTL 2)\n" ANSI_RESET);
    printf(ANSI_CYAN " Bare-Metal Super-Loop Execution Demonstration\n" ANSI_RESET);
    printf(ANSI_CYAN "============================================================\n" ANSI_RESET);

    /* 1. HAL & FSM Initialization */
    print_step_banner("STEP 1", "Hardware Abstraction Layer & FSM Bootstrapping");
    mock_hal_reset();
    hal_output_callbacks_t cbs = mock_hal_get_callbacks();
    wm_context_t ctx;
    wm_fsm_init(&ctx, &cbs);

    /* Configure a short 30-second cycle for demo clarity (spin starts at <= 5s) */
    ctx.cycle_duration_setting = 30;

    log_system_status(&ctx);
    printf("  RLED: %d, BLED: %d\n", mock_hal_get_state()->rled, mock_hal_get_state()->bled);

    /* 2. Coin Insertion */
    print_step_banner("STEP 2", "Coin Insertion (20\xC2\xA2 + 50\xC2\xA2 = 70\xC2\xA2, Threshold: 50\xC2\xA2)");
    printf("  Inserting 20\xC2\xA2...\n");
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_20);
    log_system_status(&ctx);

    printf("  Inserting 50\xC2\xA2 (crosses threshold -> transitions to READY)...\n");
    wm_fsm_dispatch_event(&ctx, WM_EVT_COIN_50);
    log_system_status(&ctx);
    printf("  RLED: %d, BLED: %d (BLED solid ON = READY)\n", 
           mock_hal_get_state()->rled, mock_hal_get_state()->bled);

    /* 3. Cycle Start */
    print_step_banner("STEP 3", "Triggering RUN (Zero-Refund Rule & Agitation Phase)");
    printf("  Pressing RUN button...\n");
    wm_fsm_dispatch_event(&ctx, WM_EVT_BTN_RUN);
    log_system_status(&ctx);
    printf("  -> Balance reset to 0\xC2\xA2 immediately (No change returned).\n");
    printf("  -> Door locked securely.\n");
    printf("  -> Motor entering AGITATION mode.\n");

    /* 4. Progress Wash Cycle (Agitate -> Spin & Drain) */
    print_step_banner("STEP 4", "Wash Progression: Main Agitation Phase (Remaining 30s -> 6s)");
    advance_super_loop_time(&ctx, 24000); /* 24 seconds advance */
    log_system_status(&ctx);

    print_step_banner("STEP 5", "Actuator Profile Transition: High-Speed SPIN & DRAIN PUMP (<= 5s)");
    advance_super_loop_time(&ctx, 1000); /* 1 second advance -> remaining 5s */
    log_system_status(&ctx);
    printf("  -> Drum accelerated to high-speed SPIN.\n");
    printf("  -> Drain pump energized for wastewater evacuation.\n");

    /* 5. Cycle Completion */
    print_step_banner("STEP 6", "Cycle Completion & Automatic Door Unlock");
    advance_super_loop_time(&ctx, 5000); /* Finish remaining 5 seconds */
    log_system_status(&ctx);
    printf("  -> Actuators de-energized.\n");
    printf("  -> Door unlocked for user collection.\n");
    printf("  -> System restored to STANDBY.\n");

    /* 6. Fault Handling & Safety Lockout Demonstration */
    print_step_banner("STEP 7", "Safety Diagnostics: Door Sensor Fault Interruption");
    printf("  Injecting hardware fault: WM_FAULT_DOOR_OPEN...\n");
    wm_fsm_trigger_fault(&ctx, WM_FAULT_DOOR_OPEN);
    log_system_status(&ctx);
    printf("  Active Fault Code: %s (Mask: 0x%02X)\n", 
           wm_fault_to_str(wm_fsm_get_fault_flags(&ctx)), 
           wm_fsm_get_fault_flags(&ctx));
    printf("  RLED Blinking frequency: 2Hz (Safety alert)\n");

    /* Verify event rejection in error */
    printf("  Attempting coin deposit while in ERROR: Accepted? %s\n",
           wm_fsm_can_accept_event(&ctx, WM_EVT_COIN_50) ? "YES" : "NO (Protected)");

    /* Fault Clearing */
    print_step_banner("STEP 8", "Fault Resolution & Clean Recovery to STANDBY");
    printf("  Clearing fault...\n");
    wm_fsm_clear_fault(&ctx);
    log_system_status(&ctx);
    printf("  Fault Flags: 0x%02X (%s)\n", 
           wm_fsm_get_fault_flags(&ctx), 
           wm_fault_to_str(wm_fsm_get_fault_flags(&ctx)));

    printf(ANSI_GREEN "\n============================================================\n" ANSI_RESET);
    printf(ANSI_GREEN " SUPER-LOOP DEMO EXECUTION COMPLETE (100%% SUCCESS)\n" ANSI_RESET);
    printf(ANSI_GREEN "============================================================\n" ANSI_RESET);

    return 0;
}
