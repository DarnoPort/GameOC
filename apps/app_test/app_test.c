#include "customos.h"

int app_main(const customos_api_t *api) {
    /* 1. Очистка экрана */
    api->gfx_clear(COLOR_BLACK);

    /* 2. Рамка и заголовок окна */
    api->gfx_draw_rect(10, 10, 300, 180, COLOR_WHITE);
    api->gfx_fill_rect(12, 12, 296, 16, COLOR_DARK_GRAY);
    api->gfx_draw_text(18, 16, "CustomOS App: CBX Executable v1.0", COLOR_WHITE, COLOR_TRANSPARENT);

    /* 3. Информационный блок */
    api->gfx_draw_text(20, 38, "Running in user memory at 0x00400000", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(20, 52, "Syscall ABI     : Active & Validated", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(20, 66, "Stack Isolation : 0x004F0000", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(20, 80, "Color Format    : 2-bit (4 colors)", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

    /* 4. Интерактивная рамка */
    api->gfx_draw_rect(20, 100, 280, 54, COLOR_LIGHT_GRAY);
    api->gfx_draw_text(26, 108, "This is an independent CBX app.", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(26, 122, "Press [SPACE] or [ESC] to safely", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(26, 136, "return control to CustomOS kernel.", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

    api->gfx_flip();

    int blink = 0;
    while (1) {
        if (api->kbd_has_key()) {
            uint8_t k = api->kbd_get_key();
            if (k == ' ' || k == 27 || k == '\n') {
                /* Выход из программы */
                break;
            }
        }

        /* Мигающий индикатор активности приложения */
        blink = (blink + 1) % 40;
        uint8_t ind_col = (blink < 20) ? COLOR_WHITE : COLOR_DARK_GRAY;
        api->gfx_fill_rect(280, 16, 8, 8, ind_col);
        api->gfx_flip();

        api->timer_sleep(25);
    }

    /* Возврат кода 0 в ядро */
    return 0;
}