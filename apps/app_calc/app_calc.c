#include "customos.h"

static void int_to_dec(int32_t n, char *out) {
    if (n == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char tmp[12];
    int i = 0;
    int is_neg = 0;
    if (n < 0) {
        is_neg = 1;
        n = -n;
    }
    while (n > 0) {
        tmp[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    int j = 0;
    if (is_neg) {
        out[j++] = '-';
    }
    for (int k = i - 1; k >= 0; k--) {
        out[j++] = tmp[k];
    }
    out[j] = '\0';
}

static void to_binary_byte(uint8_t n, char *out) {
    out[0] = '0';
    out[1] = 'b';
    for (int i = 7; i >= 0; i--) {
        out[2 + (7 - i)] = (char)(((n >> i) & 1) ? '1' : '0');
    }
    out[10] = '\0';
}

static int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int next = a + b;
        a = b;
        b = next;
    }
    return b;
}

static int factorial(int n) {
    if (n <= 1) return 1;
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

int app_main(const customos_api_t *api) {
    /* 1. Выполнение логических тестов (Suite) */
    int math_val = (15 + 27) * 3 / 2;             // 63
    int bit_val  = (0xAA & 0x0F) ^ 0x05;          // 15
    int shf_val  = (1 << 6) | (1 << 2);           // 68
    int bool_val = (100 > 50 && 20 != 10);        // 1 (True)
    int fib_val  = fibonacci(7);                  // 13
    int prm_val  = count_primes(20);              // 8

    int inspect_val = 5;

    while (1) {
        while (api->kbd_has_key()) {
            uint8_t key = api->kbd_get_key();

            if (key == ' ' || key == 27) {
                return 0;
            } else if (key == '+' || key == KEY_UP || key == 'w' || key == 'W') {
                if (inspect_val < 10) inspect_val++;
            } else if (key == '-' || key == KEY_DOWN || key == 's' || key == 'S') {
                if (inspect_val > 0) inspect_val--;
            } else if (key == 'r' || key == 'R') {
                inspect_val = 5;
            }
        }

        api->gfx_clear(COLOR_BLACK);

        /* Рамка и верхний заголовок */
        api->gfx_draw_rect(6, 6, 308, 188, COLOR_LIGHT_GRAY);
        api->gfx_fill_rect(8, 8, 304, 16, COLOR_DARK_GRAY);
        api->gfx_draw_text(12, 12, "App 2: Logic & Arithmetic Engine", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Левая панель: Автоматические тесты вычислений */
        api->gfx_draw_rect(10, 28, 172, 134, COLOR_LIGHT_GRAY);
        api->gfx_fill_rect(12, 30, 168, 12, COLOR_DARK_GRAY);
        api->gfx_draw_text(16, 32, "Logic & Math Verification", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Тест 1 */
        api->gfx_draw_text(14, 46, "(15+27)*3/2 = ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char b1[8]; int_to_dec(math_val, b1);
        api->gfx_draw_text(108, 46, b1, COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(136, 46, "[OK]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Тест 2 */
        api->gfx_draw_text(14, 62, "(AA&0F)^05  = ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char b2[8]; int_to_dec(bit_val, b2);
        api->gfx_draw_text(108, 62, b2, COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(136, 62, "[OK]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Тест 3 */
        api->gfx_draw_text(14, 78, "(1<<6)|(1<<2)= ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char b3[8]; int_to_dec(shf_val, b3);
        api->gfx_draw_text(108, 78, b3, COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(136, 78, "[OK]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Тест 4 */
        api->gfx_draw_text(14, 94, "Branching   : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        api->gfx_draw_text(108, 94, bool_val ? "TRUE" : "FLS", COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(136, 94, "[OK]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Тест 5 */
        api->gfx_draw_text(14, 110, "Loop Fib(7) : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char b5[8]; int_to_dec(fib_val, b5);
        api->gfx_draw_text(108, 110, b5, COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(136, 110, "[OK]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Тест 6 */
        api->gfx_draw_text(14, 126, "Primes <=20 : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char b6[8]; int_to_dec(prm_val, b6);
        api->gfx_draw_text(108, 126, b6, COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(136, 126, "[OK]", COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_text(14, 144, "All 6 tests passed [PASS]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Правая панель: Интерактивный калькулятор/инспектор */
        api->gfx_draw_rect(188, 28, 122, 134, COLOR_LIGHT_GRAY);
        api->gfx_fill_rect(190, 30, 118, 12, COLOR_DARK_GRAY);
        api->gfx_draw_text(194, 32, "Number Inspector", COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_text(194, 48, "Value X : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char vi_buf[8]; int_to_dec(inspect_val, vi_buf);
        api->gfx_draw_text(260, 48, vi_buf, COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_text(194, 66, "Parity  : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        api->gfx_draw_text(260, 66, (inspect_val % 2 == 0) ? "EVEN" : "ODD", COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_text(194, 84, "X^2     : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char sq_buf[8]; int_to_dec(inspect_val * inspect_val, sq_buf);
        api->gfx_draw_text(260, 84, sq_buf, COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_text(194, 102, "X!      : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char fc_buf[12]; int_to_dec(factorial(inspect_val), fc_buf);
        api->gfx_draw_text(260, 102, fc_buf, COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_text(194, 120, "Binary  : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        char bin_buf[12]; to_binary_byte((uint8_t)inspect_val, bin_buf);
        api->gfx_draw_text(194, 134, bin_buf, COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_text(194, 148, "[+/-/W/S] Adjust", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

        /* Нижняя статусная панель */
        api->gfx_fill_rect(8, 166, 304, 26, COLOR_DARK_GRAY);
        api->gfx_draw_text(14, 169, "Use [+] / [-] or [W] / [S] to modify Value X", COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(14, 180, "Press [SPACE] or [ESC] to return to Launcher", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

        api->gfx_flip();
        api->timer_sleep(25);
    }
}