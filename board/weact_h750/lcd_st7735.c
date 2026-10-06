/**
 * @file lcd_st7735.c
 * @brief 0.96" ST7735 (160x80, landscape) driver over SPI4, transmit-only, register level
 * @details Panel variant: WeAct ships HannStar (default) and BOE panels. If colours look inverted or
 *          the picture is shifted by a few pixels, build with -DLCD_PANEL_BOE.
 */

#include "stm32h7xx.h"
#include "board_h750.h"
#include "gfx.h"

#ifdef LCD_PANEL_BOE
#define LCD_X_OFFSET  (0U)
#define LCD_Y_OFFSET  (24U)
#define LCD_INVERT    (false)
#define LCD_MADCTL    (0xA0U)              /* landscape rot180, RGB */
#else
#define LCD_X_OFFSET  (1U)
#define LCD_Y_OFFSET  (26U)
#define LCD_INVERT    (true)
#define LCD_MADCTL    (0xA8U)              /* landscape rot180, BGR */
#endif

/* Backlight drive (see lcd_backlight_mode() in board_h750.h). The WeAct reference firmware drives
 * PE10 with TIM1_CH2N PWM, started BEFORE the panel is initialised; plain DC on PE10 faded out.
 * 4 = PWM, pin HIGH 90% (matches the reference), 3 = PWM 10%, 1 = DC high, 2 = DC low. */
#ifndef LCD_BL_MODE
#define LCD_BL_MODE   (4U)
#endif

/* PE10 BL, PE11 CS, PE13 DC; PE12 SCK / PE14 MOSI = AF5 (SPI4) */
#define PIN_BL (10U)
#define PIN_CS (11U)
#define PIN_DC (13U)

#define SPI_SR_TXP_BIT  (1U << 1)
#define SPI_SR_EOT_BIT  (1U << 3)
#define SPI_CR1_SPE_BIT (1U << 0)
#define SPI_CR1_CSTART_BIT (1U << 9)
#define SPI_CR1_SSI_BIT (1U << 12)

void lcd_backlight_mode(uint32_t mode);

static void pin_set(uint32_t pin, bool high) {
    GPIOE->BSRR = high ? (1U << pin) : (1U << (pin + 16U));
}

static void gpio_mode(uint32_t pin, uint32_t mode) {
    GPIOE->MODER = (GPIOE->MODER & ~(3U << (pin * 2U))) | (mode << (pin * 2U));
    GPIOE->OSPEEDR |= (3U << (pin * 2U));
}

static void spi_begin(uint32_t nbytes) {
    SPI4->CR1 &= ~SPI_CR1_SPE_BIT;
    SPI4->CR2 = nbytes;
    SPI4->CR1 |= SPI_CR1_SPE_BIT;
    SPI4->CR1 |= SPI_CR1_CSTART_BIT;
}

#define SPI_WAIT_LOOPS (4000000U)

static void spi_put(uint8_t b) {
    uint32_t guard = SPI_WAIT_LOOPS;
    while ((SPI4->SR & SPI_SR_TXP_BIT) == 0U) {
        if (guard-- == 0U) {
            board_fatal(BOARD_ERR_SPI_TXP);
        }
    }
    *(volatile uint8_t *)&SPI4->TXDR = b;
}

static void spi_end(void) {
    uint32_t guard = SPI_WAIT_LOOPS;
    while ((SPI4->SR & SPI_SR_EOT_BIT) == 0U) {
        if (guard-- == 0U) {
            board_fatal(BOARD_ERR_SPI_EOT);
        }
    }
    SPI4->IFCR = SPI_SR_EOT_BIT | (1U << 4); /* EOTC | TXTFC */
    SPI4->CR1 &= ~SPI_CR1_SPE_BIT;
}

static void lcd_cmd(uint8_t cmd, const uint8_t *args, uint32_t n) {
    pin_set(PIN_CS, false);
    pin_set(PIN_DC, false);
    spi_begin(1U);
    spi_put(cmd);
    spi_end();
    if (n > 0U) {
        pin_set(PIN_DC, true);
        spi_begin(n);
        for (uint32_t i = 0U; i < n; i++) {
            spi_put(args[i]);
        }
        spi_end();
    }
    pin_set(PIN_CS, true);
}

#define CMD0(c) lcd_cmd((c), 0, 0U)
#define CMD(c, ...) do { static const uint8_t a_[] = {__VA_ARGS__}; \
    lcd_cmd((c), a_, (uint32_t)sizeof(a_)); } while (0)

