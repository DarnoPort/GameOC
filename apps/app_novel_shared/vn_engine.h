#ifndef VN_ENGINE_H
#define VN_ENGINE_H

#include <stdint.h>

#define VN_PROGRAM_MAGIC          0x31504E56u /* "VNP1" */
#define VN_MAX_SOURCE_BYTES        (48u * 1024u)
#define VN_STRING_POOL_CAPACITY    (64u * 1024u)
#define VN_MAX_INSTRUCTIONS        4096u
#define VN_MAX_LABELS              128u
#define VN_MAX_MENUS               512u
#define VN_MAX_CHOICES             1024u
#define VN_MAX_IDENTIFIER_LENGTH   31u
#define VN_MAX_RUNTIME_STEPS       8192u

typedef enum {
    VN_OP_HALT = 0,
    VN_OP_TEXT = 1,
    VN_OP_JUMP = 2,
    VN_OP_RETURN = 3,
    VN_OP_MENU = 4,
    VN_OP_CHOICE_END = 5
} vn_opcode_t;

typedef struct {
    uint32_t line;
    uint32_t a;
    uint32_t b;
    uint8_t opcode;
    uint8_t reserved[3];
} vn_instruction_t;

typedef struct {
    uint32_t line;
    uint32_t name_offset;
    uint32_t target_pc;
} vn_label_t;

typedef struct {
    uint32_t line;
    uint32_t first_choice;
    uint32_t choice_count;
    uint32_t after_pc;
} vn_menu_t;

typedef struct {
    uint32_t line;
    uint32_t text_offset;
    uint32_t target_pc;
} vn_choice_t;

/*
 * Fixed-capacity representation. Allocate one instance in static/global
 * storage, not on the 16 KiB application stack. It intentionally needs no
 * malloc/free and can be shared by the editor, player and host-side tests.
 */
typedef struct {
    uint32_t magic;
    uint32_t instruction_count;
    uint32_t label_count;
    uint32_t menu_count;
    uint32_t choice_count;
    uint32_t string_used;

    vn_instruction_t instructions[VN_MAX_INSTRUCTIONS];
    vn_label_t labels[VN_MAX_LABELS];
    vn_menu_t menus[VN_MAX_MENUS];
    vn_choice_t choices[VN_MAX_CHOICES];
    char strings[VN_STRING_POOL_CAPACITY];
} vn_program_t;

typedef enum {
    VN_ERR_NONE = 0,
    VN_ERR_EMPTY_SCRIPT,
    VN_ERR_SOURCE_TOO_LARGE,
    VN_ERR_INVALID_ARGUMENT,
    VN_ERR_EMBEDDED_NUL,
    VN_ERR_INVALID_LABEL,
    VN_ERR_DUPLICATE_LABEL,
    VN_ERR_TOO_MANY_LABELS,
    VN_ERR_TOO_MANY_INSTRUCTIONS,
    VN_ERR_TOO_MANY_MENUS,
    VN_ERR_TOO_MANY_CHOICES,
    VN_ERR_STRING_POOL_FULL,
    VN_ERR_EXPECTED_SEMICOLON,
    VN_ERR_EMPTY_TEXT,
    VN_ERR_UNESCAPED_SEMICOLON,
    VN_ERR_BAD_COMMAND_SYNTAX,
    VN_ERR_INVALID_JUMP_TARGET,
    VN_ERR_JUMP_TARGET_NOT_FOUND,
    VN_ERR_EMPTY_MENU,
    VN_ERR_EMPTY_CHOICE,
    VN_ERR_EXPECTED_ENDCHOICE,
    VN_ERR_EXPECTED_ENDMENU,
    VN_ERR_BAD_NESTING,
    VN_ERR_UNSUPPORTED_COMMAND,
    VN_ERR_UNKNOWN_COMMAND,
    VN_ERR_EMPTY_LABEL,
    VN_ERR_NO_LABELS,
    VN_ERR_BAD_PROGRAM,
    VN_ERR_LABEL_NOT_FOUND,
    VN_ERR_RUNTIME_STEP_LIMIT,
    VN_ERR_CHOICE_NOT_PENDING,
    VN_ERR_CHOICE_OUT_OF_RANGE
} vn_error_code_t;

typedef struct {
    vn_error_code_t code;
    uint32_t line;
    const char *message;
} vn_error_t;

typedef enum {
    VN_EVENT_TEXT = 1,
    VN_EVENT_MENU,
    VN_EVENT_END,
    VN_EVENT_WAITING,
    VN_EVENT_ERROR
} vn_event_type_t;

typedef struct {
    vn_event_type_t type;
    uint32_t line;
    const char *text;
    uint32_t text_length;
    const vn_choice_t *choices;
    uint32_t choice_count;
} vn_event_t;

typedef struct {
    const vn_program_t *program;
    uint32_t pc;
    uint32_t steps;
    uint32_t waiting_menu;
    uint8_t waiting_for_choice;
    uint8_t finished;
} vn_runtime_t;

/*
 * Compile UTF-8 source into a validated, fixed-capacity in-memory program.
 * Returns 0 on success, nonzero on error. The source must not contain an
 * embedded NUL within source_length. Diagnostics contain the original line.
 */
int vn_compile(const char *source, uint32_t source_length,
               vn_program_t *out_program, vn_error_t *error);

/* Begin at label_name; NULL starts at the first declared label. */
int vn_runtime_start(vn_runtime_t *runtime, const vn_program_t *program,
                     const char *label_name, vn_error_t *error);

/* Emits TEXT/MENU/END/WAITING/ERROR. Call vn_runtime_choose after MENU. */
vn_event_type_t vn_runtime_next(vn_runtime_t *runtime,
                                vn_event_t *event, vn_error_t *error);

/* Resolve the currently displayed menu choice (zero-based). */
int vn_runtime_choose(vn_runtime_t *runtime, uint32_t choice_index,
                      vn_error_t *error);

const char *vn_error_message(vn_error_code_t code);

#endif
