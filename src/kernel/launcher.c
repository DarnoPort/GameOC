#include "launcher.h"
#include "app_registry.h"
#include "gfx.h"
#include "keyboard.h"
#include "timer.h"

static void int_to_dec(uint32_t n, char *out) {
    if (n == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char tmp[12];
    int i = 0;
    while (n > 0) {
        tmp[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    int j = 0;
    for (int k = i - 1; k >= 0; k--) {
        out[j++] = tmp[k];
    }
    out[j] = '\0';
}

void launcher_run(void) {
    int selected = 0;
    int last_exit_code = -999;
    char last_app_name[32] = {0};

    while (1) {
        int app_count = app_registry_get_count();

        /* Обработка ввода в меню */
        while (keyboard_has_char()) {
            uint8_t key = keyboard_get_char();

            if (key == KEY_UP || key == 'w' || key == 'W') {
                if (app_count > 0) {
                    selected = (selected - 1 + app_count) % app_count;
                }
            } else if (key == KEY_DOWN || key == 's' || key == 'S') {
                if (app_count > 0) {
                    selected = (selected + 1) % app_count;
                }
            } else if (key == '\n') {
                /* Запуск выбранного приложения */
                if (app_count > 0) {
                    const registered_app_t *app = app_registry_get(selected);
                    if (app) {
                        for (int i = 0; i < 31 && app->name[i] != '\0'; i++) {
                            last_app_name[i] = app->name[i];
                            last_app_name[i + 1] = '\0';
                        }
                    }

                    last_exit_code = app_registry_launch(selected);

                    /* Восстановление палитры и графического режима после выхода */
                    gfx_init();
                }
            }
        }

        /* 1. Фон экрана */
        gfx_clear(COLOR_BLACK);

        /* 2. Верхняя строка меню */
        gfx_fill_rect(0, 0, 320, 16, COLOR_WHITE);
        gfx_draw_text(6, 4, "CustomOS v0.1 -- Application Launcher", COLOR_BLACK, COLOR_TRANSPARENT);

        char up_buf[12];
        int_to_dec(timer_get_ticks() / 100, up_buf);
        gfx_draw_text(240, 4, "Up: ", COLOR_BLACK, COLOR_TRANSPARENT);
        gfx_draw_text(268, 4, up_buf, COLOR_BLACK, COLOR_TRANSPARENT);
        gfx_draw_text(296, 4, "s", COLOR_BLACK, COLOR_TRANSPARENT);

        /* 3. Левая панель: Список установленных программ */
        gfx_draw_rect(8, 22, 185, 154, COLOR_LIGHT_GRAY);
        gfx_fill_rect(10, 24, 181, 14, COLOR_DARK_GRAY);
        gfx_draw_text(14, 27, "Installed Applications", COLOR_WHITE, COLOR_TRANSPARENT);

        if (app_count == 0) {
            gfx_draw_text(16, 50, "No applications found!", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        } else {
            for (int i = 0; i < app_count; i++) {
                const registered_app_t *app = app_registry_get(i);
                int y_pos = 46 + (i * 22);

                if (i == selected) {
                    /* Подсветка активного элемента */
                    gfx_fill_rect(12, y_pos - 2, 177, 18, COLOR_WHITE);
                    gfx_draw_text(16, y_pos + 2, ">", COLOR_BLACK, COLOR_TRANSPARENT);

                    char idx_str[4];
                    idx_str[0] = (char)('1' + i);
                    idx_str[1] = '.';
                    idx_str[2] = ' ';
                    idx_str[3] = '\0';
                    gfx_draw_text(26, y_pos + 2, idx_str, COLOR_BLACK, COLOR_TRANSPARENT);
                    gfx_draw_text(44, y_pos + 2, app->name, COLOR_BLACK, COLOR_TRANSPARENT);
                } else {
                    /* Неактивный элемент */
                    char idx_str[4];
                    idx_str[0] = (char)('1' + i);
                    idx_str[1] = '.';
                    idx_str[2] = ' ';
                    idx_str[3] = '\0';
                    gfx_draw_text(26, y_pos + 2, idx_str, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
                    gfx_draw_text(44, y_pos + 2, app->name, COLOR_WHITE, COLOR_TRANSPARENT);
                }
            }
        }

        /* 4. Правая панель: Свойства и описание выбранного приложения */
        gfx_draw_rect(198, 22, 114, 154, COLOR_LIGHT_GRAY);
        gfx_fill_rect(200, 24, 110, 14, COLOR_DARK_GRAY);
        gfx_draw_text(204, 27, "Application Info", COLOR_WHITE, COLOR_TRANSPARENT);

        if (app_count > 0 && selected < app_count) {
            const registered_app_t *app = app_registry_get(selected);

            gfx_draw_text(204, 46, "Name:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            gfx_draw_text(204, 58, app->name, COLOR_WHITE, COLOR_TRANSPARENT);

            gfx_draw_text(204, 76, "Size:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            char sz_buf[12];
            int_to_dec(app->size, sz_buf);
            gfx_draw_text(204, 88, sz_buf, COLOR_WHITE, COLOR_TRANSPARENT);
            gfx_draw_text(250, 88, "Bytes", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

            gfx_draw_text(204, 106, "File:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            gfx_draw_text(204, 118, app->filename, COLOR_WHITE, COLOR_TRANSPARENT);

            gfx_draw_text(204, 136, "Target Base:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            gfx_draw_text(204, 148, "0x00400000", COLOR_WHITE, COLOR_TRANSPARENT);
        }

        /* 5. Нижняя строка состояния */
        gfx_fill_rect(0, 182, 320, 18, COLOR_DARK_GRAY);
        gfx_draw_line(0, 181, 319, 181, COLOR_WHITE);

        if (last_exit_code != -999) {
            gfx_draw_text(6, 186, "Returned: ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            gfx_draw_text(76, 186, last_app_name, COLOR_WHITE, COLOR_TRANSPARENT);
            gfx_draw_text(160, 186, "(Code: ", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

            char code_str[12];
            int_to_dec((uint32_t)last_exit_code, code_str);
            gfx_draw_text(210, 186, code_str, COLOR_WHITE, COLOR_TRANSPARENT);
            gfx_draw_text(225, 186, ")", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        } else {
            gfx_draw_text(6, 186, "[UP/DOWN/W/S] Select  |  [ENTER] Launch", COLOR_WHITE, COLOR_TRANSPARENT);
        }

        gfx_flip();
        timer_sleep(20);
    }
}