#include "customos.h"

int app_main(const customos_api_t *api) {
    api->gfx_clear(COLOR_BLACK);

    /* Рамка и заголовок */
    api->gfx_draw_rect(6, 6, 308, 188, COLOR_LIGHT_GRAY);
    api->gfx_fill_rect(8, 8, 304, 16, COLOR_DARK_GRAY);
    api->gfx_draw_text(14, 12, "CustomOS - System Diagnostics (SysInfo)", COLOR_WHITE, COLOR_TRANSPARENT);

    /* Системные данные */
    api->gfx_draw_text(14, 32, "OS Kernel       : CustomOS v0.1 (x86 32-bit)", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(14, 46, "Video Subsystem : 320x200 2 bpp (4 Colors)", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(14, 60, "Binary Format   : CBX1 Executable Module", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(14, 74, "Application RAM : 0x00400000 - 0x004EFFFF", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(14, 88, "Storage System  : AppFS Virtual Container", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

    /* Образцы палитры */
    api->gfx_draw_text(14, 108, "Palette [2 bpp]:", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_fill_rect(130, 106, 30, 12, COLOR_BLACK);
    api->gfx_draw_rect(130, 106, 30, 12, COLOR_WHITE);
    api->gfx_fill_rect(170, 106, 30, 12, COLOR_DARK_GRAY);
    api->gfx_fill_rect(210, 106, 30, 12, COLOR_LIGHT_GRAY);
    api->gfx_fill_rect(250, 106, 30, 12, COLOR_WHITE);

    /* Инструкция выхода */
    api->gfx_draw_rect(14, 132, 292, 46, COLOR_WHITE);
    api->gfx_draw_text(22, 140, "Diagnostic module executed successfully.", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(22, 154, "Press [SPACE] or [ESC] to return to Launcher.", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

    api->gfx_flip();

    while (1) {
        if (api->kbd_has_key()) {
            uint8_t k = api->kbd_get_key();
            if (k == ' ' || k == 27 || k == '\n') {
                break;
            }
        }
        api->timer_sleep(25);
    }

    return 0;
}