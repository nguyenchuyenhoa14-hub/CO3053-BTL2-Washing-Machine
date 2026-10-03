# CO3053 Embedded Systems — Assignment 2 (BTL 2)
## Coin-Operated Washing Machine Control Unit

**Instructor:** Assoc. Prof. Phạm Hoàng Anh (`anhpham@hcmut.edu.vn`)  
**Institution:** Ho Chi Minh City University of Technology (HCMUT)  
**Academic Year:** HK261  

---

## 1. Project Overview
This repository contains the production-grade embedded C implementation, Hardware Abstraction Layer (HAL), automated verification testbench (20 test cases), and interactive command-line simulator for the **Coin-Operated Washing Machine Control Unit**.

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
4. **Persistent Cycle Clock:**
   - 30-minute ($1800\,\text{s}$) countdown clock runs independently of actuator states.
   - **Critical Requirement:** Timer continues ticking down even while in `PAUSED` state. If paused until timer reaches 0, the machine automatically terminates back to `STANDBY`.

---

## 3. Architecture & Directory Layout

```text
.
├── Makefile                          # Build automation (make test, make sim)
├── README.md                         # Project documentation
├── src/
│   ├── include/
│   │   ├── washing_machine_config.h  # Timing parameters (30 min, 50¢ threshold, double click)
│   │   ├── hal_interfaces.h          # Hardware Abstraction Layer (LED, Motor, Valves)
│   │   └── washing_machine_fsm.h     # Public FSM core API, enums, context struct
│   ├── fsm/
│   │   └── washing_machine_fsm.c     # Deterministic Moore-Mealy FSM implementation
│   └── hal/
│       ├── mock_hal.h                # Virtual HAL recorder for unit testing
│       └── mock_hal.c                # Mock hardware implementation
├── tests/
│   └── test_washing_machine.c        # Automated unit test suite (20 exhaustive test cases)
└── sim/
    └── sim_interactive.c             # Interactive CLI simulator with live dashboard
```

---

## 4. Building and Running

### 4.1 Running the Automated Test Suite (100% Coverage)
```bash
# Compile and execute all 20 test cases
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

  [PASS] TC-01: Sub-threshold Deposit (10¢ + 20¢ stays in STANDBY)
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

============================================================
 ALL 26 TESTS PASSED SUCCESSFULLY! (100% Test Coverage)
============================================================
```

### 4.2 Running the Interactive CLI Simulator
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
- `c`: Clear hardware faults and recover to STANDBY
- `q`: Quit simulator
