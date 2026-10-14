#include "vn_engine.h"
#include <stddef.h>

#define VN_INVALID_INDEX 0xFFFFFFFFu

typedef struct {
    const char *text;
    uint32_t length;
    uint32_t line;
} vn_line_t;

typedef struct {
    const char *source;
    uint32_t source_length;
    uint32_t offset;
    uint32_t next_line_number;
    vn_program_t *program;
    vn_error_t *error;
} vn_parser_t;

static void vn_zero(void *destination, uint32_t size) {
    uint8_t *out = (uint8_t *)destination;
    for (uint32_t i = 0; i < size; i++) out[i] = 0;
}

static int vn_is_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r';
}

static int vn_is_letter(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static int vn_is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

static uint32_t vn_zlength(const char *text, uint32_t maximum) {
    uint32_t length = 0;
    if (text == NULL) return maximum + 1u;
    while (length < maximum && text[length] != '\0') length++;
    return length;
}

static int vn_string_equal(const char *a, const char *b) {
    if (a == NULL || b == NULL) return 0;
    uint32_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int vn_string_equals_slice(const char *string, const char *slice, uint32_t length) {
    if (string == NULL || slice == NULL) return 0;
    uint32_t i = 0;
    while (i < length && string[i] != '\0') {
        if (string[i] != slice[i]) return 0;
        i++;
    }
    return i == length && string[i] == '\0';
}

const char *vn_error_message(vn_error_code_t code) {
    switch (code) {
        case VN_ERR_NONE: return "No error.";
        case VN_ERR_EMPTY_SCRIPT: return "The script is empty.";
        case VN_ERR_SOURCE_TOO_LARGE: return "The script exceeds the source size limit.";
        case VN_ERR_INVALID_ARGUMENT: return "Invalid argument passed to the scenario engine.";
        case VN_ERR_EMBEDDED_NUL: return "The source contains a NUL byte.";
        case VN_ERR_INVALID_LABEL: return "Invalid label declaration or identifier.";
        case VN_ERR_DUPLICATE_LABEL: return "A label with this name is already declared.";
        case VN_ERR_TOO_MANY_LABELS: return "The script contains too many labels.";
        case VN_ERR_TOO_MANY_INSTRUCTIONS: return "The compiled script has too many instructions.";
        case VN_ERR_TOO_MANY_MENUS: return "The script contains too many menus.";
        case VN_ERR_TOO_MANY_CHOICES: return "The script contains too many menu choices.";
        case VN_ERR_STRING_POOL_FULL: return "The script string pool is full.";
        case VN_ERR_EXPECTED_SEMICOLON: return "Expected ';' at the end of the statement.";
        case VN_ERR_EMPTY_TEXT: return "A text line or choice title cannot be empty.";
        case VN_ERR_TEXT_TOO_LARGE: return "A text line or choice title exceeds 512 bytes.";
        case VN_ERR_UNESCAPED_SEMICOLON: return "Use '\\;' to include a semicolon inside text.";
        case VN_ERR_BAD_COMMAND_SYNTAX: return "The command syntax is invalid.";
        case VN_ERR_INVALID_JUMP_TARGET: return "Invalid jump target identifier.";
        case VN_ERR_JUMP_TARGET_NOT_FOUND: return "The jump target label does not exist.";
        case VN_ERR_EMPTY_MENU: return "A menu must contain at least one choice.";
        case VN_ERR_EMPTY_CHOICE: return "A menu choice must contain at least one statement.";
        case VN_ERR_EXPECTED_ENDCHOICE: return "Expected 'endchoice;' before the next menu item.";
        case VN_ERR_EXPECTED_ENDMENU: return "Expected 'endmenu;' to close the menu.";
        case VN_ERR_BAD_NESTING: return "This statement is not valid at this nesting level.";
        case VN_ERR_UNSUPPORTED_COMMAND: return "This command is not supported by language version 1.";
        case VN_ERR_UNKNOWN_COMMAND: return "Unknown command directive. Plain dialogue does not start with '@'.";
        case VN_ERR_EMPTY_LABEL: return "A label must contain at least one statement.";
        case VN_ERR_NO_LABELS: return "The script must declare at least one label.";
        case VN_ERR_BAD_PROGRAM: return "The compiled scenario data is invalid.";
        case VN_ERR_LABEL_NOT_FOUND: return "The requested start label does not exist.";
        case VN_ERR_RUNTIME_STEP_LIMIT: return "Execution stopped because the script exceeded the step limit.";
        case VN_ERR_CHOICE_NOT_PENDING: return "There is no menu waiting for a choice.";
        case VN_ERR_CHOICE_OUT_OF_RANGE: return "The selected menu item does not exist.";
        default: return "Unknown scenario engine error.";
    }
}

static int vn_set_error(vn_error_t *error, vn_error_code_t code, uint32_t line) {
    if (error != NULL) {
        error->code = code;
        error->line = line;
        error->message = vn_error_message(code);
    }
    return 0;
}

static void vn_clear_error(vn_error_t *error) {
    if (error == NULL) return;
    error->code = VN_ERR_NONE;
    error->line = 0;
    error->message = NULL;
}

static int vn_next_raw_line(vn_parser_t *parser, vn_line_t *line) {
    if (parser->offset >= parser->source_length) return 0;

    uint32_t start = parser->offset;
    uint32_t line_number = parser->next_line_number;
    while (parser->offset < parser->source_length &&
           parser->source[parser->offset] != '\n') {
        parser->offset++;
    }

    uint32_t end = parser->offset;
    if (parser->offset < parser->source_length &&
        parser->source[parser->offset] == '\n') {
        parser->offset++;
    }
    parser->next_line_number++;

    if (end > start && parser->source[end - 1] == '\r') end--;

    while (start < end && vn_is_space(parser->source[start])) start++;
    while (end > start && vn_is_space(parser->source[end - 1])) end--;

    line->text = parser->source + start;
    line->length = end - start;
    line->line = line_number;
    return 1;
}

static int vn_next_significant_line(vn_parser_t *parser, vn_line_t *line) {
    while (vn_next_raw_line(parser, line)) {
        if (line->length == 0) continue;
        if (line->text[0] == '#') continue;
        return 1;
    }
    return 0;
}

static int vn_line_equal(const vn_line_t *line, const char *text) {
    uint32_t length = vn_zlength(text, 256u);
    if (length != line->length) return 0;
    for (uint32_t i = 0; i < length; i++) {
        if (line->text[i] != text[i]) return 0;
    }
    return 1;
}

static int vn_line_starts_keyword(const vn_line_t *line, const char *keyword) {
    uint32_t length = vn_zlength(keyword, 64u);
    if (line->length < length) return 0;
    for (uint32_t i = 0; i < length; i++) {
        if (line->text[i] != keyword[i]) return 0;
    }
    if (line->length == length) return 1;
    char next = line->text[length];
    return vn_is_space(next) || next == ':' || next == ';';
}

static int vn_line_ends_with(const vn_line_t *line, char suffix) {
    return line->length != 0 && line->text[line->length - 1] == suffix;
}

static int vn_valid_identifier(const char *text, uint32_t length) {
    if (text == NULL || length == 0 || length > VN_MAX_IDENTIFIER_LENGTH) return 0;
    if (!(vn_is_letter(text[0]) || text[0] == '_')) return 0;
    for (uint32_t i = 1; i < length; i++) {
        if (!(vn_is_letter(text[i]) || vn_is_digit(text[i]) || text[i] == '_')) return 0;
    }
    return 1;
}

static int vn_add_instruction(vn_parser_t *parser, uint32_t line,
                              vn_opcode_t opcode, uint32_t a, uint32_t b,
                              uint32_t *index_out) {
    vn_program_t *program = parser->program;
    if (program->instruction_count >= VN_MAX_INSTRUCTIONS) {
        return vn_set_error(parser->error, VN_ERR_TOO_MANY_INSTRUCTIONS, line);
    }

    uint32_t index = program->instruction_count++;
    vn_instruction_t *instruction = &program->instructions[index];
    instruction->line = line;
    instruction->a = a;
    instruction->b = b;
    instruction->opcode = (uint8_t)opcode;
    instruction->reserved[0] = 0;
    instruction->reserved[1] = 0;
    instruction->reserved[2] = 0;
    if (index_out != NULL) *index_out = index;
    return 1;
}

static int vn_pool_add(vn_parser_t *parser, const char *text, uint32_t length,
                       int unescape_semicolon, uint32_t line,
                       uint32_t *offset_out, uint32_t *length_out) {
    vn_program_t *program = parser->program;
    uint32_t output_length = 0;

    for (uint32_t i = 0; i < length; i++) {
        if (unescape_semicolon && text[i] == '\\' &&
            i + 1 < length && text[i + 1] == ';') {
            i++;
        }
        output_length++;
    }

    if (output_length + 1u > VN_STRING_POOL_CAPACITY ||
        program->string_used > VN_STRING_POOL_CAPACITY - (output_length + 1u)) {
        return vn_set_error(parser->error, VN_ERR_STRING_POOL_FULL, line);
    }

    uint32_t offset = program->string_used;
    uint32_t written = 0;
    for (uint32_t i = 0; i < length; i++) {
        if (unescape_semicolon && text[i] == '\\' &&
            i + 1 < length && text[i + 1] == ';') {
            i++;
        }
        program->strings[offset + written++] = text[i];
    }
    program->strings[offset + written] = '\0';
    program->string_used += written + 1u;

    if (offset_out != NULL) *offset_out = offset;
    if (length_out != NULL) *length_out = written;
    return 1;
}

static int vn_add_label(vn_parser_t *parser, const vn_line_t *line) {
    vn_program_t *program = parser->program;
    if (program->label_count >= VN_MAX_LABELS) {
        return vn_set_error(parser->error, VN_ERR_TOO_MANY_LABELS, line->line);
    }
    if (!vn_line_ends_with(line, ':') || line->length <= 5 ||
        !vn_is_space(line->text[5])) {
        return vn_set_error(parser->error, VN_ERR_INVALID_LABEL, line->line);
    }

    uint32_t start = 5;
    while (start < line->length && vn_is_space(line->text[start])) start++;
    uint32_t end = line->length - 1u;
    uint32_t name_length = end >= start ? end - start : 0;
    if (!vn_valid_identifier(line->text + start, name_length)) {
        return vn_set_error(parser->error, VN_ERR_INVALID_LABEL, line->line);
    }

    for (uint32_t i = 0; i < program->label_count; i++) {
        if (vn_string_equals_slice(program->strings + program->labels[i].name_offset,
                                   line->text + start, name_length)) {
            return vn_set_error(parser->error, VN_ERR_DUPLICATE_LABEL, line->line);
        }
    }

    uint32_t name_offset = 0;
    if (!vn_pool_add(parser, line->text + start, name_length, 0, line->line,
                     &name_offset, NULL)) return 0;

    vn_label_t *label = &program->labels[program->label_count++];
    label->line = line->line;
    label->name_offset = name_offset;
    label->target_pc = program->instruction_count;
    return 1;
}

static int vn_is_unsupported_command(const vn_line_t *line) {
    static const char *const unsupported[] = {
        "call", "if", "elif", "else", "define", "scene", "show", "hide",
        "image", "play", "stop", "with", "python", "pass"
    };
    for (uint32_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); i++) {
        if (vn_line_starts_keyword(line, unsupported[i])) return 1;
    }
    return 0;
}

static int vn_parse_text(vn_parser_t *parser, const vn_line_t *line) {
    if (!vn_line_ends_with(line, ';')) {
        return vn_set_error(parser->error, VN_ERR_EXPECTED_SEMICOLON, line->line);
    }

    uint32_t slashes = 0;
    if (line->length >= 2) {
        uint32_t i = line->length - 1u;
        while (i > 0 && line->text[i - 1u] == '\\') {
            slashes++;
            i--;
        }
    }
    if ((slashes & 1u) != 0) {
        return vn_set_error(parser->error, VN_ERR_EXPECTED_SEMICOLON, line->line);
    }

    uint32_t text_length = line->length - 1u;
    while (text_length > 0 && vn_is_space(line->text[text_length - 1u])) text_length--;
    if (text_length == 0) {
        return vn_set_error(parser->error, VN_ERR_EMPTY_TEXT, line->line);
    }
    if (text_length > VN_MAX_TEXT_BYTES) {
        return vn_set_error(parser->error, VN_ERR_TEXT_TOO_LARGE, line->line);
    }

    for (uint32_t i = 0; i < text_length; i++) {
        if (line->text[i] == '\\' && i + 1u < text_length &&
            line->text[i + 1u] == ';') {
            i++;
        } else if (line->text[i] == ';') {
            return vn_set_error(parser->error, VN_ERR_UNESCAPED_SEMICOLON, line->line);
        }
    }

    uint32_t text_offset = 0;
    uint32_t stored_length = 0;
    if (!vn_pool_add(parser, line->text, text_length, 1, line->line,
                     &text_offset, &stored_length)) return 0;
    return vn_add_instruction(parser, line->line, VN_OP_TEXT,
                              text_offset, stored_length, NULL);
}

static int vn_parse_jump(vn_parser_t *parser, const vn_line_t *line) {
    if (!vn_line_ends_with(line, ';')) {
        return vn_set_error(parser->error, VN_ERR_EXPECTED_SEMICOLON, line->line);
    }

    uint32_t start = 4;
    while (start < line->length && vn_is_space(line->text[start])) start++;
    uint32_t end = line->length - 1u;
    while (end > start && vn_is_space(line->text[end - 1u])) end--;
    uint32_t target_length = end - start;
    if (!vn_valid_identifier(line->text + start, target_length)) {
        return vn_set_error(parser->error, VN_ERR_INVALID_JUMP_TARGET, line->line);
    }

    uint32_t target_offset = 0;
    if (!vn_pool_add(parser, line->text + start, target_length, 0, line->line,
                     &target_offset, NULL)) return 0;
    return vn_add_instruction(parser, line->line, VN_OP_JUMP,
                              VN_INVALID_INDEX, target_offset, NULL);
}

static int vn_parse_statement(vn_parser_t *parser, const vn_line_t *line,
                              int allow_menu);

static int vn_parse_menu(vn_parser_t *parser, const vn_line_t *menu_line) {
    vn_program_t *program = parser->program;
    if (program->menu_count >= VN_MAX_MENUS) {
        return vn_set_error(parser->error, VN_ERR_TOO_MANY_MENUS, menu_line->line);
    }

    uint32_t menu_index = program->menu_count++;
    vn_menu_t *menu = &program->menus[menu_index];
    menu->line = menu_line->line;
    menu->first_choice = program->choice_count;
    menu->choice_count = 0;
    menu->after_pc = 0;

    if (!vn_add_instruction(parser, menu_line->line, VN_OP_MENU,
                            menu_index, 0, NULL)) return 0;

    vn_line_t line;
    while (vn_next_significant_line(parser, &line)) {
        if (vn_line_equal(&line, "endmenu;")) {
            if (menu->choice_count == 0) {
                return vn_set_error(parser->error, VN_ERR_EMPTY_MENU, line.line);
            }
            menu->after_pc = program->instruction_count;
            return 1;
        }
        if (vn_line_equal(&line, "endchoice;")) {
            return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line.line);
        }
        if (vn_line_starts_keyword(&line, "endmenu") ||
            vn_line_starts_keyword(&line, "endchoice")) {
            return vn_set_error(parser->error, VN_ERR_EXPECTED_SEMICOLON, line.line);
        }
        if (vn_line_starts_keyword(&line, "label")) {
            return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line.line);
        }
        if (vn_line_starts_keyword(&line, "menu")) {
            return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line.line);
        }
        if (vn_is_unsupported_command(&line)) {
            return vn_set_error(parser->error, VN_ERR_UNSUPPORTED_COMMAND, line.line);
        }
        if (line.text[0] == '@') {
            return vn_set_error(parser->error, VN_ERR_UNKNOWN_COMMAND, line.line);
        }
        if (!vn_line_ends_with(&line, ':')) {
            return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line.line);
        }

        uint32_t title_length = line.length - 1u;
        while (title_length > 0 && vn_is_space(line.text[title_length - 1u])) title_length--;
        if (title_length == 0) {
            return vn_set_error(parser->error, VN_ERR_EMPTY_TEXT, line.line);
        }
        if (title_length > VN_MAX_TEXT_BYTES) {
            return vn_set_error(parser->error, VN_ERR_TEXT_TOO_LARGE, line.line);
        }
        if (menu->choice_count >= VN_MAX_CHOICES_PER_MENU ||
            program->choice_count >= VN_MAX_CHOICES) {
            return vn_set_error(parser->error, VN_ERR_TOO_MANY_CHOICES, line.line);
        }

        uint32_t title_offset = 0;
        if (!vn_pool_add(parser, line.text, title_length, 0, line.line,
                         &title_offset, NULL)) return 0;

        uint32_t choice_index = program->choice_count++;
        vn_choice_t *choice = &program->choices[choice_index];
        choice->line = line.line;
        choice->text_offset = title_offset;
        choice->target_pc = program->instruction_count;
        menu->choice_count++;

        uint32_t statements_in_choice = 0;
        int choice_closed = 0;
        while (vn_next_significant_line(parser, &line)) {
            if (vn_line_equal(&line, "endchoice;")) {
                if (statements_in_choice == 0) {
                    return vn_set_error(parser->error, VN_ERR_EMPTY_CHOICE, line.line);
                }
                if (!vn_add_instruction(parser, line.line, VN_OP_CHOICE_END,
                                        menu_index, 0, NULL)) return 0;
                choice_closed = 1;
                break;
            }
            if (vn_line_equal(&line, "endmenu;")) {
                return vn_set_error(parser->error, VN_ERR_EXPECTED_ENDCHOICE, line.line);
            }
            if (vn_line_starts_keyword(&line, "endmenu") ||
                vn_line_starts_keyword(&line, "endchoice")) {
                return vn_set_error(parser->error, VN_ERR_EXPECTED_SEMICOLON, line.line);
            }
            if (vn_line_starts_keyword(&line, "label") ||
                vn_line_starts_keyword(&line, "menu")) {
                return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line.line);
            }

            uint32_t before = program->instruction_count;
            if (!vn_parse_statement(parser, &line, 0)) return 0;
            if (program->instruction_count == before) {
                return vn_set_error(parser->error, VN_ERR_BAD_PROGRAM, line.line);
            }
            statements_in_choice++;
        }

        if (!choice_closed) {
            return vn_set_error(parser->error, VN_ERR_EXPECTED_ENDCHOICE, choice->line);
        }
    }

    return vn_set_error(parser->error, VN_ERR_EXPECTED_ENDMENU, menu_line->line);
}

