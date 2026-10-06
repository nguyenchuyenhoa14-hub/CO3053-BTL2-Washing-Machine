/**
 * @file gfx.h
 * @brief Tiny RGB565 framebuffer renderer (160x80) with a built-in 5x7 font
 * @details Hardware independent: rendered on the host for tests and flushed to the
 *          ST7735 panel on the board.
 */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_WIDTH   (160)
#define GFX_HEIGHT  (80)

#define GFX_RGB(r, g, b) ((uint16_t)((((uint16_t)(r) & 0xF8U) << 8) | \
                                     (((uint16_t)(g) & 0xFCU) << 3) | \
                                     (((uint16_t)(b) & 0xF8U) >> 3)))

#define GFX_BLACK   GFX_RGB(0, 0, 0)
#define GFX_WHITE   GFX_RGB(255, 255, 255)
#define GFX_GRAY    GFX_RGB(110, 110, 120)
#define GFX_DARK    GFX_RGB(40, 40, 48)
#define GFX_RED     GFX_RGB(255, 60, 60)
#define GFX_GREEN   GFX_RGB(60, 220, 100)
#define GFX_BLUE    GFX_RGB(70, 140, 255)
#define GFX_YELLOW  GFX_RGB(255, 210, 60)
#define GFX_ORANGE  GFX_RGB(255, 140, 40)

extern uint16_t g_gfx_fb[GFX_WIDTH * GFX_HEIGHT];

void gfx_fill(uint16_t color);
void gfx_rect(int x, int y, int w, int h, uint16_t color);
void gfx_frame(int x, int y, int w, int h, uint16_t color);
void gfx_disc(int cx, int cy, int r, uint16_t color);

/** @brief Draw text (A-Z, 0-9, common punctuation; lowercase is upper-cased). Returns end x. */
int gfx_text(int x, int y, const char *s, int scale, uint16_t color);

/** @brief Pixel width of a string at the given scale */
int gfx_text_width(const char *s, int scale);

#ifdef __cplusplus
}
#endif

#endif /* GFX_H */
