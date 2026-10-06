/**
 * @file startup_stm32h750.c
 * @brief Cortex-M7 vector table and reset handler (no vendor startup / libc required)
 */

#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

extern int main(void);
extern void board_system_init(void);

void Reset_Handler(void);
void Default_Handler(void);

void NMI_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)      __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)     __attribute__((weak, alias("Default_Handler")));

#define VEC(fn) ((uintptr_t)(fn))

__attribute__((section(".isr_vector"), used))
const uintptr_t g_vector_table[16] = {
    (uintptr_t)(&_estack),
    VEC(Reset_Handler),
    VEC(NMI_Handler),
    VEC(HardFault_Handler),
    VEC(MemManage_Handler),
    VEC(BusFault_Handler),
    VEC(UsageFault_Handler),
    0U, 0U, 0U, 0U,
    VEC(SVC_Handler),
    VEC(DebugMon_Handler),
    0U,
    VEC(PendSV_Handler),
    VEC(SysTick_Handler)
};

void Reset_Handler(void) {
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; ) {
        *dst++ = *src++;
    }
    for (uint32_t *dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0U;
    }
    board_system_init();
    (void)main();
    for (;;) {
    }
}

void Default_Handler(void) {
    for (;;) {
    }
}
