/**
 * @file lcd_test_main.c
 * @brief Stand-alone LCD bring-up test for the WeAct STM32H750 board (no washing machine logic)
 * @details - Fills the screen with red / green / blue / white / black, one colour per second,
 *            over a white 1-pixel border and corner markers (checks offsets and orientation).
 *          - Shows the current backlight mode "BLn" and the colour name.
 *          - K1 (PC13): next backlight mode (1 = DC high, 2 = DC low, 3 = PWM 10%, 4 = PWM 90%).
 *          - LED PE3 toggles every colour change, so a running firmware is always visible.
 */

#include <stdint.h>
#include "board_h750.h"
#include "gfx.h"

typedef struct {
    const char *name;
    uint16_t bg;
    uint16_t fg;
} test_color_t;

static const test_color_t k_colors[] = {
    {"RED",   GFX_RGB(255, 0, 0),     GFX_WHITE},
    {"GREEN", GFX_RGB(0, 255, 0),     GFX_BLACK},
    {"BLUE",  GFX_RGB(0, 0, 255),     GFX_WHITE},
    {"WHITE", GFX_WHITE,              GFX_BLACK},
    {"BLACK", GFX_BLACK,              GFX_WHITE}
};
#define COLOR_COUNT (sizeof(k_colors) / sizeof(k_colors[0]))

static void draw(const test_color_t *c, uint32_t bl_mode) {
    char bl[4] = {'B', 'L', (char)('0' + bl_mode), '\0'};

    gfx_fill(c->bg);
    gfx_frame(0, 0, GFX_WIDTH, GFX_HEIGHT, c->fg);
    gfx_rect(2, 2, 8, 8, GFX_RED);
    gfx_rect(GFX_WIDTH - 10, 2, 8, 8, GFX_GREEN);
    gfx_rect(2, GFX_HEIGHT - 10, 8, 8, GFX_BLUE);
    gfx_rect(GFX_WIDTH - 10, GFX_HEIGHT - 10, 8, 8, GFX_YELLOW);

    gfx_text(16, 14, "LCD TEST", 2, c->fg);
    gfx_text(16, 36, c->name, 2, c->fg);
    gfx_text(16, 58, bl, 2, c->fg);
    gfx_text(60, 62, "K1 = NEXT BL MODE", 1, c->fg);
}

int main(void) {
    uint32_t bl_mode = 4U;
    uint32_t idx = 0U;
    bool led = true;
    bool key_prev = false;

    board_init();
    board_led_set(true);
    lcd_init();   /* starts the PWM backlight (mode 4) before initialising the panel */

    uint32_t last_change = board_millis();
    draw(&k_colors[idx], bl_mode);
    lcd_flush();

    for (;;) {
        uint32_t now = board_millis();
        bool key = board_key_pressed();
        bool redraw = false;

        if (key && !key_prev) {
            bl_mode = (bl_mode % 4U) + 1U;
            lcd_backlight_mode(bl_mode);
            redraw = true;
            board_delay_ms(50U); /* crude debounce */
        }
        key_prev = key;

        if ((now - last_change) >= 1000U) {
            last_change = now;
            idx = (idx + 1U) % (uint32_t)COLOR_COUNT;
            led = !led;
            board_led_set(led);
            redraw = true;
        }

        if (redraw) {
            draw(&k_colors[idx], bl_mode);
            lcd_flush();
        }
    }
}