static int vn_parse_statement(vn_parser_t *parser, const vn_line_t *line,
                              int allow_menu) {
    if (vn_line_starts_keyword(line, "jump")) {
        return vn_parse_jump(parser, line);
    }

    if (vn_line_starts_keyword(line, "return")) {
        if (!vn_line_equal(line, "return;")) {
            return vn_set_error(parser->error,
                vn_line_ends_with(line, ';') ? VN_ERR_BAD_COMMAND_SYNTAX
                                            : VN_ERR_EXPECTED_SEMICOLON,
                line->line);
        }
        return vn_add_instruction(parser, line->line, VN_OP_RETURN, 0, 0, NULL);
    }

    if (vn_line_starts_keyword(line, "menu")) {
        if (!allow_menu) return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line->line);
        if (!vn_line_equal(line, "menu:")) {
            return vn_set_error(parser->error, VN_ERR_BAD_COMMAND_SYNTAX, line->line);
        }
        return vn_parse_menu(parser, line);
    }

    if (vn_line_starts_keyword(line, "label")) {
        return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line->line);
    }

    if (vn_line_starts_keyword(line, "endmenu") ||
        vn_line_starts_keyword(line, "endchoice")) {
        if (!vn_line_ends_with(line, ';')) {
            return vn_set_error(parser->error, VN_ERR_EXPECTED_SEMICOLON, line->line);
        }
        return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line->line);
    }

    if (vn_is_unsupported_command(line)) {
        return vn_set_error(parser->error, VN_ERR_UNSUPPORTED_COMMAND, line->line);
    }
    if (line->text[0] == '@') {
        return vn_set_error(parser->error, VN_ERR_UNKNOWN_COMMAND, line->line);
    }
    if (vn_line_ends_with(line, ':')) {
        return vn_set_error(parser->error, VN_ERR_BAD_NESTING, line->line);
    }

    return vn_parse_text(parser, line);
}

