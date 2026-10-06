/**
 * @file washing_machine_config.h
 * @brief System configuration constants for Washing Machine Control Unit (CO3053 - BTL 2)
 */

#ifndef WASHING_MACHINE_CONFIG_H
#define WASHING_MACHINE_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Full cycle duration in seconds (30 minutes = 1800s as per specification).
 */
#define WM_CYCLE_DURATION_SEC          (1800U)

/**
 * @brief Scaled cycle duration used during accelerated unit testing (30s).
 */
#define WM_CYCLE_DURATION_TEST_SEC     (30U)

/**
 * @brief Minimum accumulated coin balance required to reach READY state (50 cents).
 */
#define WM_COIN_THRESHOLD_CENTS        (50U)

/**
 * @brief Time window to register a double-press on the STOP button (1500 ms = 1.5s).
 */
#define WM_DOUBLE_PRESS_WINDOW_MS      (1500U)

/**
 * @brief Software debounce filter duration for digital button inputs (30 ms).
 */
#define WM_DEBOUNCE_TIME_MS            (30U)

/**
 * @brief LED blinking period in RUNNING state (1.0 Hz -> 500ms ON / 500ms OFF).
 */
#define WM_BLED_BLINK_PERIOD_MS        (1000U)

/**
 * @brief LED blinking period in ERROR state (2.0 Hz -> 250ms ON / 250ms OFF).
 */
#define WM_RLED_BLINK_PERIOD_MS        (500U)

/**
 * @brief Fault recovery policy (1 = resume, 0 = legacy "abandon").
 * @details 1: the state before a fault is remembered. Deposits are kept (STANDBY/COLLECTING/READY return
 *          to themselves); a RUNNING/PAUSED cycle returns to PAUSED when the fault is cleared and its
 *          timer keeps counting during the fault (same rule as PAUSE). 0: clearing a fault always
 *          returns to STANDBY with the deposit and the cycle discarded.
 */
#ifndef WM_ERROR_RESUMES_CYCLE
#define WM_ERROR_RESUMES_CYCLE         (1)
#endif

/**
 * @brief Diagnostic bitmasks for safety faults (Sensor inputs)
 */
#define WM_FAULT_NONE                  (0x00U)
#define WM_FAULT_DOOR_OPEN             (0x01U) /**< Safety interlock open during spin/wash */
#define WM_FAULT_WATER_TIMEOUT         (0x02U) /**< Inlet water filling timeout */
#define WM_FAULT_MOTOR_OVERCURRENT     (0x04U) /**< Motor stall / overcurrent detected */

#ifdef __cplusplus
}
#endif

#endif /* WASHING_MACHINE_CONFIG_H */
