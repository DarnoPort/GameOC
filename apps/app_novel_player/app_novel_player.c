#include "customos.h"
#include "vn_engine.h"
#include "vn_package.h"
#include <stddef.h>

#define PLAYER_TEXT_COLOR COLOR_WHITE
#define PLAYER_MUTED_COLOR COLOR_LIGHT_GRAY
#define PLAYER_BACKGROUND COLOR_BLACK
#define PLAYER_MAX_COLUMNS 35u
#define PLAYER_MAX_VISIBLE_LINES 8u

/* Linker places this at the start of .bss. The CBX packer inserts VPK1 here. */
static uint8_t vn_payload_region[VN_RESOURCE_HEADER_SIZE + VN_MAX_SOURCE_BYTES + 1u]
    __attribute__((section(".bss.vn_payload"), used, aligned(4)));
static vn_program_t vn_player_program;
static vn_runtime_t vn_player_runtime;
static vn_event_t vn_player_event;
static vn_error_t vn_player_error;

static uint32_t text_bytes(const char *text) {
    uint32_t length = 0;
    if (text == NULL) return 0;
    while (text[length] != '\0') length++;
    return length;
}

static int is_utf8_continuation(uint8_t value) {
    return (value & 0xC0u) == 0x80u;
}

static uint32_t utf8_next(const char *text, uint32_t offset, uint32_t length) {
    if (offset >= length) return length;
    uint8_t first = (uint8_t)text[offset];
    uint32_t step = first < 0x80u ? 1u :
                    (first >= 0xF0u ? 4u : (first >= 0xE0u ? 3u :
                    (first >= 0xC2u ? 2u : 1u)));
    if (step > length - offset) return offset + 1u;
    for (uint32_t i = 1; i < step; i++) {
        if (!is_utf8_continuation((uint8_t)text[offset + i])) return offset + 1u;
    }
    return offset + step;
}

static uint32_t utf8_codepoints(const char *text, uint32_t length) {
    uint32_t count = 0;
    for (uint32_t pos = 0; pos < length; count++) {
        pos = utf8_next(text, pos, length);
    }
    return count;
}

static void draw_text(const customos_api_t *api, int x, int y,
                      const char *text, uint8_t fg, int bg) {
    if (api->gfx_draw_utf8 != NULL) {
        api->gfx_draw_utf8(x, y, text, fg, bg);
    } else {
        api->gfx_draw_text(x, y, text, fg, bg);
    }
}

static void draw_wrapped(const customos_api_t *api, const char *text,
                         uint32_t length, int x, int y,
                         uint32_t max_columns, uint32_t max_lines) {
    uint32_t offset = 0;
    uint32_t rows = 0;
    char line[PLAYER_MAX_COLUMNS * 4u + 2u];

    while (offset < length && rows < max_lines) {
        uint32_t scan = offset;
        uint32_t codepoints = 0;
        uint32_t last_space_end = 0;
        uint32_t last_space_start = 0;

        while (scan < length && codepoints < max_columns) {
            uint32_t next = utf8_next(text, scan, length);
            if (next == scan) break;
            if (next == scan + 1u && text[scan] == '\n') break;
            if (text[scan] == ' ') {
                last_space_start = scan;
                last_space_end = next;
            }
            scan = next;
            codepoints++;
        }

        uint32_t line_end = scan;
        if (scan < length && last_space_end > offset && last_space_start >= offset) {
            line_end = last_space_start;
        }
        if (line_end == offset) {
            line_end = utf8_next(text, offset, length);
        }

        uint32_t line_length = line_end - offset;
        if (line_length >= sizeof(line)) line_length = sizeof(line) - 1u;
        for (uint32_t i = 0; i < line_length; i++) line[i] = text[offset + i];
        line[line_length] = '\0';
        draw_text(api, x, y + (int)(rows * 10u), line,
                  PLAYER_TEXT_COLOR, COLOR_TRANSPARENT);
        rows++;

        offset = line_end;
        while (offset < length && text[offset] == ' ') offset++;
        if (offset < length && text[offset] == '\n') offset++;
    }
}

static void draw_frame(const customos_api_t *api, const char *status) {
    api->gfx_clear(PLAYER_BACKGROUND);
    api->gfx_fill_rect(0, 0, 320, 20, COLOR_WHITE);
    draw_text(api, 8, 5, "CustomOS / Visual Novel", COLOR_BLACK, COLOR_TRANSPARENT);
    api->gfx_draw_line(0, 22, 319, 22, COLOR_DARK_GRAY);
    api->gfx_draw_line(0, 178, 319, 178, COLOR_DARK_GRAY);
    draw_text(api, 8, 184, status, PLAYER_MUTED_COLOR, COLOR_TRANSPARENT);
}

static uint32_t wait_for_key(const customos_api_t *api) {
    while (1) {
        uint32_t key = api->kbd_wait_codepoint != NULL
            ? api->kbd_wait_codepoint()
            : api->kbd_wait_key();
        if (key != 0 && key != KEY_LAYOUT_TOGGLE && key != KEY_F12) return key;
    }
}