static int vn_find_label(const vn_program_t *program, const char *name) {
    for (uint32_t i = 0; i < program->label_count; i++) {
        const char *label_name = program->strings + program->labels[i].name_offset;
        if (vn_string_equal(label_name, name)) return (int)i;
    }
    return -1;
}

static int vn_resolve_jumps(vn_parser_t *parser) {
    vn_program_t *program = parser->program;
    for (uint32_t i = 0; i < program->instruction_count; i++) {
        vn_instruction_t *instruction = &program->instructions[i];
        if (instruction->opcode != VN_OP_JUMP) continue;
        if (instruction->b >= program->string_used) {
            return vn_set_error(parser->error, VN_ERR_BAD_PROGRAM, instruction->line);
        }
        const char *target_name = program->strings + instruction->b;
        int target = vn_find_label(program, target_name);
        if (target < 0) {
            return vn_set_error(parser->error, VN_ERR_JUMP_TARGET_NOT_FOUND,
                                instruction->line);
        }
        instruction->a = (uint32_t)target;
        instruction->b = 0;
    }
    return 1;
}

int vn_compile(const char *source, uint32_t source_length,
               vn_program_t *out_program, vn_error_t *error) {
    vn_clear_error(error);
    if (out_program == NULL) {
        return vn_set_error(error, VN_ERR_INVALID_ARGUMENT, 0);
    }
    vn_zero(out_program, (uint32_t)sizeof(*out_program));

    if (source == NULL) return vn_set_error(error, VN_ERR_INVALID_ARGUMENT, 0);
    if (source_length == 0) return vn_set_error(error, VN_ERR_EMPTY_SCRIPT, 0);
    if (source_length > VN_MAX_SOURCE_BYTES) {
        return vn_set_error(error, VN_ERR_SOURCE_TOO_LARGE, 0);
    }
    for (uint32_t i = 0; i < source_length; i++) {
        if (source[i] == '\0') {
            return vn_set_error(error, VN_ERR_EMBEDDED_NUL, 0);
        }
    }

    vn_parser_t parser;
    parser.source = source;
    parser.source_length = source_length;
    parser.offset = 0;
    parser.next_line_number = 1;
    parser.program = out_program;
    parser.error = error;

    int current_label = -1;
    uint32_t current_label_line = 1;
    int label_has_statement = 0;
    vn_line_t line;

    while (vn_next_significant_line(&parser, &line)) {
        if (vn_line_starts_keyword(&line, "label")) {
            if (current_label >= 0) {
                if (!label_has_statement) {
                    return vn_set_error(error, VN_ERR_EMPTY_LABEL, current_label_line);
                }
                if (!vn_add_instruction(&parser, current_label_line, VN_OP_HALT,
                                        0, 0, NULL)) return 0;
            }
            if (!vn_add_label(&parser, &line)) return 0;
            current_label = (int)(out_program->label_count - 1u);
            current_label_line = line.line;
            label_has_statement = 0;
            continue;
        }

        if (current_label < 0) {
            return vn_set_error(error, VN_ERR_NO_LABELS, line.line);
        }

        if (!vn_parse_statement(&parser, &line, 1)) return 0;
        label_has_statement = 1;
    }

    if (current_label < 0 || out_program->label_count == 0) {
        return vn_set_error(error, VN_ERR_NO_LABELS, 0);
    }
    if (!label_has_statement) {
        return vn_set_error(error, VN_ERR_EMPTY_LABEL, current_label_line);
    }
    if (!vn_add_instruction(&parser, current_label_line, VN_OP_HALT, 0, 0, NULL)) {
        return 0;
    }

    if (!vn_resolve_jumps(&parser)) return 0;

    out_program->magic = VN_PROGRAM_MAGIC;
    return 1;
}

