#include "app_loader.h"
#include "gfx.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"

extern int  app_invoke(uint32_t entry_point, const customos_api_t *api, uint32_t app_stack);
extern void app_exit_trampoline(int exit_code);

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

    /* 2. Проверка соответствия размеров данных в файле */
    if (sizeof(cbx_header_t) + hdr->code_size + hdr->data_size > cbx_size) {
        return -3; // Файл повреждён или обрезан
    }

    /* 3. Проверка лимита памяти приложения (максимум 900 КБ) */
    uint32_t total_mem = hdr->code_size + hdr->data_size + hdr->bss_size;
    if (total_mem > (900 * 1024)) {
        return -4; // Превышен лимит памяти
    }

    /* 4. Загрузка сегментов в физическую память пользователя по адресу APP_BASE_ADDRESS */
    uint8_t *dest = (uint8_t *)APP_BASE_ADDRESS;
    const uint8_t *src_payload = cbx_data + sizeof(cbx_header_t);

    /* Копируем код (.text) и данные (.rodata + .data) */
    for (uint32_t i = 0; i < hdr->code_size + hdr->data_size; i++) {
        dest[i] = src_payload[i];
    }

    /* Зануляем секцию .bss */
    uint8_t *bss_dest = dest + hdr->code_size + hdr->data_size;
    for (uint32_t i = 0; i < hdr->bss_size; i++) {
        bss_dest[i] = 0;
    }

    init_system_api();

    /* 5. Вычисление точки входа и безопасный вызов приложения */
    uint32_t entry_point = APP_BASE_ADDRESS + hdr->entry_offset;
    int exit_code = app_invoke(entry_point, &system_api, APP_STACK_TOP);

    return exit_code;
}