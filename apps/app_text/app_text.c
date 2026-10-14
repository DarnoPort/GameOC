#include "customos.h"

static void int_to_str(int32_t n, char *out) {
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

int app_main(const customos_api_t *api) {
    uint8_t text_fg = COLOR_WHITE;
    int theme_idx = 0;

    while (1) {
        /* Обработка клавиатурного ввода */
        while (api->kbd_has_key()) {
            uint8_t key = api->kbd_get_key();

            if (key == ' ' || key == 27) {
                /* Выход по пробелу или ESC */
                return 0;
            } else if (key == 'c' || key == 'C') {
                /* Переключение контраста/темы текста */
                theme_idx = (theme_idx + 1) % 3;
                if (theme_idx == 0) text_fg = COLOR_WHITE;
                else if (theme_idx == 1) text_fg = COLOR_LIGHT_GRAY;
                else text_fg = COLOR_DARK_GRAY;
            }
        }

        /* 1. Очистка экрана */
        api->gfx_clear(COLOR_BLACK);

        /* 2. Рамка окна приложения */
        api->gfx_draw_rect(6, 6, 308, 188, COLOR_LIGHT_GRAY);
        api->gfx_fill_rect(8, 8, 304, 16, COLOR_DARK_GRAY);
        api->gfx_draw_text(12, 12, "App 1: Text Subsystem & Layout Demo", COLOR_WHITE, COLOR_TRANSPARENT);

        /* 3. Описание возможностей */
        api->gfx_draw_text(14, 28, "CustomOS Text Renderer (8x8 Bitmap Glyphs)", text_fg, COLOR_TRANSPARENT);
        api->gfx_draw_text(14, 38, "Testing: multiline flow, layout, and contrast", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

        /* 4. Таблица статуса подсистем */
        api->gfx_draw_rect(12, 50, 296, 68, COLOR_DARK_GRAY);
        api->gfx_fill_rect(14, 52, 292, 12, COLOR_DARK_GRAY);
        api->gfx_draw_text(18, 54, "Subsystem", COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(130, 54, "Mode", COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(220, 54, "Health", COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_line(12, 65, 307, 65, COLOR_WHITE);

        /* Строка 1 */
        api->gfx_draw_text(18, 68, "Kernel Core", text_fg, COLOR_TRANSPARENT);
        api->gfx_draw_text(130, 68, "Ring 0 / Flat", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        api->gfx_draw_text(220, 68, "[ ONLINE ]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Строка 2 */
        api->gfx_draw_text(18, 79, "Video Engine", text_fg, COLOR_TRANSPARENT);
        api->gfx_draw_text(130, 79, "320x200 2bpp", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        api->gfx_draw_text(220, 79, "[ READY  ]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Строка 3 */
        api->gfx_draw_text(18, 90, "Syscall ABI", text_fg, COLOR_TRANSPARENT);
        api->gfx_draw_text(130, 90, "Table V1", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        api->gfx_draw_text(220, 90, "[ ACTIVE ]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* Строка 4 */
        api->gfx_draw_text(18, 101, "Memory Space", text_fg, COLOR_TRANSPARENT);
        api->gfx_draw_text(130, 101, "0x00400000", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        api->gfx_draw_text(220, 101, "[ SAFE   ]", COLOR_WHITE, COLOR_TRANSPARENT);

        /* 5. Интерактивная демонстрация шрифта и счётчика */
        api->gfx_draw_rect(12, 124, 296, 36, COLOR_LIGHT_GRAY);
        api->gfx_draw_text(16, 128, "Dynamic Ticks: ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

        char t_str[12];
        int_to_str((int32_t)api->timer_get_ticks(), t_str);
        api->gfx_draw_text(130, 128, t_str, text_fg, COLOR_TRANSPARENT);

        api->gfx_draw_text(16, 142, "Color Theme  : ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        if (theme_idx == 0) api->gfx_draw_text(130, 142, "White (High Contrast)", COLOR_WHITE, COLOR_TRANSPARENT);
        else if (theme_idx == 1) api->gfx_draw_text(130, 142, "Light Gray (Soft)", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        else api->gfx_draw_text(130, 142, "Dark Gray (Dim)", COLOR_DARK_GRAY, COLOR_TRANSPARENT);

        /* 6. Панель управления внизу */
        api->gfx_fill_rect(8, 166, 304, 26, COLOR_DARK_GRAY);
        api->gfx_draw_text(14, 169, "Press [C] to cycle text color theme", COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(14, 180, "Press [SPACE] or [ESC] to return to Launcher", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

        api->gfx_flip();
        api->timer_sleep(25);
    }
}