# Makefile for Washing Machine Control Unit (CO3053 - BTL 2)

CC = gcc
CFLAGS ?= -Wall -Wextra -Werror -pedantic -std=c99 -O2 -I src/include -I src/hal

SRC = src/fsm/washing_machine_fsm.c src/hal/mock_hal.c
SRC_HAL = src/hal/hal_button_engine.c src/hal/hal_led_blinker.c
SRC_STM32 = src/hal/stm32/hal_stm32_gpio.c src/hal/stm32/hal_stm32_callbacks.c

ifeq ($(OS),Windows_NT)
    TARGET_TEST = test_runner.exe
    TARGET_TEST_HAL = test_hal_runner.exe
    TARGET_SIM = sim_wm.exe
    TARGET_DEMO = demo_wm.exe
    TARGET_STM32 = stm32_wm.exe
    RM = -cmd /c del /f /q $(TARGET_TEST) $(TARGET_TEST_HAL) $(TARGET_SIM) $(TARGET_DEMO) $(TARGET_STM32) *.o 2>nul
else
    TARGET_TEST = test_runner
    TARGET_TEST_HAL = test_hal_runner
    TARGET_SIM = sim_wm
    TARGET_DEMO = demo_wm
    TARGET_STM32 = stm32_wm
    RM = rm -f $(TARGET_TEST) $(TARGET_TEST_HAL) $(TARGET_SIM) $(TARGET_DEMO) $(TARGET_STM32) *.o
endif

.PHONY: all test test_hal sim demo stm32 clean

all: test test_hal sim demo stm32

test: $(SRC) tests/test_washing_machine.c
	$(CC) $(CFLAGS) $^ -o $(TARGET_TEST)
	./$(TARGET_TEST)

test_hal: $(SRC_HAL) tests/test_hal_engines.c
	$(CC) $(CFLAGS) $^ -o $(TARGET_TEST_HAL)
	./$(TARGET_TEST_HAL)

sim: $(SRC) sim/sim_interactive.c
	$(CC) $(CFLAGS) $^ -o $(TARGET_SIM)

demo: $(SRC) src/main.c
	$(CC) $(CFLAGS) $^ -o $(TARGET_DEMO)
	./$(TARGET_DEMO)

stm32: $(SRC) $(SRC_HAL) $(SRC_STM32) src/hal/stm32/main_stm32.c
	$(CC) $(CFLAGS) -I src/hal/stm32 $^ -o $(TARGET_STM32)
	./$(TARGET_STM32)

clean:
	$(RM)
