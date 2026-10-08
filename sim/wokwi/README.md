# Wokwi Embedded Simulation Guide (CO3053 - Assignment 2)

This directory contains the configuration and source code for the **interactive browser-based hardware simulation (Wokwi Simulator)** and VS Code Wokwi extension.

---

## 1. Hardware Circuit Components

1. **Microcontroller Unit:**
   * **Arduino Uno** (ATmega328P).
2. **Display Subsystem:**
   * **LCD 1602 I2C Display** (address `0x27`, connected to pins `A4-SDA`, `A5-SCL`).
   * Row 1: Machine state (`STANDBY`, `READY`, `RUN`, `PAUS`) and countdown timer `MM:SS`.
   * Row 2: Motor state (`AGIT`, `SPIN`, `OFF`), Drain Pump (`PUMP: 1/0`), and Door Lock (`LCK: 1/0`).
3. **Control Buttons (Active-Low with internal pull-up):**
   * **RUN** Button (Green) - Pin `D2`.
   * **PAUSE** Button (Yellow) - Pin `D3`.
   * **STOP** Button (Red) - Pin `D4` (Captures double-press within $T_{\text{double}} \le 1.5\,\text{s}$).
4. **Coin Insertion Buttons:**
   * **10¢** Button (White) - Pin `D5`.
   * **20¢** Button (White) - Pin `D6`.
   * **50¢** Button (Orange) - Pin `D7`.
5. **Fault Injection Sensor:**
   * **DOOR FAULT** Slide Switch - Pin `D8` (Toggle to simulate opening door during washing cycle -> triggers `ERROR` state).
6. **Indicator LEDs:**
   * **RLED** (Red) - Pin `D9` with 220 Ohm resistor: Solid ON in `STANDBY`, blinking at 2.0 Hz in `ERROR`.
   * **BLED** (Blue) - Pin `D10` with 220 Ohm resistor: Solid ON in `READY`, blinking at 1.0 Hz in `RUNNING`.
7. **Actuator Indicators (Relays):**
   * **MTR AGITATE** LED (Green) - Pin `D11` (Agitation cycle).
   * **MTR SPIN DRY** LED (Yellow) - Pin `D12` (High-speed spinning).
   * **DRAIN PUMP** LED (Cyan) - Pin `D13` (Drain pump active).
   * **DOOR LOCK** LED (White) - Pin `A0` (Door latch engaged).

---

## 2. Launching Browser Simulation

1. Open [https://wokwi.com/projects/new/arduino-uno](https://wokwi.com/projects/new/arduino-uno).
2. Replace the **`diagram.json`** tab with [`sim/wokwi/diagram.json`](./diagram.json).
3. Replace the **`sketch.ino`** tab with [`sim/wokwi/sketch.ino`](./sketch.ino).
4. Add the `LiquidCrystal I2C` library in the **Library Manager** tab.
5. Click **Start Simulation (Play)**.

