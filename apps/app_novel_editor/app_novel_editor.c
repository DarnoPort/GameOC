#include "customos.h"
#include "vn_engine.h"
#include "vn_package.h"
#include "novel_player_template.h"
#include <stddef.h>

#define EDITOR_TITLE_BYTES 64u
#define EDITOR_VISIBLE_ROWS 14u
#define EDITOR_VISIBLE_COLUMNS 36u
#define EDITOR_LINE_BUFFER 160u
#define EDITOR_MAX_PROJECTS 64u
#define PROJECT_MAGIC 0x4A50564Eu /* "NVPJ" in little-endian */
#define PROJECT_VERSION 1u
#define LAST_PROJECT_FILE "LASTVN.DAT"

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t id;
    char title[EDITOR_TITLE_BYTES];
} project_metadata_t;

static const char default_script[] =
    "label начало:\n"
    "    Добро пожаловать в мою новеллу;\n"
    "    Перед тобой две дороги;\n"
    "    menu:\n"
    "        Остаться дома:\n"
    "            Сегодня будет спокойный день;\n"
    "            return;\n"
    "        endchoice;\n"
    "        Отправиться в путь:\n"
    "            Ты выходишь навстречу приключениям;\n"
    "            jump лес;\n"
    "        endchoice;\n"
    "    endmenu;\n"
    "    return;\n"
    "\n"
    "label лес:\n"
    "    Впереди шумит древний лес;\n"
    "    return;\n";

static const char *const main_items[] = {
    "Новый проект",
    "Открыть проект",
    "Продолжить",
    "Справка",
    "Выход в CustomOS"
};
#define MAIN_ITEM_COUNT ((uint32_t)(sizeof(main_items) / sizeof(main_items[0])))

static const customos_api_t *os_api;
static char source_buffer[VN_MAX_SOURCE_BYTES + 1u];
static char output_buffer[VN_MAX_CBX_FILE_BYTES];
static vn_program_t compiled_program;
static vn_runtime_t test_runtime;
static vn_event_t test_event;
static vn_error_t compile_error;
static project_metadata_t current_metadata;
static uint32_t source_length;
static uint32_t cursor_offset;
static uint32_t view_top_line;
static uint32_t project_counter;
static uint8_t project_dirty;
static uint8_t has_project;
static uint8_t should_exit;
static char current_source_file[20];
static char current_metadata_file[20];
static char current_game_file[20];
static char status_text[80];

static uint32_t text_length(const char *text) {
    uint32_t length = 0;
    if (text == NULL) return 0;
    while (text[length] != '\0') length++;
    return length;
}

static void copy_text(char *destination, uint32_t capacity, const char *source) {
    if (capacity == 0) return;
    uint32_t i = 0;
    if (source != NULL) {
        while (i + 1u < capacity && source[i] != '\0') {
            destination[i] = source[i];
            i++;
        }
    }
    destination[i] = '\0';
}

