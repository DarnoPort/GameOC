#include "gfx.h"
#include "io.h"
#include "font8x8.h"

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