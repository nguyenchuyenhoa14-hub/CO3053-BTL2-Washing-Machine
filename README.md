# CO3053 Embedded Systems — Assignment 2 (BTL 2)
## Coin-Operated Washing Machine Control Unit

**Instructor:** Assoc. Prof. Phạm Hoàng Anh (`anhpham@hcmut.edu.vn`)  
**Institution:** Ho Chi Minh City University of Technology (HCMUT)  
**Academic Year:** HK261  

> [!IMPORTANT]
> ## ▶️ VIDEO DEMO: **https://youtube.com/shorts/ZFKOPDyJHqQ**
> The control unit running on the real WeAct STM32H750 board.

---

## 1. Project Overview
This repository contains the production-grade embedded C implementation, Hardware Abstraction Layer (HAL), automated verification testbench (34 exhaustive test cases), bare-metal super-loop demonstration, and interactive command-line simulator for the **Coin-Operated Washing Machine Control Unit**.

---

## 2. Core Functional Requirements & Logic Highlights
1. **Three Control Buttons:**
   - `RUN`: Starts the 30-minute washing cycle from `READY`; resumes agitation from `PAUSED`.
   - `PAUSE`: Safely halts motor agitation while the **30-minute cycle timer continues counting down**.
   - `STOP`: Must be pressed **twice within 1.5 seconds** (double-press) to force terminate execution. Single presses are buffered and discarded if no second press occurs.
2. **Multi-Denomination Coin Acceptor:**
   - Accepts `10¢`, `20¢`, and `50¢` coins.
   - Execution threshold: Minimum accumulated balance of **$50¢$**.
   - **Zero Refund Policy:** When `RUN` is pressed, the entire coin balance is cleared to `0¢` immediately without returning redundancies/surplus.
3. **Multi-Modal LED Signalling:**
   - **Red LED (`RLED`):** Solid ON when machine is in `STANDBY` (available to serve); Blinking at 2.0 Hz when in `ERROR` state.
   - **Blue LED (`BLED`):** Solid ON when machine is in `READY` ($\ge 50¢$ deposited); Blinking at 1.0 Hz when `RUNNING` (active washing).
4. **Persistent Cycle Clock & Actuator Sub-Phases:**
   - 30-minute ($1800\,\text{s}$) countdown clock runs independently of actuator states.
   - **Critical Requirement:** Timer continues ticking down even while in `PAUSED` state. If paused until timer reaches 0, the machine automatically terminates back to `STANDBY`.
   - **Multi-Phase Profile:** Main agitation (drum reverse) for the first 5/6 of the cycle; high-speed spin dry & drain pump active for the final 1/6 of the cycle.
5. **Granular Safety Fault Diagnostics:**
   - Hardware sensor bitmask monitoring (`WM_FAULT_DOOR_OPEN`, `WM_FAULT_WATER_TIMEOUT`, `WM_FAULT_MOTOR_OVERCURRENT`).
   - Total actuator de-energization and I/O lockout during fault state.


6. **FSM design decisions beyond the written spec** (all covered by tests):
   - **`COLLECTING` state:** `0 < deposit < 50¢` is its own state (RLED on, BLED off), matching the BA analysis; it behaves like STANDBY for the user.
   - **Cancel returns the deposit:** STOP pressed twice in `COLLECTING`/`READY` returns the money through the optional HAL callback `return_coins()` instead of silently discarding it. Pressing `RUN` still follows the zero-refund policy (BR-02): surplus is never returned.
   - **Fault recovery keeps the customer's context** (`WM_ERROR_RESUMES_CYCLE`, default 1): the state before the fault is remembered. A deposit survives a fault; a cycle in progress returns to `PAUSED` when the fault is cleared (the user presses `RUN` to continue) and its timer keeps counting during the fault, like during `PAUSE`. Build with `-DWM_ERROR_RESUMES_CYCLE=0` for the legacy behaviour (fault always returns to STANDBY and discards deposit and cycle).
   - **Assumptions:** STOP double-press window = 1.5 s; the BLED is solid while `PAUSED`; the spec does not define fault sources, so faults are injected (door switch / sensors).

---

## 3. Architecture & Directory Layout