static int text_equal(const char *a, const char *b) {
    uint32_t i = 0;
    if (a == NULL || b == NULL) return 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int is_continuation(uint8_t byte) {
    return (byte & 0xC0u) == 0x80u;
}

static uint32_t utf8_next(const char *text, uint32_t offset, uint32_t length) {
    if (offset >= length) return length;
    uint8_t first = (uint8_t)text[offset];
    uint32_t count = first < 0x80u ? 1u :
        (first >= 0xF0u ? 4u : (first >= 0xE0u ? 3u :
        (first >= 0xC2u ? 2u : 1u)));
    if (count > length - offset) return offset + 1u;
    for (uint32_t i = 1; i < count; i++) {
        if (!is_continuation((uint8_t)text[offset + i])) return offset + 1u;
    }
    return offset + count;
}

static uint32_t utf8_previous(const char *text, uint32_t offset) {
    if (offset == 0) return 0;
    offset--;
    while (offset > 0 && is_continuation((uint8_t)text[offset])) offset--;
    return offset;
}

static uint32_t utf8_encode(uint32_t cp, char output[4]) {
    if (cp <= 0x7Fu) {
        output[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FFu) {
        output[0] = (char)(0xC0u | (cp >> 6));
        output[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp >= 0xD800u && cp <= 0xDFFFu) return 0;
    if (cp <= 0xFFFFu) {
        output[0] = (char)(0xE0u | (cp >> 12));
        output[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        output[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    if (cp <= 0x10FFFFu) {
        output[0] = (char)(0xF0u | (cp >> 18));
        output[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        output[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        output[3] = (char)(0x80u | (cp & 0x3Fu));
        return 4;
    }
    return 0;
}

static uint32_t line_start(uint32_t offset) {
    if (offset > source_length) offset = source_length;
    while (offset > 0 && source_buffer[offset - 1u] != '\n') offset--;
    return offset;
}

static uint32_t line_end(uint32_t offset) {
    while (offset < source_length && source_buffer[offset] != '\n') offset++;
    return offset;
}

static uint32_t line_number_at(uint32_t offset) {
    uint32_t lines = 0;
    for (uint32_t i = 0; i < offset && i < source_length; i++) {
        if (source_buffer[i] == '\n') lines++;
    }
    return lines;
}

static uint32_t codepoints_between(uint32_t start, uint32_t end) {
    uint32_t count = 0;
    while (start < end) {
        uint32_t next = utf8_next(source_buffer, start, end);
        if (next <= start) break;
        start = next;
        count++;
    }
    return count;
}

static uint32_t offset_at_column(uint32_t start, uint32_t end, uint32_t column) {
    while (start < end && column > 0) {
        start = utf8_next(source_buffer, start, end);
        column--;
    }
    return start;
}

static void make_project_filename(char prefix, uint32_t id, const char *extension,
                                  char output[20]) {
    static const char hex[] = "0123456789ABCDEF";
    output[0] = prefix;
    output[1] = '_';
    for (uint32_t i = 0; i < 8; i++) {
        output[2u + i] = hex[(id >> ((7u - i) * 4u)) & 0x0Fu];
    }
    output[10] = '.';
    output[11] = extension[0];
    output[12] = extension[1];
    output[13] = extension[2];
    output[14] = '\0';
}

static int parse_project_id(const char *filename, char expected_prefix,
                            const char *extension, uint32_t *id_out) {
    if (filename == NULL || id_out == NULL ||
        filename[0] != expected_prefix || filename[1] != '_' ||
        filename[10] != '.' || filename[11] != extension[0] ||
        filename[12] != extension[1] || filename[13] != extension[2] ||
        filename[14] != '\0') return 0;

    uint32_t id = 0;
    for (uint32_t i = 2; i < 10; i++) {
        char ch = filename[i];
        uint32_t value;
        if (ch >= '0' && ch <= '9') value = (uint32_t)(ch - '0');
        else if (ch >= 'A' && ch <= 'F') value = (uint32_t)(ch - 'A' + 10);
        else if (ch >= 'a' && ch <= 'f') value = (uint32_t)(ch - 'a' + 10);
        else return 0;
        id = (id << 4) | value;
    }
    *id_out = id;
    return 1;
}


static void format_line_error(char *out, uint32_t capacity, uint32_t line,
                              const char *message) {
    if (capacity == 0) return;
    uint32_t at = 0;
    const char prefix[] = "Line ";
    for (uint32_t i = 0; prefix[i] != '\0' && at + 1u < capacity; i++) out[at++] = prefix[i];
    char reversed[12];
    uint32_t digits = 0;
    do {
        reversed[digits++] = (char)('0' + line % 10u);
        line /= 10u;
    } while (line > 0 && digits < sizeof(reversed));
    while (digits > 0 && at + 1u < capacity) out[at++] = reversed[--digits];
    if (at + 2u < capacity) {
        out[at++] = ':';
        out[at++] = ' ';
    }
    if (message != NULL) {
        for (uint32_t i = 0; message[i] != '\0' && at + 1u < capacity; i++) out[at++] = message[i];
    }
    out[at] = '\0';
}

static void set_status(const char *message) {
    copy_text(status_text, sizeof(status_text), message);
}

static void draw_text(const char *text, int x, int y, uint8_t fg, int bg) {
    if (os_api->gfx_draw_utf8 != NULL) {
        os_api->gfx_draw_utf8(x, y, text, fg, bg);
    } else {
        os_api->gfx_draw_text(x, y, text, fg, bg);
    }
}

static void draw_frame(const char *title, const char *hint) {
    os_api->gfx_clear(COLOR_BLACK);
    os_api->gfx_fill_rect(0, 0, 320, 18, COLOR_WHITE);
    draw_text(title, 5, 5, COLOR_BLACK, COLOR_TRANSPARENT);
    os_api->gfx_draw_line(0, 178, 319, 178, COLOR_DARK_GRAY);
    draw_text(hint, 5, 184, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
}

static uint32_t next_key(void) {
    while (1) {
        uint32_t key = os_api->kbd_wait_codepoint != NULL
            ? os_api->kbd_wait_codepoint() : os_api->kbd_wait_key();
        if (key == KEY_F12) continue;
        return key;
    }
}

static void draw_wrapped(const char *text, uint32_t length, int x, int y,
                         uint32_t max_columns, uint32_t max_lines) {
    uint32_t pos = 0;
    uint32_t row = 0;
    char buffer[160];

    while (pos < length && row < max_lines) {
        uint32_t end = pos;
        uint32_t count = 0;
        uint32_t last_space = 0;
        while (end < length && count < max_columns && text[end] != '\n') {
            uint32_t next = utf8_next(text, end, length);
            if (next <= end) break;
            if (text[end] == ' ') last_space = end;
            end = next;
            count++;
        }
        if (end < length && text[end] != '\n' && last_space > pos) end = last_space;
        if (end == pos) end = utf8_next(text, pos, length);
        uint32_t bytes = end - pos;
        if (bytes >= sizeof(buffer)) bytes = sizeof(buffer) - 1u;
        for (uint32_t i = 0; i < bytes; i++) buffer[i] = text[pos + i];
        buffer[bytes] = '\0';
        draw_text(buffer, x, y + (int)(row * 10u), COLOR_WHITE, COLOR_TRANSPARENT);
        row++;
        pos = end;
        while (pos < length && text[pos] == ' ') pos++;
        if (pos < length && text[pos] == '\n') pos++;
    }
}

static void show_message(const char *title, const char *message) {
    draw_frame(title, "Press any key to continue");
    draw_wrapped(message, text_length(message), 8, 40, 36, 12);
    os_api->gfx_flip();
    (void)next_key();
}

static void show_help(void) {
    static const char help[] =
        "F2 - save project\n"
        "F3 - validate script\n"
        "F4 - test run without build\n"
        "F5 - compile/install standalone CBX\n"
        "F12 - switch EN/RU keyboard layout\n"
        "Arrows move cursor; Home/End move in line.\n"
        "Backspace/Delete edit UTF-8 characters.\n"
        "Script commands end with semicolon.\n"
        "menu: needs choice headers, endchoice; and endmenu;.\n"
        "Press Esc to return to menu.";
    show_message("Visual Novel Studio - Help", help);
}

static int read_last_id(uint32_t *id_out) {
    char last[16];
    int32_t size = os_api->fs_read(LAST_PROJECT_FILE, last, sizeof(last));
    if (size != 8) return 0;
    uint32_t id = 0;
    for (uint32_t i = 0; i < 8; i++) {
        char ch = last[i];
        uint32_t value;
        if (ch >= '0' && ch <= '9') value = (uint32_t)(ch - '0');
        else if (ch >= 'A' && ch <= 'F') value = (uint32_t)(ch - 'A' + 10);
        else if (ch >= 'a' && ch <= 'f') value = (uint32_t)(ch - 'a' + 10);
        else return 0;
        id = (id << 4) | value;
    }
    *id_out = id;
    return 1;
}

static void save_last_id(void) {
    static const char hex[] = "0123456789ABCDEF";
    char last[8];
    for (uint32_t i = 0; i < 8; i++) {
        last[i] = hex[(current_metadata.id >> ((7u - i) * 4u)) & 0x0Fu];
    }
    (void)os_api->fs_write(LAST_PROJECT_FILE, last, sizeof(last));
}

static int load_project_by_id(uint32_t id) {
    project_metadata_t metadata;
    char meta_filename[20];
    char script_filename[20];
    char game_filename[20];
    make_project_filename('P', id, "MET", meta_filename);
    make_project_filename('P', id, "SCR", script_filename);
    make_project_filename('G', id, "CBX", game_filename);

    int32_t metadata_size = os_api->fs_read(meta_filename, &metadata, sizeof(metadata));
    if (metadata_size != (int32_t)sizeof(metadata) ||
        metadata.magic != PROJECT_MAGIC || metadata.version != PROJECT_VERSION ||
        metadata.id != id) return 0;

    int32_t script_size = os_api->fs_read(script_filename, source_buffer,
                                          VN_MAX_SOURCE_BYTES);
    if (script_size < 0 || (uint32_t)script_size > VN_MAX_SOURCE_BYTES) return 0;
    source_length = (uint32_t)script_size;
    source_buffer[source_length] = '\0';
    metadata.title[EDITOR_TITLE_BYTES - 1u] = '\0';

    current_metadata = metadata;
    copy_text(current_source_file, sizeof(current_source_file), script_filename);
    copy_text(current_metadata_file, sizeof(current_metadata_file), meta_filename);
    copy_text(current_game_file, sizeof(current_game_file), game_filename);
    cursor_offset = 0;
    view_top_line = 0;
    has_project = 1;
    project_dirty = 0;
    save_last_id();
    set_status("Project opened.");
    return 1;
}

static int save_project(void) {
    if (!has_project) {
        set_status("Create or open a project first.");
        return 0;
    }
    if (source_length > VN_MAX_SOURCE_BYTES) {
        set_status("Script exceeds the 48 KiB source limit.");
        return 0;
    }

    /* Each ProjectFS replacement is committed only after write/read-back CRC validation. */
    if (os_api->fs_write(current_source_file, source_buffer, source_length) != 0) {
        set_status("Could not save script. Disk may be full.");
        return 0;
    }
    if (os_api->fs_write(current_metadata_file, &current_metadata,
                         sizeof(current_metadata)) != 0) {
        set_status("Script saved, but project metadata write failed.");
        return 0;
    }

    project_dirty = 0;
    save_last_id();
    set_status("Project saved.");
    return 1;
}

static int create_project(void) {
    char title[EDITOR_TITLE_BYTES];
    title[0] = '\0';
    draw_frame("Create a new project", "Type name; Enter accepts, Esc cancels");
    draw_text("Project name:", 8, 40, COLOR_WHITE, COLOR_TRANSPARENT);
    os_api->gfx_flip();

    uint32_t length = 0;
    while (1) {
        uint32_t key = next_key();
        if (key == 27) return 0;
        if (key == KEY_LAYOUT_TOGGLE) {
            os_api->gfx_fill_rect(8, 84, 304, 12, COLOR_BLACK);
            draw_text(os_api->kbd_russian_layout != NULL && os_api->kbd_russian_layout()
                ? "Keyboard layout: RU" : "Keyboard layout: EN",
                8, 86, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
            os_api->gfx_flip();
            continue;
        }
        if (key == '\n' || key == 13u) break;
        if (key == '\b' && length > 0) {
            uint32_t pos = utf8_previous(title, length);
            length = pos;
            title[length] = '\0';
        } else if (key >= 32u && key <= 0x10FFFFu) {
            char encoded[4];
            uint32_t encoded_length = utf8_encode(key, encoded);
            if (encoded_length > 0 && length + encoded_length < sizeof(title) - 1u) {
                for (uint32_t i = 0; i < encoded_length; i++) title[length++] = encoded[i];
                title[length] = '\0';
            }
        }

        os_api->gfx_fill_rect(8, 62, 304, 16, COLOR_BLACK);
        draw_text(title, 8, 66, COLOR_WHITE, COLOR_TRANSPARENT);
        os_api->gfx_flip();
    }
    if (length == 0) {
        show_message("Create project", "Project name cannot be empty.");
        return 0;
    }

    uint32_t id = 0;
    int found = 0;
    for (uint32_t attempt = 0; attempt < 1024u; attempt++) {
        id = os_api->timer_get_ticks() * 2654435761u ^
             (0x9E3779B9u + (++project_counter * 2246822519u));
        char meta_filename[20];
        make_project_filename('P', id, "MET", meta_filename);
        if (os_api->fs_size(meta_filename) == -2) {
            found = 1;
            break;
        }
    }
    if (!found) {
        show_message("Create project", "Could not allocate a unique project ID.");
        return 0;
    }

    current_metadata.magic = PROJECT_MAGIC;
    current_metadata.version = PROJECT_VERSION;
    current_metadata.id = id;
    for (uint32_t i = 0; i < EDITOR_TITLE_BYTES; i++) current_metadata.title[i] = 0;
    copy_text(current_metadata.title, sizeof(current_metadata.title), title);

    make_project_filename('P', id, "MET", current_metadata_file);
    make_project_filename('P', id, "SCR", current_source_file);
    make_project_filename('G', id, "CBX", current_game_file);
    source_length = (uint32_t)(sizeof(default_script) - 1u);
    for (uint32_t i = 0; i < source_length; i++) source_buffer[i] = default_script[i];
    source_buffer[source_length] = '\0';
    cursor_offset = 0;
    view_top_line = 0;
    has_project = 1;
    project_dirty = 1;

    if (!save_project()) {
        has_project = 0;
        show_message("Create project", "Initial project files could not be saved.");
        return 0;
    }
    set_status("New project created.");
    return 1;
}

typedef struct {
    uint32_t id;
    char title[EDITOR_TITLE_BYTES];
} project_list_item_t;

static int list_projects(project_list_item_t items[EDITOR_MAX_PROJECTS]) {
    int count = 0;
    int32_t file_count = os_api->fs_count();
    if (file_count < 0) return 0;

    for (int32_t i = 0; i < file_count && count < (int)EDITOR_MAX_PROJECTS; i++) {
        char filename[32];
        if (os_api->fs_name_at((uint32_t)i, filename, sizeof(filename)) != 0) continue;
        uint32_t id = 0;
        if (!parse_project_id(filename, 'P', "MET", &id)) continue;

        project_metadata_t metadata;
        if (os_api->fs_read(filename, &metadata, sizeof(metadata)) != (int32_t)sizeof(metadata) ||
            metadata.magic != PROJECT_MAGIC || metadata.version != PROJECT_VERSION ||
            metadata.id != id) continue;
        metadata.title[EDITOR_TITLE_BYTES - 1u] = '\0';
        items[count].id = id;
        copy_text(items[count].title, sizeof(items[count].title), metadata.title);
        count++;
    }
    return count;
}

static int open_project_menu(void) {
    project_list_item_t items[EDITOR_MAX_PROJECTS];
    int count = list_projects(items);
    if (count <= 0) {
        show_message("Open project", "No saved projects found on the CustomOS disk.");
        return 0;
    }

    int selected = 0;
    int scroll = 0;
    while (1) {
        draw_frame("Open project", "UP/DOWN browse; Enter open; Esc cancel");
        if (selected < scroll) scroll = selected;
        if (selected >= scroll + 13) scroll = selected - 12;
        for (int i = scroll; i < count && i < scroll + 13; i++) {
            int y = 28 + (i - scroll) * 11;
            if (i == selected) os_api->gfx_fill_rect(4, y - 2, 312, 10, COLOR_DARK_GRAY);
            draw_text(items[i].title, 8, y, i == selected ? COLOR_WHITE : COLOR_LIGHT_GRAY,
                      COLOR_TRANSPARENT);
        }
        os_api->gfx_flip();
        uint32_t key = next_key();
        if (key == 27) return 0;
        if (key == KEY_UP && selected > 0) selected--;
        else if (key == KEY_DOWN && selected + 1 < count) selected++;
        else if (key == '\n' || key == 13u) {
            if (load_project_by_id(items[selected].id)) return 1;
            show_message("Open project", "Could not read that project's files.");
            return 0;
        }
    }
}

static int load_continue_project(void) {
    uint32_t id = 0;
    if (!read_last_id(&id)) {
        show_message("Continue editing", "There is no recent project yet.");
        return 0;
    }
    if (load_project_by_id(id)) return 1;
    show_message("Continue editing", "The recent project could not be opened.");
    return 0;
}

static void draw_editor(void) {
    uint32_t cursor_line = line_number_at(cursor_offset);
    uint32_t current_line_start = line_start(cursor_offset);
    uint32_t cursor_column = codepoints_between(current_line_start, cursor_offset);
    uint32_t horizontal = cursor_column >= EDITOR_VISIBLE_COLUMNS
        ? cursor_column - EDITOR_VISIBLE_COLUMNS + 1u : 0u;

    if (cursor_line < view_top_line) view_top_line = cursor_line;
    if (cursor_line >= view_top_line + EDITOR_VISIBLE_ROWS) {
        view_top_line = cursor_line - EDITOR_VISIBLE_ROWS + 1u;
    }

    draw_frame(has_project ? current_metadata.title : "Visual Novel Studio",
               "F2 save F3 check F4 test F5 build Esc menu");
    os_api->gfx_fill_rect(0, 20, 320, 156, COLOR_BLACK);

    uint32_t line_offset = 0;
    uint32_t logical_line = 0;
    uint32_t row = 0;
    uint32_t start = 0;
    while (logical_line < view_top_line && start < source_length) {
        if (source_buffer[start++] == '\n') logical_line++;
    }
    line_offset = start;

    while (row < EDITOR_VISIBLE_ROWS) {
        uint32_t end = line_end(line_offset);
        if (line_offset > source_length) break;

        char number[8];
        uint32_t number_value = logical_line + 1u;
        char reverse[8];
        uint32_t digits = 0;
        do {
            reverse[digits++] = (char)('0' + number_value % 10u);
            number_value /= 10u;
        } while (number_value > 0 && digits < sizeof(reverse));
        for (uint32_t i = 0; i < digits; i++) number[i] = reverse[digits - i - 1u];
        number[digits] = '\0';

        int y = 25 + (int)(row * 10u);
        draw_text(number, 2, y, COLOR_DARK_GRAY, COLOR_TRANSPARENT);
        uint32_t visible_start = offset_at_column(line_offset, end, horizontal);
        uint32_t visible_end = visible_start;
        for (uint32_t columns = 0; visible_end < end && columns < EDITOR_VISIBLE_COLUMNS; columns++) {
            visible_end = utf8_next(source_buffer, visible_end, end);
        }
        uint32_t byte_count = visible_end - visible_start;
        char line_buffer[EDITOR_LINE_BUFFER];
        if (byte_count >= sizeof(line_buffer)) byte_count = sizeof(line_buffer) - 1u;
        for (uint32_t i = 0; i < byte_count; i++) line_buffer[i] = source_buffer[visible_start + i];
        line_buffer[byte_count] = '\0';
        draw_text(line_buffer, 27, y, COLOR_WHITE, COLOR_TRANSPARENT);

        if (logical_line == cursor_line) {
            uint32_t visible_column = cursor_column >= horizontal ? cursor_column - horizontal : 0;
            if (visible_column < EDITOR_VISIBLE_COLUMNS) {
                int x = 27 + (int)(visible_column * 8u);
                os_api->gfx_draw_rect(x, y - 1, 8, 9, COLOR_LIGHT_GRAY);
            }
        }

        row++;
        if (end >= source_length) break;
        line_offset = end + 1u;
        logical_line++;
    }

    char info[80];
    uint32_t column = codepoints_between(line_start(cursor_offset), cursor_offset) + 1u;
    uint32_t lineno = cursor_line + 1u;
    (void)column;
    (void)lineno;
    copy_text(info, sizeof(info), status_text);
    draw_text(info, 4, 164, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    if (project_dirty) draw_text("*", 305, 5, COLOR_BLACK, COLOR_TRANSPARENT);

    os_api->gfx_flip();
}

static int insert_bytes(uint32_t offset, const char *bytes, uint32_t count) {
    if (count > VN_MAX_SOURCE_BYTES - source_length) return 0;
    for (uint32_t i = source_length; i > offset; i--) {
        source_buffer[i + count - 1u] = source_buffer[i - 1u];
    }
    for (uint32_t i = 0; i < count; i++) source_buffer[offset + i] = bytes[i];
    source_length += count;
    source_buffer[source_length] = '\0';
    cursor_offset += count;
    project_dirty = 1;
    return 1;
}

static void delete_range(uint32_t start, uint32_t end) {
    if (start >= end || end > source_length) return;
    uint32_t count = end - start;
    for (uint32_t i = start; i + count <= source_length; i++) {
        source_buffer[i] = source_buffer[i + count];
    }
    source_length -= count;
    source_buffer[source_length] = '\0';
    cursor_offset = start;
    project_dirty = 1;
}

static void ensure_save(void) {
    if (project_dirty) {
        show_message("Unsaved changes", "Save with F2 before leaving this project.");
    }
}

static void editor_test_run(void) {
    if (!vn_compile(source_buffer, source_length, &compiled_program, &compile_error)) {
        set_status("Check failed: fix the error shown.");
        char message[160];
        format_line_error(message, sizeof(message), compile_error.line, compile_error.message);
        show_message("Ошибка сценария", message);
        return;
    }
    if (!vn_runtime_start(&test_runtime, &compiled_program, NULL, &compile_error)) {
        show_message("Test run", "The compiled scenario could not be started.");
        return;
    }

    while (1) {
        vn_event_type_t type = vn_runtime_next(&test_runtime, &test_event, &compile_error);
        if (type == VN_EVENT_TEXT) {
            draw_frame("Test Run", "Any key continues; Esc ends test");
            draw_wrapped(test_event.text, test_event.text_length, 8, 44, 36, 11);
            os_api->gfx_flip();
            uint32_t key = next_key();
            if (key == 27) break;
        } else if (type == VN_EVENT_MENU) {
            uint32_t selected = 0;
            while (1) {
                draw_frame("Test Run - choose", "UP/DOWN select; Enter accepts; Esc ends test");
                draw_text("Choose an option:", 8, 28, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
                uint32_t first = selected > 9u ? selected - 9u : 0u;
                for (uint32_t i = first; i < test_event.choice_count && i < first + 10u; i++) {
                    int y = 43 + (int)((i - first) * 12u);
                    if (i == selected) os_api->gfx_fill_rect(5, y - 2, 310, 11, COLOR_DARK_GRAY);
                    const char *title = compiled_program.strings + test_event.choices[i].text_offset;
                    draw_wrapped(title, text_length(title), 10, y, 36, 1);
                }
                os_api->gfx_flip();
                uint32_t key = next_key();
                if (key == 27) goto test_finished;
                if (key == KEY_UP && selected > 0) selected--;
                else if (key == KEY_DOWN && selected + 1u < test_event.choice_count) selected++;
                else if (key == '\n' || key == 13u) break;
            }
            if (!vn_runtime_choose(&test_runtime, selected, &compile_error)) break;
        } else if (type == VN_EVENT_END) {
            show_message("Test Run", "End of story. Press a key to return to the editor.");
            break;
        } else if (type == VN_EVENT_ERROR) {
            show_message("Test Run", compile_error.message != NULL ? compile_error.message : "Runtime error.");
            break;
        } else if (type == VN_EVENT_WAITING) {
            (void)next_key();
        }
    }

test_finished:
    set_status("Returned from test run.");
}

static void editor_compile_project(void) {
    if (!has_project) return;
    if (!vn_compile(source_buffer, source_length, &compiled_program, &compile_error)) {
        char message[160];
        format_line_error(message, sizeof(message), compile_error.line, compile_error.message);
        show_message("Ошибка сборки", message);
        set_status("Build failed; previous game was not changed.");
        return;
    }

    uint32_t output_size = 0;
    int packed = vn_pack_player_cbx(
        vn_player_template, VN_PLAYER_TEMPLATE_SIZE,
        source_buffer, source_length,
        current_metadata.title, "CustomOS Visual Novel",
        (uint8_t *)output_buffer, sizeof(output_buffer), &output_size);
    if (packed != 0 || output_size == 0 || output_size > sizeof(output_buffer)) {
        show_message("Build failed", "The player template or source exceeds the CBX build limits.");
        set_status("Build failed; previous game was not changed.");
        return;
    }

    /* ProjectFS commits the directory only after writing and CRC-verifying a new slot. */
    if (os_api->fs_write(current_game_file, output_buffer, output_size) != 0) {
        show_message("Build failed", "Could not commit the new CBX. The prior build is preserved.");
        set_status("Build failed; previous game was not changed.");
        return;
    }

    if (os_api->app_install_persistent == NULL) {
        show_message("Build warning", "Game saved. The launcher API is unavailable; restart OS to reload apps.");
        set_status("Build saved; restart OS to register the game.");
        return;
    }
    int32_t installed = os_api->app_install_persistent(current_game_file);
    if (installed > 0) {
        show_message("Build successful", "Standalone game saved and added/updated in the CustomOS launcher.");
        set_status("Build successful; launcher entry updated.");
    } else {
        show_message("Build saved", "CBX was saved, but current registry could not reload it. Restart OS after freeing app space.");
        set_status("Build saved; registration will retry on boot.");
    }
}

static void editor_loop(void) {
    while (has_project && !should_exit) {
        draw_editor();
        uint32_t key = next_key();

        if (key == KEY_LAYOUT_TOGGLE) {
            set_status(os_api->kbd_russian_layout != NULL &&
                os_api->kbd_russian_layout() ? "Keyboard layout: RU (F12 switches)" :
                                               "Keyboard layout: EN (F12 switches)");
            continue;
        }
        if (key == 27) {
            if (project_dirty) {
                show_message("Unsaved changes", "F2 saves. Press Esc again to leave without saving.");
                uint32_t decision = next_key();
                if (decision == KEY_F2) (void)save_project();
                else if (decision != 27) continue;
            }
            break;
        }
        if (key == KEY_F2) {
            (void)save_project();
            continue;
        }
        if (key == KEY_F3) {
            if (vn_compile(source_buffer, source_length, &compiled_program, &compile_error)) {
                set_status("Scenario is valid.");
            } else {
                char message[160];
                format_line_error(message, sizeof(message), compile_error.line, compile_error.message);
                show_message("Ошибка проверки", message);
                set_status("Scenario validation failed.");
            }
            continue;
        }
        if (key == KEY_F4) {
            editor_test_run();
            continue;
        }
        if (key == KEY_F5) {
            editor_compile_project();
            continue;
        }

        if (key == KEY_LEFT) {
            cursor_offset = utf8_previous(source_buffer, cursor_offset);
            continue;
        }
        if (key == KEY_RIGHT) {
            cursor_offset = utf8_next(source_buffer, cursor_offset, source_length);
            continue;
        }
        if (key == KEY_HOME) {
            cursor_offset = line_start(cursor_offset);
            continue;
        }
        if (key == KEY_END) {
            cursor_offset = line_end(cursor_offset);
            continue;
        }
        if (key == KEY_UP || key == KEY_DOWN) {
            uint32_t old_start = line_start(cursor_offset);
            uint32_t old_column = codepoints_between(old_start, cursor_offset);
            if (key == KEY_UP && old_start > 0) {
                uint32_t previous_end = old_start - 1u;
                uint32_t previous_start = line_start(previous_end);
                uint32_t previous_column = old_column;
                cursor_offset = offset_at_column(previous_start, previous_end, previous_column);
            } else if (key == KEY_DOWN) {
                uint32_t current_end = line_end(cursor_offset);
                if (current_end < source_length) {
                    uint32_t next_start = current_end + 1u;
                    uint32_t next_end = line_end(next_start);
                    cursor_offset = offset_at_column(next_start, next_end, old_column);
                }
            }
            continue;
        }
        if (key == KEY_PAGE_UP) {
            for (uint32_t i = 0; i < EDITOR_VISIBLE_ROWS && cursor_offset > 0; i++) {
                uint32_t start = line_start(cursor_offset);
                if (start == 0) { cursor_offset = 0; break; }
                uint32_t previous_end = start - 1u;
                cursor_offset = offset_at_column(line_start(previous_end), previous_end,
                    codepoints_between(start, cursor_offset));
            }
            continue;
        }
        if (key == KEY_PAGE_DOWN) {
            for (uint32_t i = 0; i < EDITOR_VISIBLE_ROWS && cursor_offset < source_length; i++) {
                uint32_t start = line_start(cursor_offset);
                uint32_t end = line_end(cursor_offset);
                if (end >= source_length) { cursor_offset = source_length; break; }
                uint32_t column = codepoints_between(start, cursor_offset);
                cursor_offset = offset_at_column(end + 1u, line_end(end + 1u), column);
            }
            continue;
        }
        if (key == '\b') {
            if (cursor_offset > 0) delete_range(utf8_previous(source_buffer, cursor_offset), cursor_offset);
            continue;
        }
        if (key == KEY_DELETE) {
            if (cursor_offset < source_length) delete_range(cursor_offset,
                utf8_next(source_buffer, cursor_offset, source_length));
            continue;
        }
        if (key == '\n' || key == 13u) {
            const char newline = '\n';
            if (!insert_bytes(cursor_offset, &newline, 1)) set_status("Script limit reached.");
            continue;
        }
        if (key >= 32u && key <= 0x10FFFFu) {
            char encoded[4];
            uint32_t count = utf8_encode(key, encoded);
            if (count > 0 && !insert_bytes(cursor_offset, encoded, count)) {
                set_status("Script limit reached (48 KiB).");
            }
        }
    }
}

int app_main(const customos_api_t *api) {
    if (api == NULL || api->gfx_clear == NULL || api->fs_read == NULL ||
        api->fs_write == NULL || api->fs_count == NULL || api->fs_name_at == NULL ||
        api->kbd_wait_key == NULL) return -1;
    os_api = api;
    source_buffer[0] = '\0';
    set_status("Welcome to Visual Novel Studio.");

    uint32_t selected = 0;
    while (!should_exit) {
        draw_frame("CustomOS Visual Novel Studio", "UP/DOWN select; Enter; Esc exits");
        draw_text("Create and compile stories inside CustomOS.", 8, 29, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        for (uint32_t i = 0; i < MAIN_ITEM_COUNT; i++) {
            int y = 54 + (int)(i * 18u);
            if (i == selected) os_api->gfx_fill_rect(6, y - 3, 308, 16, COLOR_DARK_GRAY);
            draw_text(main_items[i], 14, y, i == selected ? COLOR_WHITE : COLOR_LIGHT_GRAY,
                      COLOR_TRANSPARENT);
        }
        draw_text(status_text, 8, 154, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
        draw_text("F12 changes keyboard layout while typing.", 8, 168, COLOR_DARK_GRAY, COLOR_TRANSPARENT);
        os_api->gfx_flip();

        uint32_t key = next_key();
        if (key == KEY_UP && selected > 0) selected--;
        else if (key == KEY_DOWN && selected + 1u < MAIN_ITEM_COUNT) selected++;
        else if (key == 27) break;
        else if (key == '\n' || key == 13u) {
            int opened = 0;
            switch (selected) {
                case 0:
                    opened = create_project();
                    break;
                case 1:
                    opened = open_project_menu();
                    break;
                case 2:
                    opened = load_continue_project();
                    break;
                case 3:
                    show_help();
                    break;
                case 4:
                    should_exit = 1;
                    break;
            }
            if (opened) editor_loop();
        }
    }
    os_api->gfx_clear(COLOR_BLACK);
    os_api->gfx_flip();
    return 0;
}
