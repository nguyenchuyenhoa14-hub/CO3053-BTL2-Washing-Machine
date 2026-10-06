/**
 * @file board_h750.h
 * @brief WeAct Studio STM32H750VBT6 core board: pins, clocks and low-level drivers
 * @details Onboard resources used (from WeAct MiniSTM32H7xx schematic / SDK):
 *            LED   PE3  (active HIGH)
 *            KEY   PC13 (K1, active HIGH, internal pull-down)
 *            EXT   PA0 = RUN/PAUSE, PA1 = STOP (optional, to GND, internal pull-up)
 *            LCD   0.96" ST7735 160x80 on SPI4: SCK PE12, MOSI PE14, CS PE11, DC PE13,
 *                  BL PE10 (HIGH = on), RST wired to NRST
 *            HSE   25 MHz
 */

#ifndef BOARD_H750_H
#define BOARD_H750_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Called from Reset_Handler: power, PLL (400 MHz), caches, vector table */
void board_system_init(void);

/** @brief GPIO + SysTick (1 kHz) setup; call once from main */
void board_init(void);

void board_led_set(bool on);
bool board_key_pressed(void);

/** @brief Optional external push-buttons wired between the pin and GND (internal pull-up) */
bool board_ext_run_pressed(void);   /**< PA0 */
bool board_ext_stop_pressed(void);  /**< PA1 */

/** @brief Cause of the last reset ("POR","BOR","IWDG","WWDG","SFT","PIN"), captured at boot */
const char *board_reset_cause(void);

/** @brief USART1 on PA9 (TX) / PA10 (RX), 115200 8N1, polled. Connect a USB-TTL adapter (3.3 V, common GND). */
void board_uart_init(void);
void board_uart_puts(const char *s);

/** @brief Non-blocking read; returns -1 when no byte is available */
int board_uart_getc(void);

/** @brief Millisecond counter incremented by SysTick */
uint32_t board_millis(void);

/** Fatal error codes shown as N LED blinks repeated forever (see HDSD section 7) */
#define BOARD_ERR_SPI_TXP     (2U)   /**< SPI4 never became ready for data */
#define BOARD_ERR_SPI_EOT     (3U)   /**< SPI4 transfer never completed */
#define BOARD_ERR_SYSTICK     (4U)   /**< SysTick interrupt not running */
#define BOARD_ERR_HARDFAULT   (5U)   /**< CPU fault */

/** @brief Never returns: blink the LED `code` times, pause, repeat (works without SysTick) */
void board_fatal(uint32_t code);

/** @brief Busy-wait helper used only during LCD bring-up */
void board_delay_ms(uint32_t ms);

/** @brief Initialize the ST7735 panel (landscape, 160x80) and turn the backlight on */
void lcd_init(void);

/**
 * @brief Backlight drive on PE10: 1 = DC high, 2 = DC low, 3 = TIM1 PWM 10% high,
 *        4 = TIM1 PWM 90% high (default: the only mode that stays lit on the WeAct board)
 */
void lcd_backlight_mode(uint32_t mode);

/** @brief Push g_gfx_fb to the panel */
void lcd_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H750_H */