```text
.
├── Makefile                          # Build automation (test, test_hal, sim, demo, stm32)
├── README.md                         # Project documentation
├── src/
│   ├── include/
│   │   ├── washing_machine_config.h  # Timing parameters (30 min, 50¢ threshold, double click)
│   │   ├── hal_interfaces.h          # Hardware Abstraction Layer interfaces
│   │   └── washing_machine_fsm.h     # Public FSM core API, enums, context struct
│   ├── fsm/
│   │   └── washing_machine_fsm.c     # Deterministic Moore-Mealy FSM implementation
│   ├── hal/
│   │   ├── mock_hal.h / .c           # Virtual HAL recorder for unit testing
│   │   ├── hal_button_engine.h / .c  # 30ms debounce & coin pulse validator engine
│   │   ├── hal_led_blinker.h / .c    # 1.0Hz / 2.0Hz non-blocking LED & actuator guard
│   │   └── stm32/                    # Bare-metal STM32 (ARM Cortex-M) HAL driver
│   │       ├── stm32_compat.h        # Portable CMSIS register definitions
│   │       ├── hal_stm32_gpio.h / .c # STM32 GPIO registers driver
│   │       ├── hal_stm32_callbacks.h # Output callbacks bound to physical pins
│   │       └── main_stm32.c          # STM32 SysTick 1ms super-loop entry point
│   └── main.c                        # Bare-metal super-loop demonstration
├── tests/
│   ├── test_washing_machine.c        # Automated FSM test suite (34 test cases, 100% pass)
│   └── test_hal_engines.c            # Automated HAL test suite (debouncing, pulses, blinkers)
└── sim/
    ├── sim_interactive.c             # Interactive CLI simulator with live dashboard
    └── wokwi/                        # Interactive Wokwi web simulation project
        ├── diagram.json              # Full schematic (Uno, LCD1602, 3 buttons, 2 LEDs, 4 relays)
        ├── sketch.ino                # Real-time embedded firmware with LCD display
        ├── libraries.txt             # Wokwi dependencies (LiquidCrystal I2C)
        └── wokwi.toml                # Wokwi configuration
```

---

## 4. Building and Running

### 4.1 Running the Automated Test Suite (100% Coverage)
```bash
# Compile and execute all 34 test cases
mingw32-make test
# or with standard make:
make test
```

Expected output:
```text
============================================================
 BTL 2: Washing Machine Control Unit - Verification Suite
 CO3053 Embedded Systems - HCMUT
============================================================

  [PASS] TC-01: Sub-threshold Deposit (10¢ + 20¢ stays in COLLECTING, RLED on)
  [PASS] TC-02: Exact Threshold Deposit (50¢ transitions to READY)
  [PASS] TC-03: Surplus Deposit Accumulation (Accepts 60¢, 110¢ in READY)
  [PASS] TC-04: Execution & Zero Refund (70¢ cleared to 0¢, 30-min timer active)
  [PASS] TC-05: Premature RUN Attempt (Ignored when balance < 50¢)
  [PASS] TC-06: Normal Pause and Resume (Actuators safely suspended and resumed)
  [PASS] TC-07: Persistent Timer in Pause (Timer ticked down from 1600s to 1300s while paused)
  [PASS] TC-08: Pause Timeout Termination (Timer expiring in PAUSED resets to STANDBY)
  [PASS] TC-09: Single STOP Rejection (Single press does not force stop)
  [PASS] TC-10: Force Stop on Double Press (2 presses within 300ms forces termination)
  [PASS] TC-11: Force Stop from Paused State (Double STOP terminates paused machine)
  [PASS] TC-12: Normal 30-min Cycle Completion (1800s expires naturally to STANDBY)
  [PASS] TC-13: Fault Interruption and Recovery (Safety shutdown & error reset)
  [PASS] TC-14: Rapid Alternating PAUSE/RUN Toggling (Stress test on clock & motor)
  [PASS] TC-15: Coin Rejection During Active Cycle (Coins rejected in RUNNING, PAUSED, ERROR)
  [PASS] TC-16: Fault in STANDBY and READY States (Consistent error transition & LED signalling)
  [PASS] TC-17: Complete Lockout During ERROR State (Buttons and coins strictly locked)
  [PASS] TC-18: Ready State Double STOP Cancellation (User cancel before run)
  [PASS] TC-19: Multiple Isolated Single STOPS (Spaced presses never falsely force stop)
  [PASS] TC-20: Null Pointer and API Resilience (Zero segmentation faults, robust error handling)
  [PASS] TC-21: Consecutive Multi-Cycle Sessions (Flawless back-to-back operations without leakage)
  [PASS] TC-22: Boundary Double-Stop Timing (Exact 1499ms hit vs 1501ms expiration verified)
  [PASS] TC-23: Single STOP in READY (Preserves accumulated deposit against accidental touch)
  [PASS] TC-24: Granular Fault Diagnostics (Multi-sensor bitmask tracking and string reports)
  [PASS] TC-25: Arithmetic Overflow Resilience (MISRA-C Rule 12.4 wrap-around defense)
  [PASS] TC-26: Multi-Phase Wash Profile (Agitate -> Spin/Drain -> Complete verified)
  [PASS] TC-27: Event Acceptance Query Protocol (Deterministic event filtering across the states)
  [PASS] TC-28: Cycle Sub-Phase Query & Enum Decoders (Correct phase detection throughout cycle)
  [PASS] TC-31: COLLECTING State (RLED on until the 50¢ threshold, then READY)
  [PASS] TC-32: Cancel Returns Deposit (STOP x2 in COLLECTING/READY refunds; RUN still zero-refund)
  [PASS] TC-33: Fault Preserves Deposit (COLLECTING/READY restored after the fault is cleared)
  [PASS] TC-34: Fault During Cycle (timer keeps counting, cleared fault resumes in PAUSED, expiry respected)
  [PASS] TC-29: Pause Across Phase Boundary (Continuous timer crosses into Spin)
  [PASS] TC-30: MISRA-C Boundary & Corrupted Enum Resilience (100% defensive branch safety)

============================================================
 ALL 30 TESTS PASSED SUCCESSFULLY! (100% Specification & Transition Coverage)
============================================================
```

