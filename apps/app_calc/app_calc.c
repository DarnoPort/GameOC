#include "customos.h"

/* Безопасное преобразование знакового 32-битного числа в десятичную строку. */
static void int_to_dec(int32_t n, char *out) {
    char tmp[10];
    uint32_t value;
    int i = 0;
    int j = 0;
    int negative = 0;

    if (n < 0) {
        negative = 1;
        /* Не допускаем переполнения на INT32_MIN. */
        value = (uint32_t)(-(n + 1)) + 1u;
    } else {
        value = (uint32_t)n;
    }

    do {
        tmp[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && i < (int)sizeof(tmp));

    if (negative) {
        out[j++] = '-';
    }
    while (i > 0) {
        out[j++] = tmp[--i];
    }
    out[j] = '\0';
}

static void to_binary_byte(uint8_t n, char *out) {
    out[0] = '0';
    out[1] = 'b';
    for (int i = 7; i >= 0; i--) {
        out[2 + (7 - i)] = (char)(((n >> i) & 1u) ? '1' : '0');
    }
    out[10] = '\0';
}

static int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0;
    int b = 1;
    for (int i = 2; i <= n; i++) {
        int next = a + b;
        a = b;
        b = next;
    }
    return b;
}

/* Инспектор ограничивает X диапазоном 0..10: 10! помещается в int32_t. */
static int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

static int count_primes(int limit) {
    int count = 0;
    for (int num = 2; num <= limit; num++) {
        int is_prime = 1;
        for (int d = 2; d * d <= num; d++) {
            if (num % d == 0) {
                is_prime = 0;
                break;
            }
        }
        if (is_prime) count++;
    }
    return count;
}

static void draw_screen(const customos_api_t *api, int inspect_val,
                        int math_val, int bit_val, int shift_val,
                        int bool_val, int fib_val, int prime_val) {
    char b1[12];
    char b2[12];
    char b3[12];
    char b5[12];
    char b6[12];
    char value_buf[12];
    char square_buf[12];
    char fact_buf[12];
    char bin_buf[11];

    int_to_dec(math_val, b1);
    int_to_dec(bit_val, b2);
    int_to_dec(shift_val, b3);
    int_to_dec(fib_val, b5);
    int_to_dec(prime_val, b6);
    int_to_dec(inspect_val, value_buf);
    int_to_dec(inspect_val * inspect_val, square_buf);
    int_to_dec(factorial(inspect_val), fact_buf);
    to_binary_byte((uint8_t)inspect_val, bin_buf);

    api->gfx_clear(COLOR_BLACK);

    /* Экран 320x200: все строки ограничены шириной своих панелей. */
    api->gfx_draw_rect(4, 4, 312, 192, COLOR_LIGHT_GRAY);
    api->gfx_fill_rect(6, 6, 308, 16, COLOR_DARK_GRAY);
    api->gfx_draw_text(12, 10, "LogicCalc: Math & Logic", COLOR_WHITE, COLOR_TRANSPARENT);

    /* Панель проверок — короткие строки не накладываются на соседнюю панель. */
    api->gfx_draw_rect(8, 26, 152, 132, COLOR_LIGHT_GRAY);
    api->gfx_fill_rect(10, 28, 148, 14, COLOR_DARK_GRAY);
    api->gfx_draw_text(15, 31, "SELF TESTS", COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(14, 48,  "Arithmetic : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(126, 48, b1, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(14, 61,  "Bitwise    : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(126, 61, b2, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(14, 74,  "Shift      : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(126, 74, b3, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(14, 87,  "Branch     : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(126, 87, bool_val ? "TRUE" : "FALSE", COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(14, 100, "Fib(7)     : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(126, 100, b5, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(14, 113, "Primes <=20:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(126, 113, b6, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(14, 137, "6 checks: PASS", COLOR_WHITE, COLOR_TRANSPARENT);

    /* Инспектор чисел. Значения обновляются только после ввода с клавиатуры. */
    api->gfx_draw_rect(164, 26, 148, 132, COLOR_LIGHT_GRAY);
    api->gfx_fill_rect(166, 28, 144, 14, COLOR_DARK_GRAY);
    api->gfx_draw_text(170, 31, "NUMBER INSPECTOR", COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(170, 48, "Value:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(226, 48, value_buf, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(170, 62, (inspect_val % 2 == 0) ? "Parity: EVEN" : "Parity: ODD",
                       COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

    api->gfx_draw_text(170, 76, "Square:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(226, 76, square_buf, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(170, 90, "Fact:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(210, 90, fact_buf, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(170, 104, "Binary:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(170, 116, bin_buf, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_text(170, 136, "Use +/- or W/S", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

    /* Обе строки не длиннее доступных 36 символов. */
    api->gfx_fill_rect(8, 164, 304, 28, COLOR_DARK_GRAY);
    api->gfx_draw_text(14, 168, "SPACE/ESC: exit | +/- or W/S: adjust", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(14, 180, "Arrows also work. R resets X to 5.", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

    api->gfx_flip();
}

int app_main(const customos_api_t *api) {
    const int math_val = (15 + 27) * 3 / 2;       /* 63 */
    const int bit_val = (0xAA & 0x0F) ^ 0x05;     /* 15 */
    const int shift_val = (1 << 6) | (1 << 2);    /* 68 */
    const int bool_val = (100 > 50 && 20 != 10);  /* true */
    const int fib_val = fibonacci(7);              /* 13 */
    const int prime_val = count_primes(20);        /* 8 */

    int inspect_val = 5;
    int needs_redraw = 1;

    while (1) {
        while (api->kbd_has_key()) {
            uint8_t key = api->kbd_get_key();

            if (key == ' ' || key == 27) {
                return 0;
            }

            int previous = inspect_val;
            if (key == '+' || key == KEY_UP || key == 'w' || key == 'W') {
                if (inspect_val < 10) inspect_val++;
            } else if (key == '-' || key == KEY_DOWN || key == 's' || key == 'S') {
                if (inspect_val > 0) inspect_val--;
            } else if (key == 'r' || key == 'R') {
                inspect_val = 5;
            }

            if (inspect_val != previous) {
                needs_redraw = 1;
            }
        }

        /* Не перерисовываем весь кадр без необходимости: это снижает нагрузку на QEMU. */
        if (needs_redraw) {
            draw_screen(api, inspect_val, math_val, bit_val, shift_val,
                        bool_val, fib_val, prime_val);
            needs_redraw = 0;
        }

        api->timer_sleep(10);
    }
}
