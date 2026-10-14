#include "gfx.h"
#include "io.h"
#include "font8x8.h"
#include "font8x8_cyrillic.h"

/* Внутренний двухбитный кадровый буфер ОС (16 000 байт) */
static uint8_t gfx_backbuffer[GFX_BUFFER_SIZE];

/* Аппаратная видеопамять VGA Mode 13h */
static volatile uint8_t *const vga_mem = (volatile uint8_t *)0xA0000;

static void vga_set_mode_13h(void) {
    /* 1. Регистр Misc Output */
    outb(0x3C2, 0x63);

    /* 2. Регистры Sequencer (синхронизатор) */
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x0E); // Chain-4 режим

    /* 3. Снятие защиты от записи CRTC */
    outb(0x3D4, 0x11);
    uint8_t crtc11 = inb(0x3D5);
    outb(0x3D5, crtc11 & 0x7F);

    /* 4. Таблица CRTC регистров для режима 320x200 */
    const uint8_t crtc_regs[25] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
        0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9C, 0x8E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
        0xFF
    };
    for (uint8_t i = 0; i < 25; i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc_regs[i]);
    }

    /* 5. Graphics Controller регистры */
    const uint8_t gc_regs[9] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF
    };
    for (uint8_t i = 0; i < 9; i++) {
        outb(0x3CE, i);
        outb(0x3CF, gc_regs[i]);
    }

    /* 6. Attribute Controller регистры */
    const uint8_t ac_regs[21] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x41, 0x00, 0x0F, 0x00, 0x00
    };
    for (uint8_t i = 0; i < 21; i++) {
        inb(0x3DA); // Сброс триггера индекса AC
        outb(0x3C0, i);
        outb(0x3C0, ac_regs[i]);
    }

    /* 7. Включение видеовыхода */
    inb(0x3DA);
    outb(0x3C0, 0x20);
}

static void gfx_init_palette(void) {
    /* VGA DAC принимает значения 0..63 для каждого RGB канала */

    /* Индекс 0: Чёрный (0, 0, 0) */
    outb(0x3C8, 0);
    outb(0x3C9, 0); outb(0x3C9, 0); outb(0x3C9, 0);

    /* Индекс 1: Тёмно-серый (21/63 ≈ 85/255) */
    outb(0x3C8, 1);
    outb(0x3C9, 21); outb(0x3C9, 21); outb(0x3C9, 21);

    /* Индекс 2: Светло-серый (42/63 ≈ 170/255) */
    outb(0x3C8, 2);
    outb(0x3C9, 42); outb(0x3C9, 42); outb(0x3C9, 42);

    /* Индекс 3: Белый (63/63 = 255/255) */
    outb(0x3C8, 3);
    outb(0x3C9, 63); outb(0x3C9, 63); outb(0x3C9, 63);
}

void gfx_init(void) {
    vga_set_mode_13h();
    gfx_init_palette();
    gfx_clear(COLOR_BLACK);
    gfx_flip();
}

uint8_t *gfx_get_backbuffer(void) {
    return gfx_backbuffer;
}

void gfx_clear(uint8_t color) {
    color &= 0x03;
    /* Упаковываем один и тот же 2-битный цвет во все 4 позиции байта */
    uint8_t packed = (uint8_t)((color << 6) | (color << 4) | (color << 2) | color);
    for (uint32_t i = 0; i < GFX_BUFFER_SIZE; i++) {
        gfx_backbuffer[i] = packed;
    }
}

void gfx_put_pixel(int x, int y, uint8_t color) {
    if ((uint32_t)x >= GFX_WIDTH || (uint32_t)y >= GFX_HEIGHT) {
        return;
    }
    color &= 0x03;

    /* Индекс байта в буфере и сдвиг пикселя внутри байта (4 пикселя на байт) */
    uint32_t byte_idx = (uint32_t)y * GFX_ROW_BYTES + ((uint32_t)x >> 2);
    uint32_t shift = (3 - (x & 3)) << 1;
    uint8_t mask = (uint8_t)(0x03 << shift);

    gfx_backbuffer[byte_idx] = (gfx_backbuffer[byte_idx] & ~mask) | (color << shift);
}

uint8_t gfx_get_pixel(int x, int y) {
    if ((uint32_t)x >= GFX_WIDTH || (uint32_t)y >= GFX_HEIGHT) {
        return 0;
    }
    uint32_t byte_idx = (uint32_t)y * GFX_ROW_BYTES + ((uint32_t)x >> 2);
    uint32_t shift = (3 - (x & 3)) << 1;
    return (gfx_backbuffer[byte_idx] >> shift) & 0x03;
}

void gfx_flip(void) {
    /* Быстрая распаковка 2-битного буфера ядра (16 КБ) в аппаратный буфер VGA */
    uint32_t vga_idx = 0;
    for (uint32_t i = 0; i < GFX_BUFFER_SIZE; i++) {
        uint8_t b = gfx_backbuffer[i];
        vga_mem[vga_idx++] = (b >> 6) & 0x03;
        vga_mem[vga_idx++] = (b >> 4) & 0x03;
        vga_mem[vga_idx++] = (b >> 2) & 0x03;
        vga_mem[vga_idx++] = b & 0x03;
    }
}

void gfx_draw_line(int x0, int y0, int x1, int y1, uint8_t color) {
    int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 >= y0) ? (y0 - y1) : (y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        gfx_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_draw_rect(int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) return;
    gfx_draw_line(x, y, x + w - 1, y, color);
    gfx_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    gfx_draw_line(x, y, x, y + h - 1, color);
    gfx_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void gfx_fill_rect(int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) return;
    for (int cy = y; cy < y + h; cy++) {
        for (int cx = x; cx < x + w; cx++) {
            gfx_put_pixel(cx, cy, color);
        }
    }
}

