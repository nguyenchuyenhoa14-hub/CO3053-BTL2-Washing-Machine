# Makefile for Washing Machine Control Unit (CO3053 - BTL 2)

CC = gcc
CFLAGS ?= -Wall -Wextra -Werror -O2 -I src/include -I src/hal

SRC = src/fsm/washing_machine_fsm.c src/hal/mock_hal.c

ifeq ($(OS),Windows_NT)
    TARGET_TEST = test_runner.exe
    TARGET_SIM = sim_wm.exe
    RM = -cmd /c del /f /q $(TARGET_TEST) $(TARGET_SIM) *.o 2>nul
else
    TARGET_TEST = test_runner
    TARGET_SIM = sim_wm
    RM = rm -f $(TARGET_TEST) $(TARGET_SIM) *.o
endif

.PHONY: all test sim clean

all: test sim

test: $(SRC) tests/test_washing_machine.c
	$(CC) $(CFLAGS) $^ -o $(TARGET_TEST)
	./$(TARGET_TEST)

sim: $(SRC) sim/sim_interactive.c
	$(CC) $(CFLAGS) $^ -o $(TARGET_SIM)

clean:
	$(RM)

