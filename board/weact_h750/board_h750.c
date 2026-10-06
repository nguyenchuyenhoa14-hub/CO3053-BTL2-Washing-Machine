/**
 * @file board_h750.c
 * @brief Clock tree, GPIO and SysTick for the WeAct STM32H750VBT6 board
 * @details SYSCLK 400 MHz (VOS1, LDO supply) from the 25 MHz HSE crystal. If the crystal does not
 *          start, falls back to HSI 64 MHz with a PLL setting that still yields 400 MHz, so the
 *          firmware never hangs at boot and all timing constants stay valid.
 */

#include "stm32h7xx.h"
#include "board_h750.h"

#define SYSCLK_HZ        (400000000U)
#define HSE_TIMEOUT_LOOP (2000000U)

uint32_t SystemCoreClock = SYSCLK_HZ;

static volatile uint32_t g_millis = 0U;
static const char *g_reset_cause = "?";

const char *board_reset_cause(void) {
    return g_reset_cause;
}

void SysTick_Handler(void) {
    g_millis++;
}

uint32_t board_millis(void) {
    return g_millis;
}

/* Wall-clock wait independent of SysTick/interrupts: DWT cycle counter at SYSCLK_HZ */
static void dwt_wait_ms(uint32_t ms) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = ms * (SYSCLK_HZ / 1000U);
    while ((DWT->CYCCNT - start) < cycles) {
    }
}

void board_fatal(uint32_t code) {
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
    (void)RCC->AHB4ENR;
    GPIOE->MODER = (GPIOE->MODER & ~(3U << (3U * 2U))) | (1U << (3U * 2U));
    for (;;) {
        for (uint32_t k = 0U; k < code; k++) {
            GPIOE->BSRR = (1U << 3U);
            dwt_wait_ms(500U);
            GPIOE->BSRR = (1U << (3U + 16U));
            dwt_wait_ms(500U);
        }
        dwt_wait_ms(3000U);
    }
}

void HardFault_Handler(void) {
    board_fatal(BOARD_ERR_HARDFAULT);
}

void board_delay_ms(uint32_t ms) {
    uint32_t start = g_millis;
    uint32_t guard = ms * 4000000U; /* >> loop speed of a 400 MHz M7; only trips if SysTick is dead */
    while ((g_millis - start) < ms) {
        if (guard-- == 0U) {
            board_fatal(BOARD_ERR_SYSTICK);
        }
    }
}

static bool start_hse(void) {
    RCC->CR |= RCC_CR_HSEON;
    for (uint32_t i = 0U; i < HSE_TIMEOUT_LOOP; i++) {
        if ((RCC->CR & RCC_CR_HSERDY) != 0U) {
            return true;
        }
    }
    RCC->CR &= ~RCC_CR_HSEON;
    return false;
}

void board_system_init(void) {
    /* 0. Start from a known clock state. After a USB-DFU "leave" the ROM bootloader jumps here
     *    without a clean reset (PLL1 may already be running, which makes PLL writes ineffective). */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0U) {
    }
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0U) {
    }
    RCC->CR &= ~RCC_CR_PLL1ON;
    while ((RCC->CR & RCC_CR_PLL1RDY) != 0U) {
    }
    __disable_irq();
    SCB_DisableDCache();
    SCB_DisableICache();

    /* 1. Supply: LDO, then voltage scale 1 (required for 400 MHz) */
    PWR->CR3 = (PWR->CR3 | PWR_CR3_LDOEN) & ~PWR_CR3_BYPASS;
    while ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) == 0U) {
    }
    PWR->D3CR |= PWR_D3CR_VOS;
    while ((PWR->D3CR & PWR_D3CR_VOSRDY) == 0U) {
    }

    /* 2. PLL1: HSE 25/5 = 5 MHz x160 = 800 MHz VCO /2 = 400 MHz (HSI: 64/16 = 4 MHz x200) */
    bool hse_ok = start_hse();
    uint32_t divm = hse_ok ? 5U : 16U;
    uint32_t divn = hse_ok ? 160U : 200U;
    uint32_t src  = hse_ok ? 2U : 0U;
    RCC->PLLCKSELR = (divm << 4) | src;
    RCC->PLLCFGR = (2U << 2) |                 /* PLL1RGE: 4..8 MHz input */
                   RCC_PLLCFGR_DIVP1EN;        /* wide VCO, no fractional */
    RCC->PLL1DIVR = (divn - 1U) | ((2U - 1U) << 9) | ((8U - 1U) << 16) | ((2U - 1U) << 24);
    RCC->CR |= RCC_CR_PLL1ON;
    while ((RCC->CR & RCC_CR_PLL1RDY) == 0U) {
    }

    /* 3. Bus prescalers: CPU 400, AHB/AXI 200, all APB 100 MHz */
    RCC->D1CFGR = (8U << 0) | (4U << 4);       /* HPRE /2, D1PPRE /2, D1CPRE /1 */
    RCC->D2CFGR = (4U << 4) | (4U << 8);       /* D2PPRE1 /2, D2PPRE2 /2 */
    RCC->D3CFGR = (4U << 4);                   /* D3PPRE /2 */

    /* 4. Flash wait states for 200 MHz AXI @ VOS1, then switch to PLL1 */
    FLASH->ACR = (2U << FLASH_ACR_LATENCY_Pos) | (2U << FLASH_ACR_WRHIGHFREQ_Pos);
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != (2U << FLASH_ACR_LATENCY_Pos)) {
    }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (3U << RCC_CFGR_SW_Pos);
    while ((RCC->CFGR & RCC_CFGR_SWS) != (3U << RCC_CFGR_SWS_Pos)) {
    }

    SCB->VTOR = FLASH_BANK1_BASE;
    SCB_EnableICache();
    SCB_EnableDCache();
    __enable_irq();
}

