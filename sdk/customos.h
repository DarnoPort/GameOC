#ifndef SDK_CUSTOMOS_H
#define SDK_CUSTOMOS_H

typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;

#define NULL ((void *)0)

/* 2-битные цвета */
#define COLOR_BLACK       0
#define COLOR_DARK_GRAY   1
#define COLOR_LIGHT_GRAY  2
#define COLOR_WHITE       3
#define COLOR_TRANSPARENT -1

/* Коды клавиш управления */
#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83
#define KEY_HOME  0x84
#define KEY_END 0x85
#define KEY_DELETE 0x86
#define KEY_PAGE_UP 0x87
#define KEY_PAGE_DOWN 0x88
#define KEY_INSERT 0x89
#define KEY_F1 0x90
#define KEY_F2 0x91
#define KEY_F3 0x92
#define KEY_F4 0x93
#define KEY_F5 0x94
#define KEY_F6 0x95
#define KEY_F7 0x96
#define KEY_F8 0x97
#define KEY_F9 0x98
#define KEY_F10 0x99
#define KEY_F11 0x9A
#define KEY_F12 0x9B
#define KEY_LAYOUT_TOGGLE 0x9C

/* CustomOS API: v2 persistent files, v3 UTF-8 input/rendering, v4 app install. */
typedef struct customos_api {
    uint32_t version;

    void (*print)(const char *str);
    void (*print_num)(int32_t n);
    void (*print_hex)(uint32_t n);

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

    int     (*kbd_has_key)(void);
    uint8_t (*kbd_get_key)(void);
    uint8_t (*kbd_wait_key)(void);

    uint32_t (*timer_get_ticks)(void);
    void     (*timer_sleep)(uint32_t ms);

    void     (*exit)(int exit_code);

    /* Persistent flat-file storage API. Names are ASCII, sizes in bytes. */
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

/* Обязательная точка входа приложений CustomOS */
int app_main(const customos_api_t *api);

#endif
