# WeAct STM32H750VBT6 target

Washing Machine Control Unit running on the **WeAct Studio STM32H750VBT6 core board** with the
0.96" 160x80 ST7735 LCD attached. Same portable FSM/HAL code as the host build; only this folder is
board specific.

## Hardware used
| Function | Pin | Notes |
|---|---|---|
| Button K1 | PC13 | active HIGH, internal pull-down |
| LED | PE3 | active HIGH, mirrors `RLED \| BLED` (blink 1 Hz RUNNING, 2 Hz ERROR) |
| LCD SCK / MOSI | PE12 / PE14 | SPI4 AF5, 12.5 MHz, TX only |
| Ext RUN/PAUSE, STOP (optional) | PA0, PA1 | to GND, internal pull-up |
| LCD CS / DC / BL | PE11 / PE13 / PE10 | RST is wired to NRST |

The relays/motor/pump/door-lock have no pins on this board, so they are shown as indicators on the LCD.

## The 3 onboard buttons (schematic V1.2: SW1/SW2/SW3)
| Button | Wired to | Software can read it? | Role here |
|---|---|---|---|
| K1 (SW3) | PC13 (R8 10k + C4 100 nF) | **yes** - the only programmable input | all FSM control (gestures below) |
| NRST (SW1) | NRST (reset chip) | no - it resets the MCU | restarts the machine; also used for flashing |
| BOOT0 (SW2) | dedicated BOOT0 pin 94 (not a GPIO on H7) | no - sampled only at reset | hold during reset = USB-DFU bootloader |

So the three coin/RUN/PAUSE/STOP buttons of the original spec are folded into K1 gestures.

## Build & flash
```
make h750          # build/h750/washer.{elf,bin,hex}   (arm-none-eabi-gcc from PATH or PlatformIO)
make flash         # auto: ST-Link (SWD) if present, else USB-DFU
make flash-dfu     # USB-C: hold BOOT0, tap NRST, release BOOT0, then run (lsusb: 0483:df11)
make flash-swd     # ST-Link on SWDIO/SWCLK/GND/3V3 (OpenOCD)
make preview       # render the dashboard on the PC -> build/preview/*.ppm (no board needed)
make test_gesture  # host unit tests for the button gestures
```
Linux: install `99-weact-h750.rules` once to flash without sudo. If the image looks shifted/colour
inverted, rebuild with `make h750 LCD_PANEL=BOE` (the 0.96" module ships with two panel types).

Full Vietnamese user guide: [`HDSD_BOARD_WEACT_H750.md`](../../HDSD_BOARD_WEACT_H750.md).

## Using the single button
| Gesture | STANDBY | READY | RUNNING | PAUSED | ERROR |
|---|---|---|---|---|---|
| Click | +50c -> READY | RUN | PAUSE | RESUME | clear fault |
| Double-click | +10c | inject door-open fault | inject door-open fault | inject door-open fault | clear fault |
| Hold 0.6 s | arms STOP | arms STOP | arms STOP | arms STOP | - |
| Keep holding to 1.2 s | - | STANDBY | STANDBY | STANDBY | STANDBY |

The LCD footer shows a hold progress bar (tick = first STOP, full = second STOP, i.e. the spec's
double-press STOP inside the 1.5 s window).

## Clocks
HSE 25 MHz -> PLL1 -> 400 MHz CPU, 200 MHz AHB, 100 MHz APB (VOS1, LDO). If the crystal fails to
start, the HSI is used with an equivalent PLL setting so the firmware still boots.
