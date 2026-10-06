#!/usr/bin/env bash
# Flash firmware to the WeAct STM32H750 board. Auto-selects the transport:
#   * ST-Link / CMSIS-DAP on SWD  -> OpenOCD
#   * USB-C in ROM bootloader     -> dfu-util   (hold BOOT0, tap NRST, release BOOT0)
# Usage: flash.sh <firmware.bin> <firmware.elf> [dfu|swd]
set -euo pipefail

BIN="${1:?firmware.bin missing}"
ELF="${2:?firmware.elf missing}"
MODE="${3:-auto}"

PIO_PKG="${HOME}/.platformio/packages"
OPENOCD="$(command -v openocd || true)"
[ -z "$OPENOCD" ] && [ -x "$PIO_PKG/tool-openocd/bin/openocd" ] && OPENOCD="$PIO_PKG/tool-openocd/bin/openocd"
DFU_UTIL="$(command -v dfu-util || true)"
[ -z "$DFU_UTIL" ] && [ -x "$PIO_PKG/tool-dfuutil/bin/dfu-util" ] && DFU_UTIL="$PIO_PKG/tool-dfuutil/bin/dfu-util"

USB="$(lsusb 2>/dev/null || true)"
has_dfu() { grep -qi '0483:df11' <<<"$USB"; }
has_swd() { grep -qiE '0483:(3748|374[b-f]|3752|3753)|0d28:0204' <<<"$USB"; }

if [ "$MODE" = auto ]; then
    if has_swd; then MODE=swd
    elif has_dfu; then MODE=dfu
    else
        cat >&2 <<MSG
No board found. Either:
  * USB DFU : hold BOOT0, press+release NRST (or plug USB-C while holding BOOT0), release BOOT0,
              then run 'make flash' again (lsusb must show 0483:df11).
  * SWD     : connect an ST-Link to SWDIO/SWCLK/GND/3V3 and run 'make flash' again.
MSG
        exit 1
    fi
fi

case "$MODE" in
    swd)
        [ -n "$OPENOCD" ] || { echo "openocd not found (install it or: pio pkg install -g --tool platformio/tool-openocd)" >&2; exit 1; }
        SCRIPTS="$(dirname "$OPENOCD")/../openocd/scripts"
        [ -d "$SCRIPTS" ] || SCRIPTS="$(dirname "$OPENOCD")/../share/openocd/scripts"
        "$OPENOCD" -s "$SCRIPTS" -f interface/stlink.cfg -f target/stm32h7x.cfg \
            -c "program $ELF verify reset exit"
        ;;
    dfu)
        [ -n "$DFU_UTIL" ] || { echo "dfu-util not found (sudo apt install dfu-util, or: pio pkg install -g --tool platformio/tool-dfuutil)" >&2; exit 1; }
        # After ':leave' the chip resets and USB disappears, so dfu-util's final get_status always
        # fails (exit 74). Judge success by the download/verify message instead of the exit code.
        LOG="$(mktemp)"
        "$DFU_UTIL" -a 0 -d 0483:df11 -s 0x08000000:leave -D "$BIN" 2>&1 | tee "$LOG" || true
        if ! grep -q "File downloaded successfully" "$LOG"; then
            rm -f "$LOG"; echo "Flash FAILED (see output above)." >&2; exit 1
        fi
        rm -f "$LOG"
        ;;
    *)
        echo "unknown mode '$MODE' (use dfu or swd)" >&2; exit 1
        ;;
esac
echo "Flash OK."
