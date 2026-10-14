#include "customos.h"

static void uint_to_dec(uint32_t value, char *out) {
    char reversed[10];
    int count = 0;
    int index = 0;

    do {
        reversed[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < (int)sizeof(reversed));

    while (count > 0) out[index++] = reversed[--count];
    out[index] = '\0';
}

/*
 * This program is intentionally not part of the built-in AppFS.
 * It is compiled to CBX separately and supplied through QEMU -initrd.
 */
int app_main(const customos_api_t *api) {
    uint32_t frames = 0;
    uint8_t text_color = COLOR_WHITE;

    while (1) {
        while (api->kbd_has_key()) {
            uint8_t key = api->kbd_get_key();
            if (key == ' ' || key == 27) return 0;
            if (key == 'c' || key == 'C') {
                text_color = (text_color == COLOR_WHITE)
                    ? COLOR_LIGHT_GRAY : COLOR_WHITE;
            }
        }

        char frame_text[12];
        uint_to_dec(frames, frame_text);

        api->gfx_clear(COLOR_BLACK);
        api->gfx_draw_rect(10, 10, 300, 180, COLOR_LIGHT_GRAY);
        api->gfx_fill_rect(12, 12, 296, 18, COLOR_DARK_GRAY);
        api->gfx_draw_text(22, 18, "EXTERNAL APP TRANSFER TEST",
                           COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_text(24, 48, "This program was not linked into the kernel.",
                           text_color, COLOR_TRANSPARENT);
        api->gfx_draw_text(24, 66, "It was supplied as a separate CBX/AppFS module.",
                           text_color, COLOR_TRANSPARENT);
        api->gfx_draw_text(24, 92, "Frames:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        api->gfx_draw_text(88, 92, frame_text, COLOR_WHITE, COLOR_TRANSPARENT);

        api->gfx_draw_rect(20, 120, 280, 42, COLOR_DARK_GRAY);
        api->gfx_draw_text(28, 130, "Press [C] to change text contrast.",
                           COLOR_WHITE, COLOR_TRANSPARENT);
        api->gfx_draw_text(28, 144, "Press [SPACE] or [ESC] to return.",
                           COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

        api->gfx_flip();
        api->timer_sleep(100);
        frames++;
    }
}
