#include "vga.h"
#include "io.h"

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

static uint16_t *const vga_buffer = (uint16_t *)VGA_ADDRESS;
static uint32_t cursor_row = 0;
static uint32_t cursor_col = 0;
static uint8_t  current_color = 0x07;

static inline uint8_t make_color(uint8_t fg, uint8_t bg) {
    return (uint8_t)(fg | (bg << 4));
}

static inline uint16_t make_vga_entry(char c, uint8_t color) {
    return (uint16_t)((unsigned char)c) | ((uint16_t)color << 8);
}

static void update_hardware_cursor(uint32_t x, uint32_t y) {
    uint16_t pos = (uint16_t)(y * VGA_WIDTH + x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    current_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
}

void vga_clear(void) {
    for (uint32_t y = 0; y < VGA_HEIGHT; y++) {
        for (uint32_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = make_vga_entry(' ', current_color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hardware_cursor(cursor_col, cursor_row);
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = make_color(fg, bg);
}

static void scroll(void) {
    for (uint32_t y = 1; y < VGA_HEIGHT; y++) {
        for (uint32_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }
    for (uint32_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_vga_entry(' ', current_color);
    }
    cursor_row = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        vga_backspace();
        return;
    } else {
        vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = make_vga_entry(c, current_color);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        scroll();
    }
    update_hardware_cursor(cursor_col, cursor_row);
}

void vga_backspace(void) {
    if (cursor_col > 0) {
        cursor_col--;
        vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = make_vga_entry(' ', current_color);
        update_hardware_cursor(cursor_col, cursor_row);
    }
}

void vga_print(const char *str) {
    while (*str) {
        vga_putc(*str++);
    }
}

void vga_print_hex(uint32_t n) {
    const char hex_chars[] = "0123456789ABCDEF";
    vga_print("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (n >> i) & 0x0F;
        vga_putc(hex_chars[nibble]);
    }
}

void vga_print_dec(uint32_t n) {
    if (n == 0) {
        vga_putc('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        vga_putc(buf[j]);
    }
}