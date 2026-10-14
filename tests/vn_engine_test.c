#include "vn_engine.h"

#include <stdio.h>
#include <string.h>

static vn_program_t program;
static vn_runtime_t runtime;
static vn_event_t event;
static vn_error_t error;
static char oversized_source[VN_MAX_SOURCE_BYTES + 1u];

static unsigned checks = 0;
static unsigned failures = 0;

#define CHECK(condition, description) do { \
    checks++; \
    if (!(condition)) { \
        failures++; \
        printf("FAIL: %s (line %d)\n", description, __LINE__); \
    } \
} while (0)

static uint32_t text_length(const char *text) {
    uint32_t length = 0;
    while (text[length] != '\0') length++;
    return length;
}

static int compile_source(const char *source) {
    return vn_compile(source, text_length(source), &program, &error);
}

static void expect_compile_error(const char *source, vn_error_code_t expected,
                                 uint32_t expected_line, const char *description) {
    int result = compile_source(source);
    CHECK(result == 0, description);
    if (error.code != expected || error.line != expected_line) {
        printf("EXPECT ERROR [%s]: result=%d got code=%d line=%u; expected code=%d line=%u\n",
               description, result, (int)error.code, error.line,
               (int)expected, expected_line);
    }
    CHECK(error.code == expected, description);
    CHECK(error.line == expected_line, description);
    CHECK(error.message != NULL, description);
}

static int event_text_is(const vn_event_t *actual, const char *expected) {
    uint32_t length = text_length(expected);
    return actual->type == VN_EVENT_TEXT &&
           actual->text_length == length &&
           memcmp(actual->text, expected, length) == 0;
}

static void test_branch_runtime(void) {
    static const char source[] =
        "# A complete branching story.\n"
        "\n"
        "label start:\n"
        "    Welcome to the story;\n"
        "    menu:\n"
        "        Go left:\n"
        "            The left road begins;\n"
        "            jump forest;\n"
        "        endchoice;\n"
        "        Stay here:\n"
        "            You stayed at the station;\n"
        "        endchoice;\n"
        "    endmenu;\n"
        "    return;\n"
        "\n"
        "label forest:\n"
        "    You reached the forest;\n"
        "    return;\n";

    CHECK(compile_source(source) == 1, "compile a valid branching story");
    CHECK(error.code == VN_ERR_NONE, "successful compile clears diagnostic");
    CHECK(program.label_count == 2, "two labels are registered");
    CHECK(program.menu_count == 1, "one menu is registered");
    CHECK(program.choice_count == 2, "two choices are registered");

    CHECK(vn_runtime_start(&runtime, &program, NULL, &error) == 1,
          "start at first label");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_TEXT,
          "first event is dialogue");
    CHECK(event_text_is(&event, "Welcome to the story"),
          "first dialogue text is intact");

    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_MENU,
          "second event is an interactive menu");
    CHECK(event.choice_count == 2, "menu exposes two choices");
    CHECK(strcmp(program.strings + event.choices[0].text_offset, "Go left") == 0,
          "first choice title is intact");
    CHECK(strcmp(program.strings + event.choices[1].text_offset, "Stay here") == 0,
          "second choice title is intact");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_WAITING,
          "runtime waits while a menu choice is pending");
    CHECK(vn_runtime_choose(&runtime, 9, &error) == 0,
          "reject out-of-range choice");
    CHECK(error.code == VN_ERR_CHOICE_OUT_OF_RANGE,
          "out-of-range choice has a diagnostic");
    CHECK(vn_runtime_choose(&runtime, 0, &error) == 1,
          "accept the first menu choice");

    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_TEXT,
          "first choice executes its dialogue");
    CHECK(event_text_is(&event, "The left road begins"),
          "first choice branch text is intact");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_TEXT,
          "jump switches to the target label");
    CHECK(event_text_is(&event, "You reached the forest"),
          "jump target text is executed");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_END,
          "return finishes the story");

    CHECK(vn_runtime_start(&runtime, &program, "start", &error) == 1,
          "start by explicit label name");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_TEXT,
          "restarted story reaches first dialogue");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_MENU,
          "restarted story reaches menu");
    CHECK(vn_runtime_choose(&runtime, 1, &error) == 1,
          "accept the second menu choice");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_TEXT,
          "second branch executes dialogue");
    CHECK(event_text_is(&event, "You stayed at the station"),
          "second branch text is intact");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_END,
          "fall-through from branch resumes after menu and returns");
}

static void test_escaped_semicolon(void) {
    static const char source[] =
        "label hello:\n"
        "This includes a literal \\; in the middle;\n"
        "return;\n";

    CHECK(compile_source(source) == 1, "escaped semicolon compiles");
    CHECK(vn_runtime_start(&runtime, &program, NULL, &error) == 1,
          "start escaped-semicolon test");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_TEXT,
          "escaped-semicolon source emits dialogue");
    CHECK(event_text_is(&event, "This includes a literal ; in the middle"),
          "escape marker is removed from displayed dialogue");
}

