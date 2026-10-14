#include "launcher.h"
#include "app_registry.h"
#include "gfx.h"
#include "keyboard.h"
#include "timer.h"

#define LIST_VISIBLE_ROWS 6

static void int_to_dec(uint32_t n, char *out) {
    if (n == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    char tmp[12];
    int i = 0;
    while (n > 0 && i < (int)sizeof(tmp) - 1) {
        tmp[i++] = (char)('0' + (n % 10u));
        n /= 10u;
    }

    int j = 0;
    while (i > 0) out[j++] = tmp[--i];
    out[j] = '\0';
}

static uint32_t utf8_step(const char *text, uint32_t offset) {
    uint8_t first = (uint8_t)text[offset];
    uint32_t count = first < 0x80u ? 1u :
        (first >= 0xF0u ? 4u : (first >= 0xE0u ? 3u :
        (first >= 0xC2u ? 2u : 1u)));
    for (uint32_t i = 1; i < count; i++) {
        uint8_t byte = (uint8_t)text[offset + i];
        if (byte == 0 || (byte & 0xC0u) != 0x80u) return 1u;
    }
    return count;
}

static void draw_truncated(int x, int y, const char *src,
                           uint32_t max_chars, uint8_t fg) {
    char text[64];
    uint32_t source_offset = 0;
    uint32_t written = 0;
    uint32_t characters = 0;
    int truncated = 0;

    if (max_chars > 48u) max_chars = 48u;
    while (src[source_offset] != '\0' && characters < max_chars) {
        uint32_t step = utf8_step(src, source_offset);
        uint32_t end = source_offset + step;
        if (written + step >= sizeof(text) - 1u) {
            truncated = 1;
            break;
        }
        for (uint32_t i = 0; i < step; i++) text[written++] = src[source_offset + i];
        source_offset = end;
        characters++;
    }
    if (src[source_offset] != '\0') truncated = 1;
    if (truncated && written < sizeof(text) - 2u) text[written++] = '~';
    text[written] = '\0';

    gfx_draw_utf8(x, y, text, fg, COLOR_TRANSPARENT);
}

void launcher_run(void) {
    int selected = 0;
    int scroll_start = 0;
    const char *status_message = app_registry_external_available()
        ? "Module ready: press I" : "No external module";

    while (1) {
        int app_count = app_registry_get_count();

        /* Обрабатываем события. После изменения каталога перечитываем его ниже. */
        while (keyboard_has_char()) {
            uint8_t key = keyboard_get_char();

            if (key == KEY_UP || key == 'w' || key == 'W') {
                if (app_count > 0) selected = (selected - 1 + app_count) % app_count;
            } else if (key == KEY_DOWN || key == 's' || key == 'S') {
                if (app_count > 0) selected = (selected + 1) % app_count;
            } else if (key == 'i' || key == 'I') {
                int installed = app_registry_install_external();
                if (installed > 0) {
                    status_message = "External apps added";
                    selected = app_registry_get_count() - 1;
                    if (selected < 0) selected = 0;
                    scroll_start = selected >= LIST_VISIBLE_ROWS
                        ? selected - LIST_VISIBLE_ROWS + 1 : 0;
                } else if (installed == 0) status_message = "No new apps imported";
                else if (installed == -1) status_message = "No external package";
                else if (installed == -3) status_message = "No memory for apps";
                else if (installed == -4) status_message = "Registry full";
                else status_message = "Package invalid";
                break;
            } else if (key == 'd' || key == 'D') {
                int removed = app_registry_remove(selected);
                if (removed == 0) status_message = "External app removed";
                else if (removed == -2) status_message = "Built-in app protected";
                else status_message = "No app to remove";
                break;
            } else if (key == '\n') {
                const registered_app_t *app = app_registry_get(selected);
                if (app) {
                    int last_exit_code;
                    last_exit_code = app_registry_launch(selected);
                    (void)last_exit_code;
                    gfx_init();
                    status_message = "Returned to launcher";
                }
                break;
            }
        }

        app_count = app_registry_get_count();
        if (app_count == 0) {
            selected = 0;
            scroll_start = 0;
        } else {
            if (selected >= app_count) selected = app_count - 1;
            if (selected < 0) selected = 0;
            if (selected < scroll_start) scroll_start = selected;
            if (selected >= scroll_start + LIST_VISIBLE_ROWS) {
                scroll_start = selected - LIST_VISIBLE_ROWS + 1;
            }
            if (scroll_start < 0) scroll_start = 0;
        }

        /* Фон и верхняя строка */
        gfx_clear(COLOR_BLACK);
        gfx_fill_rect(0, 0, 320, 16, COLOR_WHITE);
        gfx_draw_text(6, 4, "CustomOS -- App Launcher", COLOR_BLACK, COLOR_TRANSPARENT);

        char uptime_buf[12];
        int_to_dec(timer_get_ticks() / 100u, uptime_buf);
        gfx_draw_text(258, 4, "Up:", COLOR_BLACK, COLOR_TRANSPARENT);
        gfx_draw_text(282, 4, uptime_buf, COLOR_BLACK, COLOR_TRANSPARENT);
        gfx_draw_text(306, 4, "s", COLOR_BLACK, COLOR_TRANSPARENT);

        /* Список приложений с прокруткой, чтобы внешние приложения не уходили за экран. */
        gfx_draw_rect(8, 22, 185, 154, COLOR_LIGHT_GRAY);
        gfx_fill_rect(10, 24, 181, 14, COLOR_DARK_GRAY);
        gfx_draw_text(14, 27, "Applications", COLOR_WHITE, COLOR_TRANSPARENT);

        if (app_count == 0) {
            gfx_draw_text(16, 52, "No applications", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        } else {
            int end = scroll_start + LIST_VISIBLE_ROWS;
            if (end > app_count) end = app_count;

            for (int i = scroll_start; i < end; i++) {
                const registered_app_t *app = app_registry_get(i);
                int row = i - scroll_start;
                int y = 44 + row * 20;
                char number[8];
                char number_text[8];
                int_to_dec((uint32_t)i + 1u, number);
                uint32_t n = 0;
                while (number[n] != '\0' && n < sizeof(number_text) - 3) {
                    number_text[n] = number[n];
                    n++;
                }
                number_text[n++] = '.';
                number_text[n++] = ' ';
                number_text[n] = '\0';

                if (i == selected) {
                    gfx_fill_rect(12, y - 2, 177, 18, COLOR_WHITE);
                    gfx_draw_text(16, y + 2, ">", COLOR_BLACK, COLOR_TRANSPARENT);
                    gfx_draw_text(26, y + 2, number_text, COLOR_BLACK, COLOR_TRANSPARENT);
                    draw_truncated(48, y + 2, app->name, 16, COLOR_BLACK);
                    if (app->removable) gfx_draw_text(176, y + 2, "*", COLOR_BLACK, COLOR_TRANSPARENT);
                } else {
                    gfx_draw_text(26, y + 2, number_text, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
                    draw_truncated(48, y + 2, app->name, 16, COLOR_WHITE);
                    if (app->removable) gfx_draw_text(176, y + 2, "*", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
                }
            }
        }

        /* Информация о выделенном приложении */
        gfx_draw_rect(198, 22, 114, 154, COLOR_LIGHT_GRAY);
        gfx_fill_rect(200, 24, 110, 14, COLOR_DARK_GRAY);
        gfx_draw_text(204, 27, "App details", COLOR_WHITE, COLOR_TRANSPARENT);

        const registered_app_t *app = app_registry_get(selected);
        if (app) {
            gfx_draw_text(204, 46, "Name:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            draw_truncated(204, 58, app->name, 13, COLOR_WHITE);

            gfx_draw_text(204, 76, "Size:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            char size_buf[12];
            int_to_dec(app->size, size_buf);
            draw_truncated(204, 88, size_buf, 12, COLOR_WHITE);
            gfx_draw_text(204, 106, "Source:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            gfx_draw_text(204, 118, app->persistent ? "Disk" :
                          (app->removable ? "External" : "Built-in"),
                          COLOR_WHITE, COLOR_TRANSPARENT);

            gfx_draw_text(204, 138, "D: remove", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            gfx_draw_text(204, 150, app->removable ? "Available" : "Protected",
                          COLOR_WHITE, COLOR_TRANSPARENT);
        }

        draw_truncated(12, 164, status_message, 22, COLOR_WHITE);

        /* Две короткие строки управления умещаются в 320x200. */
        gfx_fill_rect(0, 178, 320, 22, COLOR_DARK_GRAY);
        gfx_draw_line(0, 177, 319, 177, COLOR_WHITE);
        gfx_draw_text(4, 180, "UP/DOWN select  ENTER launch  I import",
                      COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_text(4, 190, "D remove external | I import package",
                      COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

        gfx_flip();
        timer_sleep(20);
    }
}
