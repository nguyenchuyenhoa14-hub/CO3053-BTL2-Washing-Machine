#!/usr/bin/env python3
"""
Interactive Software Simulator for Coin-Operated Washing Machine Control Unit
CO3053 - Embedded Systems - HCMUT (Assignment 2 / BTL 2)

Features:
- Pure Python 3 implementation of the deterministic 5-state FSM.
- Interactive live CLI dashboard with ANSI colors.
- Real-time or simulated time advance.
- Automated self-verification mode (python sim/simulator.py --test).
"""

import sys
import time
from enum import Enum, auto

class State(Enum):
    STANDBY = "STANDBY"
    READY   = "READY"
    RUNNING = "RUNNING"
    PAUSED  = "PAUSED"
    ERROR   = "ERROR"

class LedMode(Enum):
    OFF = "OFF"
    ON = "SOLID ON"
    BLINK_1HZ = "BLINK (1Hz)"
    BLINK_2HZ = "BLINK (2Hz)"

class MotorMode(Enum):
    OFF = "OFF"
    AGITATE = "AGITATING (Wash)"
    SPIN = "SPINNING (1000 RPM)"

class WashingMachineSimulator:
    CYCLE_DURATION_SEC = 1800
    COIN_THRESHOLD_CENTS = 50
    DOUBLE_PRESS_WINDOW_MS = 1500

    def __init__(self, cycle_sec: int = 1800):
        self.state = State.STANDBY
        self.balance_cents = 0
        self.remaining_sec = 0
        self.cycle_duration = cycle_sec
        self.stop_press_count = 0
        self.stop_window_ms = 0
        self.active_faults = 0

        # Actuators and LEDs
        self.rled = LedMode.ON
        self.bled = LedMode.OFF
        self.motor = MotorMode.OFF
        self.drain_pump = False
        self.door_locked = False

    def enter_standby(self):
        self.state = State.STANDBY
        self.balance_cents = 0
        self.remaining_sec = 0
        self.stop_press_count = 0
        self.stop_window_ms = 0
        self.rled = LedMode.ON
        self.bled = LedMode.OFF
        self.motor = MotorMode.OFF
        self.drain_pump = False
        self.door_locked = False

    def enter_ready(self):
        self.state = State.READY
        self.rled = LedMode.OFF
        self.bled = LedMode.ON

    def _update_running_actuators(self):
        self.door_locked = True
        spin_threshold = self.cycle_duration // 6  # Last 5 mins of 30 mins
        if 0 < self.remaining_sec <= spin_threshold:
            self.motor = MotorMode.SPIN
            self.drain_pump = True
        else:
            self.motor = MotorMode.AGITATE
            self.drain_pump = False

    def enter_running(self, is_resuming: bool = False):
        self.state = State.RUNNING
        self.stop_press_count = 0
        self.stop_window_ms = 0
        if not is_resuming:
            # Unconditional Zero-Refund Rule
            self.balance_cents = 0
            self.remaining_sec = self.cycle_duration

        self.rled = LedMode.OFF
        self.bled = LedMode.BLINK_1HZ
        self._update_running_actuators()

    def enter_paused(self):
        self.state = State.PAUSED
        self.stop_press_count = 0
        self.stop_window_ms = 0
        self.motor = MotorMode.OFF
        self.drain_pump = False
        self.bled = LedMode.ON  # Solid ON in pause

    def enter_error(self, fault_code: int = 1):
        self.state = State.ERROR
        self.active_faults |= fault_code  # Bitmask accumulation (mirrors C core)
        self.stop_press_count = 0
        self.stop_window_ms = 0
        self.motor = MotorMode.OFF
        self.drain_pump = False
        self.door_locked = False
        self.bled = LedMode.OFF
        self.rled = LedMode.BLINK_2HZ

    def insert_coin(self, amount: int) -> bool:
        if amount not in (10, 20, 50):
            return False

        if self.state == State.STANDBY:
            self.balance_cents += amount
            if self.balance_cents >= self.COIN_THRESHOLD_CENTS:
                self.enter_ready()
            return True
        elif self.state == State.READY:
            self.balance_cents += amount
            return True
        # Coins rejected in RUNNING, PAUSED, ERROR
        return False

    def press_run(self) -> bool:
        if self.state == State.READY:
            self.enter_running(is_resuming=False)
            return True
        elif self.state == State.PAUSED:
            self.enter_running(is_resuming=True)
            return True
        return False

    def press_pause(self) -> bool:
        if self.state == State.RUNNING:
            self.enter_paused()
            return True
        return False

    def press_stop(self) -> bool:
        if self.state == State.STANDBY:
            return False

        if self.stop_press_count == 0:
            self.stop_press_count = 1
            self.stop_window_ms = self.DOUBLE_PRESS_WINDOW_MS
            return True
        else:
            # Second press within 1.5s: Force Stop!
            self.enter_standby()
            return True

    def trigger_fault(self, fault_code: int = 1):
        self.enter_error(fault_code)

    def clear_fault(self):
        if self.state == State.ERROR:
            self.active_faults = 0
            self.enter_standby()

    def tick_1ms(self, delta_ms: int = 1):
        if self.stop_window_ms > 0:
            if self.stop_window_ms > delta_ms:
                self.stop_window_ms -= delta_ms
            else:
                self.stop_window_ms = 0
                self.stop_press_count = 0

    def tick_1s(self):
        # Countdown operates in BOTH RUNNING and PAUSED
        if self.state in (State.RUNNING, State.PAUSED):
            if self.remaining_sec > 1:
                self.remaining_sec -= 1
                if self.state == State.RUNNING:
                    self._update_running_actuators()
            else:
                self.remaining_sec = 0
                self.enter_standby()

    def advance_time(self, seconds: int):
        for _ in range(seconds):
            self.tick_1ms(1000)
            self.tick_1s()

    def get_dashboard_str(self) -> str:
        G = "\033[1;32m"
        R = "\033[1;31m"
        Y = "\033[1;33m"
        B = "\033[1;34m"
        C = "\033[1;36m"
        W = "\033[1;37m"
        N = "\033[0m"

        state_color = {
            State.STANDBY: G,
            State.READY: B,
            State.RUNNING: Y,
            State.PAUSED: Y,
            State.ERROR: R
        }[self.state]

        phase_str = "IDLE"
        if self.state in (State.RUNNING, State.PAUSED):
            spin_thresh = self.cycle_duration // 6
            phase_str = "FINAL_SPIN" if self.remaining_sec <= spin_thresh else "WASH_AGITATE"

        min_val = self.remaining_sec // 60
        sec_val = self.remaining_sec % 60

        lines = [
            f"\n{C}======================================================================{N}",
            f"{W}  COIN-OPERATED WASHING MACHINE CONTROL UNIT — INTERACTIVE SIMULATOR{N}",
            f"{C}  HCMUT CO3053 Embedded Systems — Assignment 2 (BTL 2){N}",
            f"{C}======================================================================{N}",
            f" [STATE]      : {state_color}[ {self.state.value:<7} ]{N}",
            f" [BALANCE]    : {G}${self.balance_cents / 100:.2f}{N} (Min threshold: $0.50)",
            f" [TIMER]      : {C}{min_val:02d}:{sec_val:02d}{N} remaining [{W}{phase_str}{N}]",
            f" [RLED (Red)] : {R}[ {self.rled.value} ]{N}",
            f" [BLED (Blue)]: {B}[ {self.bled.value} ]{N}",
            f" [MOTOR]      : {G if self.motor != MotorMode.OFF else W}[ {self.motor.value} ]{N}",
            f" [DRAIN PUMP] : {G if self.drain_pump else W}[ {'ACTIVE (Pumping)' if self.drain_pump else 'OFF'} ]{N}",
            f" [DOOR LOCK]  : {R if self.door_locked else G}[ {'LOCKED' if self.door_locked else 'UNLOCKED'} ]{N}"
        ]

        if self.stop_press_count > 0:
            lines.append(f" {Y}[STOP NOTICE]: 1st press recorded! {self.stop_window_ms}ms remaining to double-stop.{N}")

        if self.state == State.ERROR:
            fault_map = {1: "DOOR_LATCH_OPEN", 2: "WATER_INLET_TIMEOUT", 4: "MOTOR_OVERCURRENT"}
            lines.append(f" {R}[FAULT CODE ]: {fault_map.get(self.active_faults, 'MULTIPLE_FAULTS')} (Mask: 0x{self.active_faults:02X}){N}")

        lines.extend([
            f"{C}======================================================================{N}",
            f" COMMANDS:",
            f"  [1] Insert 10¢    [2] Insert 20¢    [3] Insert 50¢",
            f"  [r] Press RUN     [p] Press PAUSE   [s] Press STOP (Single click)",
            f"  [ss] Double STOP (Force stop)       [t <sec>] Advance simulated time",
            f"  [e1] Fault: Door Open               [e2] Fault: Water Timeout",
            f"  [e3] Fault: Motor Overcurrent       [c] Clear Fault & Reset",
            f"  [q] Quit simulator",
            f"{C}----------------------------------------------------------------------{N}"
        ])
        return "\n".join(lines)


