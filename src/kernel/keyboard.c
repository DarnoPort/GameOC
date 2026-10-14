#include "keyboard.h"
#include "isr.h"
#include "io.h"

#define KBD_DATA_PORT 0x60
#define KBD_BUF_SIZE  128

static uint8_t kbd_buffer[KBD_BUF_SIZE];
static volatile int buf_head = 0;
static volatile int buf_tail = 0;

static int shift_active = 0;
static int extended_code = 0;

/* Таблица трансляции Set 1: без Shift */
static const uint8_t kbd_ascii_normal[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/* Таблица трансляции Set 1: с Shift */
static const uint8_t kbd_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static void buffer_put(uint8_t c) {
    int next = (buf_head + 1) % KBD_BUF_SIZE;
    if (next != buf_tail) {
        kbd_buffer[buf_head] = c;
        buf_head = next;
    }
}

static void keyboard_callback(registers_t *regs) {
    (void)regs;
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (scancode == 0xE0) {
        extended_code = 1;
        return;
    }

    if (extended_code) {
        extended_code = 0;
        if (!(scancode & 0x80)) {
            switch (scancode) {
                case 0x48: buffer_put((uint8_t)KEY_UP); break;
                case 0x50: buffer_put((uint8_t)KEY_DOWN); break;
                case 0x4B: buffer_put((uint8_t)KEY_LEFT); break;
                case 0x4D: buffer_put((uint8_t)KEY_RIGHT); break;
            }
        }
        return;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_active = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_active = 0;
        return;
    }

    if (scancode & 0x80) {
        return;
    }

    if (scancode < 128) {
        uint8_t ch = shift_active ? kbd_ascii_shift[scancode] : kbd_ascii_normal[scancode];
        if (ch != 0) {
            buffer_put(ch);
        }
    }
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_callback);
}

int keyboard_has_char(void) {
    return buf_head != buf_tail;
}

uint8_t keyboard_get_char(void) {
    if (buf_head == buf_tail) {
        return 0;
    }
    uint8_t c = kbd_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KBD_BUF_SIZE;
    return c;
}

uint8_t keyboard_wait_char(void) {
    while (!keyboard_has_char()) {
        __asm__ volatile ("hlt");
    }
    return keyboard_get_char();
}