static int draw_error(const customos_api_t *api, const char *message,
                      uint32_t line_number) {
    draw_frame(api, "Press any key to close");
    draw_text(api, 8, 40, "Novel could not start", COLOR_WHITE, COLOR_TRANSPARENT);
    draw_wrapped(api, message, text_bytes(message), 8, 64, 35, 5);
    if (line_number != 0) {
        draw_text(api, 8, 128, "Source line:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        api->print_num((int32_t)line_number);
    }
    api->gfx_flip();
    (void)wait_for_key(api);
    return -1;
}

static void draw_text_event(const customos_api_t *api, const vn_event_t *event) {
    draw_frame(api, "Any key: continue   ESC: stop story");
    draw_wrapped(api, event->text, event->text_length, 12, 42,
                 PLAYER_MAX_COLUMNS, 11);
    api->gfx_flip();
}

static int wait_for_menu_choice(const customos_api_t *api, const vn_event_t *event,
                                uint32_t *choice_out) {
    if (event->choice_count == 0 || event->choices == NULL) return -1;
    uint32_t selected = 0;

    while (1) {
        draw_frame(api, "UP/DOWN: choose   ENTER: select   ESC: stop");
        draw_wrapped(api, "Choose your answer:", 19, 10, 30, PLAYER_MAX_COLUMNS, 1);

        uint32_t first = selected >= 7u ? selected - 6u : 0u;
        uint32_t last = first + PLAYER_MAX_VISIBLE_LINES;
        if (last > event->choice_count) last = event->choice_count;
        for (uint32_t index = first; index < last; index++) {
            int y = 48 + (int)((index - first) * 15u);
            if (index == selected) {
                api->gfx_fill_rect(8, y - 2, 304, 13, COLOR_DARK_GRAY);
                draw_text(api, 12, y, ">", COLOR_WHITE, COLOR_TRANSPARENT);
            }
            const char *title = vn_player_program.strings +
                                event->choices[index].text_offset;
            uint32_t title_length = text_bytes(title);
            draw_wrapped(api, title, title_length,
                         index == selected ? 26 : 16, y, 33, 1);
        }
        api->gfx_flip();

        uint32_t key = wait_for_key(api);
        if (key == 27) return -1;
        if (key == KEY_UP && selected > 0) selected--;
        else if (key == KEY_DOWN && selected + 1u < event->choice_count) selected++;
        else if (key == '\n' || key == ' ' || key == 13u) {
            *choice_out = selected;
            return 0;
        }
    }
}

int app_main(const customos_api_t *api) {
    if (api == NULL || api->gfx_clear == NULL || api->kbd_wait_key == NULL) return -1;

    const char *source = NULL;
    uint32_t source_length = 0;
    int resource_status = vn_resource_validate(vn_payload_region,
        (uint32_t)sizeof(vn_payload_region), &source, &source_length);
    if (resource_status != 0) {
        return draw_error(api, "The game package is missing or damaged.", 0);
    }

    if (!vn_compile(source, source_length, &vn_player_program, &vn_player_error)) {
        return draw_error(api, vn_player_error.message != NULL
            ? vn_player_error.message : "Scenario validation failed.",
            vn_player_error.line);
    }

    if (!vn_runtime_start(&vn_player_runtime, &vn_player_program, NULL,
                          &vn_player_error)) {
        return draw_error(api, vn_player_error.message != NULL
            ? vn_player_error.message : "Scenario runtime could not start.",
            vn_player_error.line);
    }

    while (1) {
        vn_event_type_t type = vn_runtime_next(&vn_player_runtime,
                                                &vn_player_event,
                                                &vn_player_error);
        if (type == VN_EVENT_TEXT) {
            draw_text_event(api, &vn_player_event);
            uint32_t key = wait_for_key(api);
            if (key == 27) {
                draw_frame(api, "Press any key to close");
                draw_text(api, 8, 68, "Story stopped.", COLOR_WHITE, COLOR_TRANSPARENT);
                api->gfx_flip();
                (void)wait_for_key(api);
                return 0;
            }
        } else if (type == VN_EVENT_MENU) {
            uint32_t choice = 0;
            if (wait_for_menu_choice(api, &vn_player_event, &choice) != 0) {
                draw_frame(api, "Press any key to close");
                draw_text(api, 8, 68, "Story stopped.", COLOR_WHITE, COLOR_TRANSPARENT);
                api->gfx_flip();
                (void)wait_for_key(api);
                return 0;
            }
            if (!vn_runtime_choose(&vn_player_runtime, choice, &vn_player_error)) {
                return draw_error(api, vn_player_error.message, vn_player_error.line);
            }
        } else if (type == VN_EVENT_END) {
            draw_frame(api, "Press any key to close");
            draw_text(api, 8, 64, "The story has ended.", COLOR_WHITE, COLOR_TRANSPARENT);
            api->gfx_flip();
            (void)wait_for_key(api);
            return 0;
        } else if (type == VN_EVENT_ERROR) {
            return draw_error(api, vn_player_error.message, vn_player_error.line);
        } else if (type == VN_EVENT_WAITING) {
            (void)wait_for_key(api);
        }
    }
}