static int vn_pool_string_terminated(const vn_program_t *program, uint32_t offset) {
    if (program == NULL || offset >= program->string_used) return 0;
    for (uint32_t i = offset; i < program->string_used; i++) {
        if (program->strings[i] == '\0') return 1;
    }
    return 0;
}

static int vn_program_basic_valid(const vn_program_t *program) {
    if (program == NULL || program->magic != VN_PROGRAM_MAGIC) return 0;
    if (program->instruction_count == 0 ||
        program->instruction_count > VN_MAX_INSTRUCTIONS) return 0;
    if (program->label_count == 0 || program->label_count > VN_MAX_LABELS) return 0;
    if (program->menu_count > VN_MAX_MENUS ||
        program->choice_count > VN_MAX_CHOICES ||
        program->string_used > VN_STRING_POOL_CAPACITY) return 0;

    for (uint32_t i = 0; i < program->label_count; i++) {
        const vn_label_t *label = &program->labels[i];
        if (label->name_offset >= program->string_used ||
            label->target_pc >= program->instruction_count ||
            program->strings[label->name_offset] == '\0' ||
            !vn_pool_string_terminated(program, label->name_offset)) return 0;
    }
    for (uint32_t i = 0; i < program->menu_count; i++) {
        const vn_menu_t *menu = &program->menus[i];
        if (menu->first_choice > program->choice_count ||
            menu->choice_count == 0 ||
            menu->choice_count > program->choice_count - menu->first_choice ||
            menu->after_pc > program->instruction_count) return 0;
    }
    for (uint32_t i = 0; i < program->choice_count; i++) {
        const vn_choice_t *choice = &program->choices[i];
        if (choice->text_offset >= program->string_used ||
            choice->target_pc >= program->instruction_count ||
            !vn_pool_string_terminated(program, choice->text_offset)) return 0;
    }
    for (uint32_t i = 0; i < program->instruction_count; i++) {
        const vn_instruction_t *instruction = &program->instructions[i];
        if (instruction->opcode == VN_OP_TEXT) {
            if (instruction->a >= program->string_used ||
                instruction->b >= program->string_used - instruction->a ||
                program->strings[instruction->a + instruction->b] != '\0') return 0;
        } else if (instruction->opcode == VN_OP_JUMP) {
            if (instruction->a >= program->label_count) return 0;
        } else if (instruction->opcode == VN_OP_MENU ||
                   instruction->opcode == VN_OP_CHOICE_END) {
            if (instruction->a >= program->menu_count) return 0;
        } else if (instruction->opcode != VN_OP_HALT &&
                   instruction->opcode != VN_OP_RETURN) {
            return 0;
        }
    }
    return 1;
}

