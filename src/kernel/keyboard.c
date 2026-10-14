#include "keyboard.h"
#include "isr.h"
#include "io.h"

#define KBD_DATA_PORT 0x60
#define KBD_BUF_SIZE  128

static uint32_t kbd_buffer[KBD_BUF_SIZE];
static volatile int buf_head = 0;
static volatile int buf_tail = 0;

static int left_shift_active = 0;
static int right_shift_active = 0;
static int extended_code = 0;
static int russian_layout = 0;

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

static void buffer_put(uint32_t c) {
    int next = (buf_head + 1) % KBD_BUF_SIZE;
    if (next != buf_tail) {
        kbd_buffer[buf_head] = c;
        buf_head = next;
    }
}


static int shift_is_active(void) {
    return left_shift_active || right_shift_active;
}

static uint32_t translate_russian(uint8_t code, int shift, uint32_t fallback) {
    switch (code) {
        case 0x10: return shift ? 0x0419u : 0x0439u;
        case 0x11: return shift ? 0x0426u : 0x0446u;
        case 0x12: return shift ? 0x0423u : 0x0443u;
        case 0x13: return shift ? 0x041Au : 0x043Au;
        case 0x14: return shift ? 0x0415u : 0x0435u;
        case 0x15: return shift ? 0x041Du : 0x043Du;
        case 0x16: return shift ? 0x0413u : 0x0433u;
        case 0x17: return shift ? 0x0428u : 0x0448u;
        case 0x18: return shift ? 0x0429u : 0x0449u;
        case 0x19: return shift ? 0x0417u : 0x0437u;
        case 0x1A: return shift ? 0x0425u : 0x0445u;
        case 0x1B: return shift ? 0x042Au : 0x044Au;
        case 0x1E: return shift ? 0x0424u : 0x0444u;
        case 0x1F: return shift ? 0x042Bu : 0x044Bu;
        case 0x20: return shift ? 0x0412u : 0x0432u;
        case 0x21: return shift ? 0x0410u : 0x0430u;
        case 0x22: return shift ? 0x041Fu : 0x043Fu;
        case 0x23: return shift ? 0x0420u : 0x0440u;
        case 0x24: return shift ? 0x041Eu : 0x043Eu;
        case 0x25: return shift ? 0x041Bu : 0x043Bu;
        case 0x26: return shift ? 0x0414u : 0x0434u;
        case 0x27: return shift ? 0x0416u : 0x0436u;
        case 0x28: return shift ? 0x042Du : 0x044Du;
        case 0x29: return shift ? 0x0401u : 0x0451u;
        case 0x2C: return shift ? 0x042Fu : 0x044Fu;
        case 0x2D: return shift ? 0x0427u : 0x0447u;
        case 0x2E: return shift ? 0x0421u : 0x0441u;
        case 0x2F: return shift ? 0x041Cu : 0x043Cu;
        case 0x30: return shift ? 0x0418u : 0x0438u;
        case 0x31: return shift ? 0x0422u : 0x0442u;
        case 0x32: return shift ? 0x042Cu : 0x044Cu;
        case 0x33: return shift ? 0x0411u : 0x0431u;
        case 0x34: return shift ? 0x042Eu : 0x044Eu;
        case 0x35: return shift ? ',' : '.';
        default: return fallback;
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
                case 0x48: buffer_put(KEY_UP); break;
                case 0x50: buffer_put(KEY_DOWN); break;
                case 0x4B: buffer_put(KEY_LEFT); break;
                case 0x4D: buffer_put(KEY_RIGHT); break;
                case 0x47: buffer_put(KEY_HOME); break;
                case 0x4F: buffer_put(KEY_END); break;
                case 0x53: buffer_put(KEY_DELETE); break;
                case 0x49: buffer_put(KEY_PAGE_UP); break;
                case 0x51: buffer_put(KEY_PAGE_DOWN); break;
                case 0x52: buffer_put(KEY_INSERT); break;
            }
        }
        return;
    }

    if (scancode == 0x2A) { left_shift_active = 1; return; }
    if (scancode == 0x36) { right_shift_active = 1; return; }
    if (scancode == 0xAA) { left_shift_active = 0; return; }
    if (scancode == 0xB6) { right_shift_active = 0; return; }

    if (scancode & 0x80) return;

    if (scancode >= 0x3B && scancode <= 0x44) {
        buffer_put((uint32_t)(KEY_F1 + (scancode - 0x3B)));
        return;
    }
    if (scancode == 0x57) { buffer_put(KEY_F11); return; }
    if (scancode == 0x58) {
        russian_layout = !russian_layout;
        buffer_put(KEY_F12);
        buffer_put(KEY_LAYOUT_TOGGLE);
        return;
    }

    if (scancode < 128) {
        int shift = shift_is_active();
        uint32_t fallback = shift ? kbd_ascii_shift[scancode]
                                  : kbd_ascii_normal[scancode];
        uint32_t codepoint = russian_layout
            ? translate_russian(scancode, shift, fallback)
            : fallback;
        if (codepoint != 0) buffer_put(codepoint);
    }
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_callback);
}

int keyboard_has_char(void) {
    return buf_head != buf_tail;
}

uint32_t keyboard_get_codepoint(void) {
    if (buf_head == buf_tail) return 0;
    uint32_t codepoint = kbd_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KBD_BUF_SIZE;
    return codepoint;
}

uint32_t keyboard_wait_codepoint(void) {
    while (!keyboard_has_char()) {
        __asm__ volatile ("hlt");
    }
    return keyboard_get_codepoint();
}

/* Compatibility API for applications compiled against the original byte ABI. */
uint8_t keyboard_get_char(void) {
    uint32_t codepoint = keyboard_get_codepoint();
    return codepoint <= 0xFFu ? (uint8_t)codepoint : (uint8_t)'?';
}

uint8_t keyboard_wait_char(void) {
    uint32_t codepoint = keyboard_wait_codepoint();
    return codepoint <= 0xFFu ? (uint8_t)codepoint : (uint8_t)'?';
}

int keyboard_russian_layout(void) {
    return russian_layout;
}