void lcd_init(void) {
    /* Pins: CS/DC/BL outputs (CS idle high, backlight off until first frame is ready) */
    pin_set(PIN_CS, true);
    pin_set(PIN_DC, true);
    pin_set(PIN_BL, false);
    gpio_mode(PIN_BL, 1U);
    gpio_mode(PIN_CS, 1U);
    gpio_mode(PIN_DC, 1U);
    gpio_mode(12U, 2U);
    gpio_mode(14U, 2U);
    GPIOE->AFR[1] = (GPIOE->AFR[1] & ~((0xFU << 16) | (0xFU << 24))) | (5U << 16) | (5U << 24);

#ifndef BL_EXPERIMENT
    lcd_backlight_mode(LCD_BL_MODE);   /* vendor order: backlight PWM first, then panel init */
#endif

    /* SPI4: kernel clock = APB2 (100 MHz) / 8 = 12.5 MHz, mode 0, 8-bit, master, TX only, soft NSS */
    RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;
    (void)RCC->APB2ENR;
    RCC->D2CCIP1R &= ~(7U << 16);
    SPI4->CR1 = SPI_CR1_SSI_BIT;
    SPI4->CFG1 = (2U << 28) | 7U;
    SPI4->CFG2 = (1U << 31) | (1U << 26) | (1U << 22) | (1U << 17);

    CMD0(0x01);                                   /* software reset (panel RST is tied to NRST) */
    board_delay_ms(120U);
    CMD0(0x11);                                   /* sleep out */
    board_delay_ms(120U);
    CMD(0xB1, 0x01, 0x2C, 0x2D);
    CMD(0xB2, 0x01, 0x2C, 0x2D);
    CMD(0xB3, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D);
    CMD(0xB4, 0x07);
    CMD(0xC0, 0xA2, 0x02, 0x84);
    CMD(0xC1, 0xC5);
    CMD(0xC2, 0x0A, 0x00);
    CMD(0xC3, 0x8A, 0x2A);
    CMD(0xC4, 0x8A, 0xEE);
    CMD(0xC5, 0x0E);
    CMD0(LCD_INVERT ? 0x21U : 0x20U);
    CMD(0x3A, 0x05);                             /* RGB565 */
    CMD(0xE0, 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10);
    CMD(0xE1, 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10);
    CMD0(0x13);                                   /* normal display mode */
    CMD(0x36, LCD_MADCTL);
    CMD0(0x29);                                   /* display on */
    board_delay_ms(20U);

    gfx_fill(GFX_BLACK);
    lcd_flush();
#ifdef BL_EXPERIMENT
    pin_set(PIN_BL, true);      /* legacy ending: exactly what the first BL_TEST build did */
#endif
}

void lcd_flush(void) {
    const uint32_t x0 = LCD_X_OFFSET;
    const uint32_t x1 = LCD_X_OFFSET + GFX_WIDTH - 1U;
    const uint32_t y0 = LCD_Y_OFFSET;
    const uint32_t y1 = LCD_Y_OFFSET + GFX_HEIGHT - 1U;

    uint8_t col[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t row[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_cmd(0x2A, col, 4U);
    lcd_cmd(0x2B, row, 4U);

    pin_set(PIN_CS, false);
    pin_set(PIN_DC, false);
    spi_begin(1U);
    spi_put(0x2CU);
    spi_end();
    pin_set(PIN_DC, true);
    spi_begin((uint32_t)GFX_WIDTH * (uint32_t)GFX_HEIGHT * 2U);
    for (uint32_t i = 0U; i < ((uint32_t)GFX_WIDTH * (uint32_t)GFX_HEIGHT); i++) {
        spi_put((uint8_t)(g_gfx_fb[i] >> 8));
        spi_put((uint8_t)g_gfx_fb[i]);
    }
    spi_end();
    pin_set(PIN_CS, true);
}

void lcd_backlight_mode(uint32_t mode) {
    /* TIM1 clock = 2 x APB2 = 200 MHz; /20 /1000 = 10 kHz PWM like the WeAct reference */
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->CCER = 0U;
    if (mode == 1U || mode == 2U) {
        gpio_mode(PIN_BL, 1U);
        pin_set(PIN_BL, mode == 1U);
        return;
    }
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;
    GPIOE->AFR[1] = (GPIOE->AFR[1] & ~(0xFU << 8)) | (1U << 8);   /* PE10 = AF1 (TIM1_CH2N) */
    gpio_mode(PIN_BL, 2U);
    TIM1->PSC = 19U;
    TIM1->ARR = 999U;
    TIM1->CCR2 = 100U;
    TIM1->CCMR1 = (6U << 12) | (1U << 11);                          /* OC2M = PWM1, preload */
    TIM1->CCER = TIM_CCER_CC2NE | ((mode == 4U) ? TIM_CCER_CC2NP : 0U);
    TIM1->BDTR |= TIM_BDTR_MOE;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->CR1 |= TIM_CR1_CEN;
}