void board_init(void) {
    {
        uint32_t rsr = RCC->RSR;
        if ((rsr & RCC_RSR_PORRSTF) != 0U)        { g_reset_cause = "POR"; }
        else if ((rsr & RCC_RSR_BORRSTF) != 0U)   { g_reset_cause = "BOR"; }
        else if ((rsr & RCC_RSR_IWDG1RSTF) != 0U) { g_reset_cause = "IWDG"; }
        else if ((rsr & RCC_RSR_WWDG1RSTF) != 0U) { g_reset_cause = "WWDG"; }
        else if ((rsr & RCC_RSR_SFTRSTF) != 0U)   { g_reset_cause = "SFT"; }
        else if ((rsr & RCC_RSR_PINRSTF) != 0U)   { g_reset_cause = "PIN"; }
        RCC->RSR |= RCC_RSR_RMVF;
    }

    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIOEEN;
    (void)RCC->AHB4ENR; /* read-back: guarantee clock is running before first access */

    /* LED PE3: push-pull output, off */
    GPIOE->BSRR = (1U << (3U + 16U));
    GPIOE->MODER = (GPIOE->MODER & ~(3U << (3U * 2U))) | (1U << (3U * 2U));

    /* KEY PC13: input with pull-down (K1 pulls the pin to 3.3 V when pressed) */
    GPIOC->MODER &= ~(3U << (13U * 2U));
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3U << (13U * 2U))) | (2U << (13U * 2U));

    /* External buttons PA0 / PA1: inputs with pull-up (pressed = LOW) */
    GPIOA->MODER &= ~((3U << (0U * 2U)) | (3U << (1U * 2U)));
    GPIOA->PUPDR = (GPIOA->PUPDR & ~((3U << (0U * 2U)) | (3U << (1U * 2U)))) |
                   (1U << (0U * 2U)) | (1U << (1U * 2U));

    (void)SysTick_Config(SYSCLK_HZ / 1000U);
}

void board_led_set(bool on) {
    GPIOE->BSRR = on ? (1U << 3U) : (1U << (3U + 16U));
}

bool board_key_pressed(void) {
    return (GPIOC->IDR & (1U << 13U)) != 0U;
}

bool board_ext_run_pressed(void) {
    return (GPIOA->IDR & (1U << 0U)) == 0U;
}

bool board_ext_stop_pressed(void) {
    return (GPIOA->IDR & (1U << 1U)) == 0U;
}

#define UART_BAUD (115200U)
#define UART_PCLK (100000000U) /* USART1 kernel clock = APB2 (default selection) */

void board_uart_init(void) {
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    /* PA9 = TX, PA10 = RX: alternate function 7, internal pull-up on RX */
    GPIOA->MODER = (GPIOA->MODER & ~((3U << (9U * 2U)) | (3U << (10U * 2U)))) |
                   (2U << (9U * 2U)) | (2U << (10U * 2U));
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((0xFU << 4) | (0xFU << 8))) | (7U << 4) | (7U << 8);
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3U << (10U * 2U))) | (1U << (10U * 2U));
    GPIOA->OSPEEDR |= (3U << (9U * 2U));

    USART1->CR1 = 0U;
    USART1->BRR = UART_PCLK / UART_BAUD;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void uart_putc(char c) {
    uint32_t guard = 2000000U;
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) {
        if (guard-- == 0U) {
            return; /* never block the machine on a missing terminal */
        }
    }
    USART1->TDR = (uint32_t)(uint8_t)c;
}

void board_uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

int board_uart_getc(void) {
    if ((USART1->ISR & USART_ISR_ORE) != 0U) {
        USART1->ICR = USART_ICR_ORECF;
    }
    if ((USART1->ISR & USART_ISR_RXNE_RXFNE) != 0U) {
        return (int)(USART1->RDR & 0xFFU);
    }
    return -1;
}