### 4.2 Running the Hardware HAL Engines Test Suite
```bash
# Verify debouncing, coin pulse validation, LED blinkers, and actuator interlock guard
mingw32-make test_hal
```

### 4.3 Running the STM32 Bare-Metal Driver
```bash
# Compile and run the STM32 SysTick super-loop controller
mingw32-make stm32
```

### 4.4 Running the Bare-Metal Super-Loop Demo
```bash
# Build and execute the bare-metal super-loop demonstration
mingw32-make demo
```

### 4.5 Running the Interactive CLI Simulator
```bash
# Build the interactive simulator
mingw32-make sim

# Launch simulator
./sim_wm.exe
```

**Commands inside simulator:**
- `1`: Insert 10¢ coin
- `2`: Insert 20¢ coin
- `3`: Insert 50¢ coin
- `r`: Press RUN
- `p`: Press PAUSE
- `s`: Press STOP once (Starts 1.5s sliding window)
- `ss`: Press STOP twice (Force stop)
- `t <seconds>`: Fast-forward time (e.g. `t 60` to advance 1 minute)
- `e1`: Simulate Lid Open Fault
- `e2`: Simulate Water Timeout Fault
- `e3`: Simulate Motor Overcurrent Fault
- `c`: Clear hardware faults and recover to the state before the fault
- `q`: Quit simulator

### 4.6 Running Interactive Wokwi Web Simulation
Open [https://wokwi.com/projects/new/arduino-uno](https://wokwi.com/projects/new/arduino-uno), paste [`sim/wokwi/diagram.json`](./sim/wokwi/diagram.json) into the diagram tab and [`sim/wokwi/sketch.ino`](./sim/wokwi/sketch.ino) into the code tab, add `LiquidCrystal I2C` library, and click **Play** to run interactive simulation in your browser!

## Chạy trên board WeAct STM32H750

Hướng dẫn sử dụng đầy đủ: [`HDSD_BOARD_WEACT_H750.md`](HDSD_BOARD_WEACT_H750.md). Tóm tắt kỹ thuật: [`board/weact_h750/README.md`](board/weact_h750/README.md): `make h750`, `make flash`
(USB-DFU hoặc ST-Link), hiển thị trạng thái trên LCD 0.96" và điều khiển FSM bằng nút K1 (click / double-click / giữ).
