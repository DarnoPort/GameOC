#include "customos.h"

static const char test_name[] = "FS_TEST.DAT";
static const char marker[] = "CustomOS ProjectFS persistence marker v1.";

static int strings_equal(const char *a, const char *b, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static void draw_message(const customos_api_t *api,
                         const char *status,
                         const char *detail) {
    api->gfx_clear(COLOR_BLACK);
    api->gfx_fill_rect(0, 0, 320, 18, COLOR_WHITE);
    api->gfx_draw_text(6, 5, "CustomOS ProjectFS test", COLOR_BLACK, COLOR_TRANSPARENT);

    api->gfx_draw_rect(8, 28, 304, 118, COLOR_LIGHT_GRAY);
    api->gfx_draw_text(18, 40, "Persistent disk / API check", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(18, 62, status, COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(18, 82, detail, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);

    api->gfx_draw_text(8, 158, "SPACE/ESC: exit", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(8, 174, "R: remove marker and start again", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(8, 190, "Reboot QEMU, then run test again", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_flip();
}

static void run_check(const customos_api_t *api) {
    if (!api->fs_available || !api->fs_size || !api->fs_read ||
        !api->fs_write || !api->fs_delete) {
        draw_message(api, "FAIL: API fields missing", "Check CustomOS API version / ABI");
        return;
    }

    if (api->fs_available() <= 0) {
        draw_message(api, "FAIL: ProjectFS unavailable", "Check IDE drive and CFS1 image");
        return;
    }

    int32_t existing_size = api->fs_size(test_name);
    if (existing_size == -2) {
        int result = api->fs_write(test_name, marker, sizeof(marker) - 1);
        if (result == 0) {
            draw_message(api, "MARKER CREATED", "Reboot CustomOS and run this test again");
        } else {
            draw_message(api, "FAIL: cannot write marker", "Disk full or IDE write error");
        }
        return;
    }

    if (existing_size < 0) {
        draw_message(api, "FAIL: fs_size error", "ProjectFS may not be ready");
        return;
    }
    if ((uint32_t)existing_size != sizeof(marker) - 1) {
        draw_message(api, "FAIL: marker size mismatch", "Press R to reset test marker");
        return;
    }

    char buffer[sizeof(marker)];
    int32_t read_size = api->fs_read(test_name, buffer, sizeof(buffer));
    if (read_size != (int32_t)(sizeof(marker) - 1) ||
        !strings_equal(buffer, marker, sizeof(marker) - 1)) {
        draw_message(api, "FAIL: marker CRC/content", "File contents are not as expected");
        return;
    }

    draw_message(api, "PERSISTENCE PASS", "Marker survived; disk and read/write API work");
}

int app_main(const customos_api_t *api) {
    run_check(api);

    while (1) {
        while (api->kbd_has_key()) {
            uint8_t key = api->kbd_get_key();
            if (key == 27 || key == ' ') return 0;
            if (key == 'r' || key == 'R') {
                int32_t result = api->fs_delete(test_name);
                (void)result;
                run_check(api);
            }
        }

        api->timer_sleep(20);
    }
}