def run_automated_self_test() -> bool:
    """Execute all 13 canonical assignment test scenarios in Python simulator."""
    sim = WashingMachineSimulator()
    print("Running Automated Self-Test (All 13 Canonical Scenarios)...")

    # TC-01: Sub-threshold deposit
    sim.insert_coin(10)
    sim.insert_coin(20)
    assert sim.state == State.STANDBY and sim.balance_cents == 30, "TC-01 Failed"

    # TC-02: Exact threshold
    sim.enter_standby()
    sim.insert_coin(50)
    assert sim.state == State.READY and sim.balance_cents == 50, "TC-02 Failed"

    # TC-03: Surplus deposit
    sim.insert_coin(20)
    assert sim.state == State.READY and sim.balance_cents == 70, "TC-03 Failed"

    # TC-04: Zero refund on RUN
    sim.press_run()
    assert sim.state == State.RUNNING and sim.balance_cents == 0 and sim.remaining_sec == 1800, "TC-04 Failed"

    # TC-05: Premature run attempt
    sim.enter_standby()
    sim.insert_coin(20)
    res = sim.press_run()
    assert not res and sim.state == State.STANDBY, "TC-05 Failed"

    # TC-06: Pause and resume
    sim.insert_coin(50)
    sim.press_run()
    sim.press_pause()
    assert sim.state == State.PAUSED and sim.motor == MotorMode.OFF, "TC-06 Failed"
    sim.press_run()
    assert sim.state == State.RUNNING and sim.motor == MotorMode.AGITATE, "TC-06 Resume Failed"

    # TC-07: Persistent clock in pause
    sim.press_pause()
    t_before = sim.remaining_sec
    sim.advance_time(60)
    assert sim.state == State.PAUSED and sim.remaining_sec == (t_before - 60), "TC-07 Failed"

    # TC-08: Pause timeout
    sim.advance_time(sim.remaining_sec)
    assert sim.state == State.STANDBY and sim.remaining_sec == 0, "TC-08 Failed"

    # TC-09: Single stop rejection
    sim.insert_coin(50)
    sim.press_run()
    sim.press_stop()
    assert sim.state == State.RUNNING, "TC-09 Failed: Single stop must not stop machine"
    sim.tick_1ms(1600)  # window expired
    assert sim.stop_press_count == 0, "TC-09 Failed: Window should reset"

    # TC-10: Force stop on double stop
    sim.press_stop()
    sim.tick_1ms(200)
    sim.press_stop()
    assert sim.state == State.STANDBY and sim.motor == MotorMode.OFF, "TC-10 Failed"

    # TC-11: Normal 30-min completion
    sim.insert_coin(50)
    sim.press_run()
    sim.advance_time(1800)
    assert sim.state == State.STANDBY and sim.door_locked is False, "TC-11 Failed"

    # TC-12: Fault interruption
    sim.insert_coin(50)
    sim.press_run()
    sim.trigger_fault(1)
    assert sim.state == State.ERROR and sim.motor == MotorMode.OFF and sim.rled == LedMode.BLINK_2HZ, "TC-12 Failed"

    # TC-13: Fault recovery
    sim.clear_fault()
    assert sim.state == State.STANDBY and sim.rled == LedMode.ON, "TC-13 Failed"

    print("ALL 13 CANONICAL TEST SCENARIOS PASSED 100% IN PYTHON SIMULATOR!")
    return True


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ("--test", "-t"):
        success = run_automated_self_test()
        sys.exit(0 if success else 1)

    sim = WashingMachineSimulator()

    while True:
        print(sim.get_dashboard_str())
        try:
            cmd = input(" Enter command > ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print("\nExiting simulator. Goodbye!")
            break

        if cmd in ("q", "quit", "exit"):
            print("\nExiting simulator. Goodbye!")
            break
        elif cmd == "1":
            sim.insert_coin(10)
        elif cmd == "2":
            sim.insert_coin(20)
        elif cmd == "3":
            sim.insert_coin(50)
        elif cmd in ("r", "run"):
            sim.press_run()
        elif cmd in ("p", "pause"):
            sim.press_pause()
        elif cmd in ("s", "stop"):
            sim.press_stop()
        elif cmd == "ss":
            sim.press_stop()
            sim.tick_1ms(200)
            sim.press_stop()
        elif cmd.startswith("t"):
            parts = cmd.split()
            sec = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 1
            sim.advance_time(sec)
        elif cmd == "e1":
            sim.trigger_fault(1)
        elif cmd == "e2":
            sim.trigger_fault(2)
        elif cmd == "e3":
            sim.trigger_fault(4)
        elif cmd in ("c", "clear"):
            sim.clear_fault()
        elif cmd:
            print(f"\033[1;33mUnknown command: '{cmd}'\033[0m")
            time.sleep(0.5)


if __name__ == "__main__":
    main()
