# BTL 2: Washing Machine Control Unit (CO3053 - Embedded Systems)

## 1. Project Context & Mission
This workspace is configured for **Assignment 2 (BTL 2) - CO3053 Embedded Systems** at Ho Chi Minh City University of Technology (HCMUT), instructed by Assoc. Prof. Phạm Hoàng Anh.

**Project Core Objective:**
Design, specify, implement, and rigorously verify the **Control Unit of a Coin-Operated Washing Machine** satisfying all strict logical, timing, and fault-handling constraints defined in the assignment specification.

---

## 2. Core Operational Rules for AI Agents

1. **Deterministic FSM First**:
   - The heart of this system is a formal **Finite State Machine (FSM)**.
   - Every state transition must be mathematically deterministic and unambiguous. No hidden states or undefined event behavior.
   - States must strictly separate logical modes: `STANDBY`, `READY`, `RUNNING (WASHING)`, `PAUSED`, and `ERROR`.

2. **Strict Requirement Adherence**:
   - **Coin Accumulator**: Only 10¢, 20¢, 50¢ coins accepted. Minimum execution threshold is 50¢.
   - **No Change Return**: When `RUN` is pressed, coin balance resets to 0 immediately without refunding surplus money.
   - **Persistent 30-Minute Timer**: The 30-min countdown timer **must continue counting down** during `PAUSED` state. Pausing stops actuators, NOT the cycle clock!
   - **Double-Press Stop**: `STOP` must be pressed **twice** (within a defined double-click interval, default $T_{\text{double}} \le 1.5\,\text{s}$) to force termination. A single press must NOT terminate execution.
   - **LED Encodings**:
     - `RLED`: Solid ON = Standby (available); Blinking = Error active.
     - `BLED`: Solid ON = Ready ($\ge 50$¢ deposited); Blinking = Running (washing in progress).

3. **Layered Architecture & HAL Separation**:
   - All code must adhere to clean embedded layered architecture:
     ```
     [ Application / User Interface / CLI / Tests ]
                           ▲
     [ Washing Machine FSM Controller Core ]
                           ▲
     [ Hardware Abstraction Layer (HAL) / Virtual Drivers ]
                           ▲
     [ Physical Hardware (STM32/AVR/ESP32) OR Software Simulator ]
     ```
   - The FSM core logic must be 100% portable and completely decoupled from specific microcontroller registers.

4. **Zero Magic Numbers & Non-Blocking Design**:
   - All timings (button debounce time, double-click window, LED blink frequencies, 30-minute cycle duration) must be defined as configurable constants/enums.
   - Zero blocking `delay()` calls in the event loop. Everything is driven by tick counters or non-blocking timer checks.

5. **Branch Strategy & Privacy Discipline**:
   - Only commit and push to branch `nguyen`. NEVER push or merge into `main` until explicitly requested by the user.
   - Strictly keep `assignment1/` local (never stage or push `assignment1/` to the repository).

6. **Priority Directive: Focus on Code Perfection First (No Report Yet)**:
   - **DO NOT start writing or generating reports yet** (Phase 6 / `report/` is strictly deferred).
   - Direct 100% of effort toward researching, refining, auditing, and perfecting the codebase, unit tests, hardware drivers, simulators, timing determinism, and MISRA-C compliance until the implementation is 100% flawless (10/10).

7. **Absolute Specification Fidelity ("Đúng, Đủ, Chuẩn Hoàn Hảo - Không Làm Thiếu, Không Làm Dư")**:
   - Mọi dòng mã nguồn và hành vi FSM phải bám sát 100% từng câu chữ trong slide đề bài của PGS. TS. Phạm Hoàng Anh (`anhpham@hcmut.edu.vn`).
   - Tuyệt đối **không làm thiếu** bất kỳ yêu cầu nào (3 nút bấm STOP/RUN/PAUSE, 2 LED RLED/BLED, xu 10¢/20¢/50¢, ngưỡng 50¢, nuốt tiền không hoàn lại, timer 30 phút vẫn đếm khi Pause, bấm STOP 2 lần mới dừng, LED báo lỗi).
   - Tuyệt đối **không làm dư** các tính năng rườm rà, phức tạp hóa không cần thiết làm sai lệch tính chất cốt lõi của đề bài.
   - Code phải thanh lịch, tường minh, chuẩn nhúng MISRA-C, không rò rỉ bộ nhớ và đạt điểm 10/10 tuyệt đối.

8. **Mandatory Post-Test Housekeeping & Clean Code Discipline (Dọn Dẹp Sạch Sẽ Sau Mỗi Lần Kiểm Thử)**:
   - Mỗi lần thực hiện biên dịch hoặc chạy kiểm thử (`mingw32-make test`, `test_hal`, `stm32`, `demo`, v.v.) xong, bắt buộc phải dọn dẹp toàn bộ các tệp nhị phân thực thi (`*.exe`, `*.o`, binaries) bằng lệnh `mingw32-make clean`.
   - Giữ mã nguồn và git working tree luôn luôn ở trạng thái **Clean hoàn hảo nhất** sau mỗi lần test, tuyệt đối không để sót lại bất kỳ file thực thi rác hay tệp build tạm bợ nào trong thư mục làm việc.

---

## 3. Directory & Artifact Structure
- `.agents/rules/`: Formal specifications, FSM design standards, embedded coding standards, and verification test suites.
- `.agents/skills/btl2-washing-machine/`: Specialized runbooks and reference tables for implementing and verifying the control unit.
- `src/`: Production-grade implementation in C/C++ (modular, portable).
- `sim/`: Interactive software simulator / testbench (Python / CLI).
- `tests/`: Automated unit tests covering 100% of state transitions and edge cases.
- `report/`: Formal LaTeX documentation and report.
