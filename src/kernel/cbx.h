#ifndef CBX_H
#define CBX_H

#include "types.h"

/* Магическое число 'CBX1' (0x31584243 в little-endian) */
#define CBX_MAGIC 0x31584243

#define CBX_FLAG_CONSOLE 0
#define CBX_FLAG_GUI     1

/* Заголовок бинарного файла CBX (ровно 124 байта) */
typedef struct {
    uint32_t magic;           // 0x31584243 ("CBX1")
    uint16_t version;         // Версия формата (1)
    uint16_t flags;           // Флаги (1 = GUI, 0 = Console)
    char     name[32];        // Имя приложения (ASCIIZ)
    char     description[64]; // Краткое описание программы
    uint32_t entry_offset;    // Смещение точки входа относительно базы 0x00400000
    uint32_t code_size;       // Размер сегмента кода (.text) в байтах
    uint32_t data_size;       // Размер сегмента данных (.rodata + .data)
    uint32_t bss_size;        // Размер неинициализированной памяти (.bss)
    uint32_t stack_size;      // Рекомендуемый размер стека приложения
} __attribute__((packed)) cbx_header_t;

/* Структура CustomOS API (ABI взаимодействия приложения с ядром) */
typedef struct customos_api {
    uint32_t version;

    /* Текстовые функции */
    void (*print)(const char *str);
    void (*print_num)(int32_t n);
    void (*print_hex)(uint32_t n);

    /* 2-битная графическая подсистема (320x200, 4 цвета) */
    void (*gfx_clear)(uint8_t color);
    void (*gfx_put_pixel)(int x, int y, uint8_t color);
    uint8_t (*gfx_get_pixel)(int x, int y);
    void (*gfx_draw_line)(int x0, int y0, int x1, int y1, uint8_t color);
    void (*gfx_draw_rect)(int x, int y, int w, int h, uint8_t color);
    void (*gfx_fill_rect)(int x, int y, int w, int h, uint8_t color);
    void (*gfx_draw_circle)(int xc, int yc, int r, uint8_t color);
    void (*gfx_fill_circle)(int xc, int yc, int r, uint8_t color);
    void (*gfx_draw_text)(int x, int y, const char *str, uint8_t fg, int bg);
    void (*gfx_flip)(void);

    /* Ввод */
    int     (*kbd_has_key)(void);
    uint8_t (*kbd_get_key)(void);
    uint8_t (*kbd_wait_key)(void);

    /* Время и задержка */
    uint32_t (*timer_get_ticks)(void);
    void     (*timer_sleep)(uint32_t ms);

    /* Завершение приложения с возвратом кода в ОС */
    void     (*exit)(int exit_code);

    /* Persistent flat-file storage API (CustomOS API v2). Names are ASCII. */
    int32_t  (*fs_available)(void);
    int32_t  (*fs_count)(void);
    int32_t  (*fs_name_at)(uint32_t index, char *name_out, uint32_t capacity);
    int32_t  (*fs_size)(const char *name);
    int32_t  (*fs_read)(const char *name, void *buffer, uint32_t capacity);
    int32_t  (*fs_write)(const char *name, const void *buffer, uint32_t size);
    int32_t  (*fs_delete)(const char *name);
    int32_t  (*fs_rename)(const char *old_name, const char *new_name);
    /* UTF-8 text and Unicode keyboard events (API v3). */
    void     (*gfx_draw_utf8)(int x, int y, const char *str, uint8_t fg, int bg);
    uint32_t (*kbd_get_codepoint)(void);
    uint32_t (*kbd_wait_codepoint)(void);
    int32_t  (*kbd_russian_layout)(void);
    /* API v4: register/update a valid G_XXXXXXXX.CBX stored on ProjectFS. */
    int32_t  (*app_install_persistent)(const char *filename);
} customos_api_t;

#endif