int vn_runtime_start(vn_runtime_t *runtime, const vn_program_t *program,
                     const char *label_name, vn_error_t *error) {
    vn_clear_error(error);
    if (runtime == NULL || !vn_program_basic_valid(program)) {
        return vn_set_error(error, VN_ERR_BAD_PROGRAM, 0);
    }

    int label_index = 0;
    if (label_name != NULL) {
        label_index = vn_find_label(program, label_name);
        if (label_index < 0) return vn_set_error(error, VN_ERR_LABEL_NOT_FOUND, 0);
    }

    runtime->program = program;
    runtime->pc = program->labels[label_index].target_pc;
    runtime->steps = 0;
    runtime->waiting_menu = 0;
    runtime->waiting_for_choice = 0;
    runtime->finished = 0;
    return 1;
}

vn_event_type_t vn_runtime_next(vn_runtime_t *runtime,
                                vn_event_t *event, vn_error_t *error) {
    vn_clear_error(error);
    if (event == NULL) {
        vn_set_error(error, VN_ERR_INVALID_ARGUMENT, 0);
        return VN_EVENT_ERROR;
    }
    vn_zero(event, (uint32_t)sizeof(*event));

    if (runtime == NULL || runtime->program == NULL ||
        runtime->program->magic != VN_PROGRAM_MAGIC ||
        runtime->program->instruction_count == 0 ||
        runtime->program->instruction_count > VN_MAX_INSTRUCTIONS ||
        runtime->pc > runtime->program->instruction_count ||
        runtime->program->label_count == 0 ||
        runtime->program->label_count > VN_MAX_LABELS ||
        runtime->program->menu_count > VN_MAX_MENUS ||
        runtime->program->choice_count > VN_MAX_CHOICES ||
        runtime->program->string_used > VN_STRING_POOL_CAPACITY) {
        vn_set_error(error, VN_ERR_BAD_PROGRAM, 0);
        event->type = VN_EVENT_ERROR;
        return event->type;
    }
    if (runtime->finished) {
        event->type = VN_EVENT_END;
        return event->type;
    }
    if (runtime->waiting_for_choice) {
        event->type = VN_EVENT_WAITING;
        return event->type;
    }

    const vn_program_t *program = runtime->program;
    while (runtime->pc < program->instruction_count) {
        if (runtime->steps >= VN_MAX_RUNTIME_STEPS) {
            runtime->finished = 1;
            vn_set_error(error, VN_ERR_RUNTIME_STEP_LIMIT,
                         program->instructions[runtime->pc].line);
            event->type = VN_EVENT_ERROR;
            return event->type;
        }
        runtime->steps++;

        const vn_instruction_t *instruction = &program->instructions[runtime->pc++];
        event->line = instruction->line;

        if (instruction->opcode == VN_OP_TEXT) {
            if (instruction->a >= program->string_used ||
                instruction->b >= program->string_used - instruction->a ||
                program->strings[instruction->a + instruction->b] != '\0') {
                runtime->finished = 1;
                vn_set_error(error, VN_ERR_BAD_PROGRAM, instruction->line);
                event->type = VN_EVENT_ERROR;
                return event->type;
            }
            event->type = VN_EVENT_TEXT;
            event->text = program->strings + instruction->a;
            event->text_length = instruction->b;
            return event->type;
        }

        if (instruction->opcode == VN_OP_MENU) {
            if (instruction->a >= program->menu_count) {
                runtime->finished = 1;
                vn_set_error(error, VN_ERR_BAD_PROGRAM, instruction->line);
                event->type = VN_EVENT_ERROR;
                return event->type;
            }
            const vn_menu_t *menu = &program->menus[instruction->a];
            runtime->waiting_menu = instruction->a;
            runtime->waiting_for_choice = 1;
            event->type = VN_EVENT_MENU;
            event->choices = &program->choices[menu->first_choice];
            event->choice_count = menu->choice_count;
            return event->type;
        }

        if (instruction->opcode == VN_OP_JUMP) {
            if (instruction->a >= program->label_count ||
                program->labels[instruction->a].target_pc >= program->instruction_count) {
                runtime->finished = 1;
                vn_set_error(error, VN_ERR_BAD_PROGRAM, instruction->line);
                event->type = VN_EVENT_ERROR;
                return event->type;
            }
            runtime->pc = program->labels[instruction->a].target_pc;
            continue;
        }

        if (instruction->opcode == VN_OP_CHOICE_END) {
            if (instruction->a >= program->menu_count ||
                program->menus[instruction->a].after_pc > program->instruction_count) {
                runtime->finished = 1;
                vn_set_error(error, VN_ERR_BAD_PROGRAM, instruction->line);
                event->type = VN_EVENT_ERROR;
                return event->type;
            }
            runtime->pc = program->menus[instruction->a].after_pc;
            continue;
        }

        if (instruction->opcode == VN_OP_RETURN || instruction->opcode == VN_OP_HALT) {
            runtime->finished = 1;
            event->type = VN_EVENT_END;
            return event->type;
        }

        runtime->finished = 1;
        vn_set_error(error, VN_ERR_BAD_PROGRAM, instruction->line);
        event->type = VN_EVENT_ERROR;
        return event->type;
    }

    runtime->finished = 1;
    event->type = VN_EVENT_END;
    return event->type;
}

int vn_runtime_choose(vn_runtime_t *runtime, uint32_t choice_index,
                      vn_error_t *error) {
    vn_clear_error(error);
    if (runtime == NULL || runtime->program == NULL ||
        !runtime->waiting_for_choice) {
        return vn_set_error(error, VN_ERR_CHOICE_NOT_PENDING, 0);
    }
    if (!vn_program_basic_valid(runtime->program)) {
        return vn_set_error(error, VN_ERR_BAD_PROGRAM, 0);
    }

    if (runtime->waiting_menu >= runtime->program->menu_count) {
        return vn_set_error(error, VN_ERR_BAD_PROGRAM, 0);
    }
    const vn_menu_t *menu = &runtime->program->menus[runtime->waiting_menu];
    if (choice_index >= menu->choice_count) {
        return vn_set_error(error, VN_ERR_CHOICE_OUT_OF_RANGE, menu->line);
    }

    const vn_choice_t *choice =
        &runtime->program->choices[menu->first_choice + choice_index];
    runtime->pc = choice->target_pc;
    runtime->waiting_for_choice = 0;
    return 1;
}
