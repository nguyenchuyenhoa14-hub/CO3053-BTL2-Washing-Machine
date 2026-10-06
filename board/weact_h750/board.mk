# Build / flash rules for the WeAct STM32H750VBT6 core board (included by the top-level Makefile)
#   make h750          build build/h750/washer.{elf,bin,hex}
#   make flash         flash over USB-DFU or ST-Link (auto-detected)
#   make flash-dfu     force USB-DFU      (BOOT0 + NRST, then dfu-util)
#   make flash-swd     force SWD          (ST-Link + OpenOCD)
#   make preview       render the LCD dashboard on the host to build/preview/*.ppm
#   make test_gesture  unit tests for the single-button gestures
#   Cycle length: demo = 30 s (default); make h750 FULL_CYCLE=1 for the real 30 min
#   Panel variant: make h750 LCD_PANEL=BOE   (default HannStar)

SRC_FSM = src/fsm/washing_machine_fsm.c
H750_DIR   = board/weact_h750
H750_OUT   = build/h750

PIO_ARM_BIN = $(HOME)/.platformio/packages/toolchain-gccarmnoneeabi/bin
ARM_PREFIX ?= $(if $(shell command -v arm-none-eabi-gcc 2>/dev/null),arm-none-eabi-,$(PIO_ARM_BIN)/arm-none-eabi-)
ARM_CC      = $(ARM_PREFIX)gcc
ARM_OBJCOPY = $(ARM_PREFIX)objcopy
ARM_SIZE    = $(ARM_PREFIX)size

H750_CPU    = -mcpu=cortex-m7 -mthumb -mfloat-abi=soft
H750_DEFS   = -DSTM32H750xx $(if $(filter BOE,$(LCD_PANEL)),-DLCD_PANEL_BOE) $(if $(BL_TEST),-DBL_EXPERIMENT) $(if $(LCD_BL),-DLCD_BL_MODE=$(LCD_BL)) $(if $(FULL_CYCLE),-DWM_DEMO_CYCLE_SEC=1800U)
H750_CFLAGS = $(H750_CPU) $(H750_DEFS) -std=c99 -Os -g3 -Wall -Wextra -Werror -pedantic \
              -ffunction-sections -fdata-sections -fno-common \
              -isystem $(H750_DIR)/cmsis -I src/include -I src/hal -I src/app -I $(H750_DIR)
H750_LDFLAGS = $(H750_CPU) -T $(H750_DIR)/stm32h750vb_flash.ld -nostartfiles \
               --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=$(H750_OUT)/washer.map

H750_SRC = $(SRC_FSM) src/hal/hal_button_engine.c src/hal/hal_led_blinker.c \
           src/hal/hal_button_gesture.c src/app/wm_single_button.c src/app/wm_two_button.c \
           $(H750_DIR)/startup_stm32h750.c $(H750_DIR)/board_h750.c $(H750_DIR)/lcd_st7735.c \
           $(H750_DIR)/gfx.c $(H750_DIR)/ui_render.c $(H750_DIR)/ui_text.c $(H750_DIR)/main_h750.c


.PHONY: h750 flash flash-dfu flash-swd preview test_gesture test_ui_text test_two_button lcd_test flash-lcdtest

h750: $(H750_OUT)/washer.bin
	@$(ARM_SIZE) $(H750_OUT)/washer.elf

$(H750_OUT)/washer.elf: $(H750_SRC) $(wildcard $(H750_DIR)/*.h) $(H750_DIR)/stm32h750vb_flash.ld
	@mkdir -p $(H750_OUT)
	$(ARM_CC) $(H750_CFLAGS) $(H750_SRC) $(H750_LDFLAGS) -o $@

$(H750_OUT)/washer.bin: $(H750_OUT)/washer.elf
	$(ARM_OBJCOPY) -O binary $< $@
	$(ARM_OBJCOPY) -O ihex $< $(H750_OUT)/washer.hex

flash: h750
	$(H750_DIR)/flash.sh $(H750_OUT)/washer.bin $(H750_OUT)/washer.elf auto

flash-dfu: h750
	$(H750_DIR)/flash.sh $(H750_OUT)/washer.bin $(H750_OUT)/washer.elf dfu

flash-swd: h750
	$(H750_DIR)/flash.sh $(H750_OUT)/washer.bin $(H750_OUT)/washer.elf swd

preview: $(SRC_FSM) src/hal/hal_button_gesture.c src/app/wm_single_button.c $(H750_DIR)/gfx.c $(H750_DIR)/ui_render.c $(H750_DIR)/preview_host.c
	@mkdir -p build/preview
	$(CC) -Wall -Wextra -Werror -pedantic -std=c99 -O2 -I src/include -I src/hal -I src/app -I $(H750_DIR) $^ -o build/preview/preview_wm
	build/preview/preview_wm build/preview/frame
	@echo "Frames written to build/preview/*.ppm"

test_gesture: $(SRC_FSM) src/hal/hal_button_gesture.c src/app/wm_single_button.c tests/test_gesture.c
	$(CC) $(CFLAGS) -I src/app $^ -o test_gesture_runner
	./test_gesture_runner

test_two_button: $(SRC_FSM) src/hal/hal_button_gesture.c src/app/wm_single_button.c src/app/wm_two_button.c tests/test_two_button.c
	$(CC) $(CFLAGS) -I src/app $^ -o test_two_button_runner
	./test_two_button_runner

test_ui_text: $(SRC_FSM) $(H750_DIR)/ui_render.c $(H750_DIR)/ui_text.c $(H750_DIR)/gfx.c tests/test_ui_text.c
	$(CC) $(CFLAGS) -I src/app -I $(H750_DIR) $^ -o test_ui_text_runner
	./test_ui_text_runner

# Stand-alone LCD test firmware (no washing machine): make flash-lcdtest
LCDTEST_OUT = build/lcdtest
LCDTEST_SRC = $(H750_DIR)/startup_stm32h750.c $(H750_DIR)/board_h750.c $(H750_DIR)/lcd_st7735.c \
              $(H750_DIR)/gfx.c $(H750_DIR)/lcd_test_main.c

lcd_test: $(LCDTEST_OUT)/lcdtest.bin
	@$(ARM_SIZE) $(LCDTEST_OUT)/lcdtest.elf

$(LCDTEST_OUT)/lcdtest.elf: $(LCDTEST_SRC) $(wildcard $(H750_DIR)/*.h) $(H750_DIR)/stm32h750vb_flash.ld
	@mkdir -p $(LCDTEST_OUT)
	$(ARM_CC) $(H750_CFLAGS) $(LCDTEST_SRC) $(subst $(H750_OUT)/washer.map,$(LCDTEST_OUT)/lcdtest.map,$(H750_LDFLAGS)) -o $@

$(LCDTEST_OUT)/lcdtest.bin: $(LCDTEST_OUT)/lcdtest.elf
	$(ARM_OBJCOPY) -O binary $< $@

flash-lcdtest: lcd_test
	$(H750_DIR)/flash.sh $(LCDTEST_OUT)/lcdtest.bin $(LCDTEST_OUT)/lcdtest.elf auto