void gfx_draw_circle(int xc, int yc, int r, uint8_t color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        gfx_put_pixel(xc + x, yc + y, color);
        gfx_put_pixel(xc - x, yc + y, color);
        gfx_put_pixel(xc + x, yc - y, color);
        gfx_put_pixel(xc - x, yc - y, color);
        gfx_put_pixel(xc + y, yc + x, color);
        gfx_put_pixel(xc - y, yc + x, color);
        gfx_put_pixel(xc + y, yc - x, color);
        gfx_put_pixel(xc - y, yc - x, color);
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void gfx_fill_circle(int xc, int yc, int r, uint8_t color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        gfx_draw_line(xc - x, yc + y, xc + x, yc + y, color);
        gfx_draw_line(xc - x, yc - y, xc + x, yc - y, color);
        gfx_draw_line(xc - y, yc + x, xc + y, yc + x, color);
        gfx_draw_line(xc - y, yc - x, xc + y, yc - x, color);
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void gfx_draw_char(int x, int y, char c, uint8_t fg, int bg) {
    if ((unsigned char)c > 127) return;
    const uint8_t *glyph = font8x8_basic[(unsigned char)c];
    for (int row = 0; row < 8; row++) {
        uint8_t row_bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if ((row_bits >> col) & 1) {
                gfx_put_pixel(x + col, y + row, fg);
            } else if (bg >= 0) {
                gfx_put_pixel(x + col, y + row, (uint8_t)bg);
            }
        }
    }
}

void gfx_draw_text(int x, int y, const char *str, uint8_t fg, int bg) {
    int cur_x = x;
    int cur_y = y;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += 10;
        } else {
            gfx_draw_char(cur_x, cur_y, *str, fg, bg);
            cur_x += 8;
        }
        str++;
    }
}

static int gfx_utf8_decode(const uint8_t *text, uint32_t *codepoint_out) {
    uint8_t first = text[0];
    if (first < 0x80u) {
        *codepoint_out = first;
        return 1;
    }
    if (first >= 0xC2u && first <= 0xDFu &&
        (text[1] & 0xC0u) == 0x80u) {
        *codepoint_out = ((uint32_t)(first & 0x1Fu) << 6) |
                         (uint32_t)(text[1] & 0x3Fu);
        return 2;
    }
    if (first >= 0xE0u && first <= 0xEFu &&
        (text[1] & 0xC0u) == 0x80u &&
        (text[2] & 0xC0u) == 0x80u) {
        uint32_t cp = ((uint32_t)(first & 0x0Fu) << 12) |
                      ((uint32_t)(text[1] & 0x3Fu) << 6) |
                      (uint32_t)(text[2] & 0x3Fu);
        if (cp >= 0x800u && !(cp >= 0xD800u && cp <= 0xDFFFu)) {
            *codepoint_out = cp;
            return 3;
        }
    }
    if (first >= 0xF0u && first <= 0xF4u &&
        (text[1] & 0xC0u) == 0x80u &&
        (text[2] & 0xC0u) == 0x80u &&
        (text[3] & 0xC0u) == 0x80u) {
        uint32_t cp = ((uint32_t)(first & 0x07u) << 18) |
                      ((uint32_t)(text[1] & 0x3Fu) << 12) |
                      ((uint32_t)(text[2] & 0x3Fu) << 6) |
                      (uint32_t)(text[3] & 0x3Fu);
        if (cp >= 0x10000u && cp <= 0x10FFFFu) {
            *codepoint_out = cp;
            return 4;
        }
    }
    *codepoint_out = '?';
    return 1;
}

static void gfx_draw_cyrillic_glyph(int x, int y, uint32_t codepoint,
                                    uint8_t fg, int bg) {
    uint32_t index;
    if (codepoint >= 0x0410u && codepoint <= 0x042Fu) {
        index = codepoint - 0x0410u;
    } else if (codepoint >= 0x0430u && codepoint <= 0x044Fu) {
        index = 32u + codepoint - 0x0430u;
    } else if (codepoint == 0x0401u) {
        index = 64u;
    } else if (codepoint == 0x0451u) {
        index = 65u;
    } else {
        gfx_draw_char(x, y, '?', fg, bg);
        return;
    }

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (bg >= 0) gfx_put_pixel(x + col, y + row, (uint8_t)bg);
        }
    }

    for (uint32_t row = 0; row < 7; row++) {
        uint8_t bits = font5x7_cyrillic[index][row];
        for (uint32_t col = 0; col < 5; col++) {
            if ((bits >> (4u - col)) & 1u) {
                gfx_put_pixel(x + (int)col + 1, y + (int)row, fg);
            }
        }
    }
}

void gfx_draw_utf8(int x, int y, const char *str, uint8_t fg, int bg) {
    if (str == NULL) return;

    int cur_x = x;
    int cur_y = y;
    const uint8_t *cursor = (const uint8_t *)str;
    while (*cursor != 0) {
        uint32_t codepoint = 0;
        int consumed = gfx_utf8_decode(cursor, &codepoint);
        if (consumed <= 0) break;

        if (codepoint == '\n') {
            cur_x = x;
            cur_y += 10;
        } else if (codepoint < 0x80u) {
            gfx_draw_char(cur_x, cur_y, (char)codepoint, fg, bg);
            cur_x += 8;
        } else {
            gfx_draw_cyrillic_glyph(cur_x, cur_y, codepoint, fg, bg);
            cur_x += 8;
        }
        cursor += consumed;
    }
}
