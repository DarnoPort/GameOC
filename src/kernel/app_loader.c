#include "app_loader.h"
#include "gfx.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"

extern int  app_invoke(uint32_t entry_point, const customos_api_t *api, uint32_t app_stack);
extern void app_exit_trampoline(int exit_code);

#define APP_MAX_MEMORY (900u * 1024u)

static customos_api_t system_api;
static int api_initialized = 0;

static void init_system_api(void) {
    if (api_initialized) return;

    system_api.version = 1;

    /* Текстовые вызовы */
    system_api.print      = vga_print;
    system_api.print_num  = (void (*)(int32_t))vga_print_dec;
    system_api.print_hex  = vga_print_hex;

    /* Графика 2 bpp */
    system_api.gfx_clear       = gfx_clear;
    system_api.gfx_put_pixel   = gfx_put_pixel;
    system_api.gfx_get_pixel   = gfx_get_pixel;
    system_api.gfx_draw_line   = gfx_draw_line;
    system_api.gfx_draw_rect   = gfx_draw_rect;
    system_api.gfx_fill_rect   = gfx_fill_rect;
    system_api.gfx_draw_circle = gfx_draw_circle;
    system_api.gfx_fill_circle = gfx_fill_circle;
    system_api.gfx_draw_text   = gfx_draw_text;
    system_api.gfx_flip        = gfx_flip;

    /* Ввод */
    system_api.kbd_has_key  = keyboard_has_char;
    system_api.kbd_get_key  = keyboard_get_char;
    system_api.kbd_wait_key = keyboard_wait_char;

    /* Время */
    system_api.timer_get_ticks = timer_get_ticks;
    system_api.timer_sleep     = timer_sleep;

    /* Выход */
    system_api.exit = app_exit_trampoline;

    api_initialized = 1;
}

int app_load_and_run(const uint8_t *cbx_data, uint32_t cbx_size) {
    if (cbx_data == NULL || cbx_size < sizeof(cbx_header_t)) {
        return -1; // Некорректный размер файла
    }

    const cbx_header_t *hdr = (const cbx_header_t *)cbx_data;

    /* 1. Проверка сигнатуры и версии */
    if (hdr->magic != CBX_MAGIC || hdr->version != 1) {
        return -2; // Невалидный формат исполняемого файла
    }

    /* 2. Проверка размеров без переполнения uint32_t */
    const uint32_t payload_size = cbx_size - (uint32_t)sizeof(cbx_header_t);
    if (hdr->code_size == 0 ||
        hdr->code_size > payload_size ||
        hdr->data_size > payload_size - hdr->code_size) {
        return -3; // Файл повреждён или обрезан
    }

    /* CBX v1 загружается по исходным адресам, поэтому entry обязан быть в коде. */
    if (hdr->entry_offset >= hdr->code_size) {
        return -5; // Точка входа расположена за пределами кода
    }

    /* 3. Проверка лимита памяти приложения (максимум 900 КБ) */
    if (hdr->code_size > APP_MAX_MEMORY ||
        hdr->data_size > APP_MAX_MEMORY - hdr->code_size) {
        return -4;
    }

    const uint32_t file_mem_size = hdr->code_size + hdr->data_size;
    if (hdr->bss_size > APP_MAX_MEMORY - file_mem_size) {
        return -4;
    }
    const uint32_t total_mem = file_mem_size + hdr->bss_size;
    if (total_mem == 0) {
        return -4;
    }

    /* 4. Загрузка образа и сохранение исходных адресов секций */
    uint8_t *dest = (uint8_t *)APP_BASE_ADDRESS;
    const uint8_t *src_payload = cbx_data + sizeof(cbx_header_t);

    for (uint32_t i = 0; i < file_mem_size; i++) {
        dest[i] = src_payload[i];
    }

    /* .bss и промежутки перед ней уже учтены упаковщиком в общем адресном пространстве. */
    uint8_t *bss_dest = dest + file_mem_size;
    for (uint32_t i = 0; i < hdr->bss_size; i++) {
        bss_dest[i] = 0;
    }

    init_system_api();

    /* 5. Вычисление точки входа и вызов приложения */
    uint32_t entry_point = APP_BASE_ADDRESS + hdr->entry_offset;
    int exit_code = app_invoke(entry_point, &system_api, APP_STACK_TOP);

    return exit_code;
}