static void test_syntax_errors(void) {
    expect_compile_error(
        "label first:\nreturn;\nlabel first:\nreturn;\n",
        VN_ERR_DUPLICATE_LABEL, 3, "reject duplicate labels");

    expect_compile_error(
        "label first:\nA line without a terminator\nreturn;\n",
        VN_ERR_EXPECTED_SEMICOLON, 2, "reject a missing semicolon");

    expect_compile_error(
        "label first:\njump missing;\nreturn;\n",
        VN_ERR_JUMP_TARGET_NOT_FOUND, 2, "reject a jump to a missing label");

    expect_compile_error(
        "label first:\nmenu:\nendmenu;\nreturn;\n",
        VN_ERR_EMPTY_MENU, 3, "reject an empty menu");

    expect_compile_error(
        "label first:\nmenu:\nEmpty option:\nendchoice;\nendmenu;\nreturn;\n",
        VN_ERR_EMPTY_CHOICE, 4, "reject an empty menu branch");

    expect_compile_error(
        "label first:\nmenu:\nOption:\nSome text;\nendmenu;\nreturn;\n",
        VN_ERR_EXPECTED_ENDCHOICE, 5, "reject a branch without endchoice");

    expect_compile_error(
        "label first:\nmenu:\nOption:\nSome text;\nendchoice;\nreturn;\n",
        VN_ERR_EXPECTED_ENDMENU, 6, "reject a menu without endmenu");

    expect_compile_error(
        "label first:\nmenu:\nOption:\nmenu:\nNested;\nendchoice;\nendmenu;\nreturn;\n",
        VN_ERR_BAD_NESTING, 4, "reject nested menu syntax");

    expect_compile_error(
        "label first:\ncall helper;\nreturn;\n",
        VN_ERR_UNSUPPORTED_COMMAND, 2, "explain unsupported call command");

    expect_compile_error(
        "label first:\n@shake;\nreturn;\n",
        VN_ERR_UNKNOWN_COMMAND, 2, "reject an explicitly unknown directive");

    expect_compile_error(
        "label first:\nA; B;\nreturn;\n",
        VN_ERR_UNESCAPED_SEMICOLON, 2, "reject unescaped semicolons in prose");

    expect_compile_error(
        "label empty:\nlabel nonempty:\nreturn;\n",
        VN_ERR_EMPTY_LABEL, 1, "reject an empty label body");

    expect_compile_error(
        "# comments only\n\n",
        VN_ERR_NO_LABELS, 0, "reject a file with no labels");
}


static void test_utf8_labels_and_validation(void) {
    static const char russian_source[] =
        "label начало:\n"
        "Добро пожаловать в историю;\n"
        "jump лес;\n"
        "label лес:\n"
        "Ты вошёл в лес;\n"
        "return;\n";
    static const char invalid_utf8[] = {
        'l','a','b','e','l',' ','o','k',':','\n',
        'T','e','x','t',' ',(char)0xC3,(char)0x28,';','\n',
        'r','e','t','u','r','n',';','\n'
    };

    CHECK(vn_compile(russian_source, sizeof(russian_source) - 1u,
                     &program, &error) == 1,
          "compile UTF-8 scenario with Russian labels and dialogue");
    CHECK(vn_runtime_start(&runtime, &program, "начало", &error) == 1,
          "start the scenario at a Russian label");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_TEXT,
          "UTF-8 scenario begins with dialogue");
    CHECK(event_text_is(&event, "Добро пожаловать в историю"),
          "Russian dialogue bytes are preserved");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_TEXT,
          "jump to a Russian label works");
    CHECK(event_text_is(&event, "Ты вошёл в лес"),
          "dialogue after a Russian jump is preserved");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_END,
          "Russian-label scenario returns normally");

    CHECK(vn_compile(invalid_utf8, sizeof(invalid_utf8),
                     &program, &error) == 0,
          "reject malformed UTF-8 source");
    CHECK(error.code == VN_ERR_INVALID_UTF8,
          "malformed UTF-8 has its own diagnostic");
    CHECK(error.line == 2,
          "malformed UTF-8 reports the correct source line");
}

static void test_invalid_sources_and_runtime_limits(void) {
    CHECK(vn_compile("", 0, &program, &error) == 0,
          "reject an empty source string");
    CHECK(error.code == VN_ERR_EMPTY_SCRIPT, "empty source has a diagnostic");

    memset(oversized_source, 'x', sizeof(oversized_source));
    CHECK(vn_compile(oversized_source, sizeof(oversized_source),
                     &program, &error) == 0,
          "reject source beyond fixed capacity");
    CHECK(error.code == VN_ERR_SOURCE_TOO_LARGE, "oversized source diagnostic");

    {
        static const char source[] = "label start:\nreturn;\n";
        static const char with_nul[] = {
            'l','a','b','e','l',' ','s','t','a','r','t',':','\n','\0',
            'r','e','t','u','r','n',';','\n'
        };
        (void)source;
        CHECK(vn_compile(with_nul, sizeof(with_nul), &program, &error) == 0,
              "reject an embedded NUL in source");
        CHECK(error.code == VN_ERR_EMBEDDED_NUL, "embedded NUL diagnostic");
    }

    CHECK(compile_source("label loop:\njump loop;\n") == 1,
          "compile a syntactically valid infinite loop");
    CHECK(vn_runtime_start(&runtime, &program, NULL, &error) == 1,
          "start loop-budget test");
    CHECK(vn_runtime_next(&runtime, &event, &error) == VN_EVENT_ERROR,
          "stop an infinite jump loop");
    CHECK(error.code == VN_ERR_RUNTIME_STEP_LIMIT,
          "infinite loop reports the execution step limit");

    CHECK(vn_runtime_start(&runtime, &program, "not_defined", &error) == 0,
          "reject a nonexistent start label");
    CHECK(error.code == VN_ERR_LABEL_NOT_FOUND,
          "missing start label has a diagnostic");
}

int main(void) {
    test_branch_runtime();
    test_escaped_semicolon();
    test_syntax_errors();
    test_utf8_labels_and_validation();
    test_invalid_sources_and_runtime_limits();

    if (failures != 0) {
        printf("vn_engine_test: %u/%u checks failed.\n", failures, checks);
        return 1;
    }

    printf("vn_engine_test: %u checks passed.\n", checks);
    return 0;
}
