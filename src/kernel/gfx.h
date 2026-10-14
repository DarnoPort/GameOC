#ifndef GFX_H
#define GFX_H

#include "types.h"

#define GFX_WIDTH       320
#define GFX_HEIGHT      200
#define GFX_ROW_BYTES   (GFX_WIDTH / 4)             // 80 байт на строку
#define GFX_BUFFER_SIZE (GFX_ROW_BYTES * GFX_HEIGHT) // 16 000 байт всего буфера

/* 2-битные цветовые константы (4 оттенка) */
#define COLOR_BLACK      0  // 0b00 (RGB: 0, 0, 0)
#define COLOR_DARK_GRAY  1  // 0b01 (RGB: 85, 85, 85)
#define COLOR_LIGHT_GRAY 2  // 0b10 (RGB: 170, 170, 170)
#define COLOR_WHITE      3  // 0b11 (RGB: 255, 255, 255)

/* Прозрачный фон при выводе текста */
#define COLOR_TRANSPARENT -1

/* Управление видеорежимом */
void gfx_init(void);
void gfx_clear(uint8_t color);
void gfx_flip(void);
uint8_t *gfx_get_backbuffer(void);

/* Примитивы рисования */
void gfx_put_pixel(int x, int y, uint8_t color);
uint8_t gfx_get_pixel(int x, int y);
void gfx_draw_line(int x0, int y0, int x1, int y1, uint8_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint8_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint8_t color);
void gfx_draw_circle(int xc, int yc, int r, uint8_t color);
void gfx_fill_circle(int xc, int yc, int r, uint8_t color);

/* Вывод растрового текста 8x8 */
void gfx_draw_char(int x, int y, char c, uint8_t fg, int bg);
void gfx_draw_text(int x, int y, const char *str, uint8_t fg, int bg);
void gfx_draw_utf8(int x, int y, const char *str, uint8_t fg, int bg);

#endif