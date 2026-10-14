#ifndef KEYBOARD_H
#define KEYBOARD_H
#include "types.h"
#define KEY_UP 0x80u
#define KEY_DOWN 0x81u
#define KEY_LEFT 0x82u
#define KEY_RIGHT 0x83u
#define KEY_HOME 0x84u
#define KEY_END 0x85u
#define KEY_DELETE 0x86u
#define KEY_PAGE_UP 0x87u
#define KEY_PAGE_DOWN 0x88u
#define KEY_INSERT 0x89u
#define KEY_F1 0x90u
#define KEY_F2 0x91u
#define KEY_F3 0x92u
#define KEY_F4 0x93u
#define KEY_F5 0x94u
#define KEY_F6 0x95u
#define KEY_F7 0x96u
#define KEY_F8 0x97u
#define KEY_F9 0x98u
#define KEY_F10 0x99u
#define KEY_F11 0x9Au
#define KEY_F12 0x9Bu
#define KEY_LAYOUT_TOGGLE 0x9Cu
void keyboard_init(void);
int keyboard_has_char(void);
uint8_t keyboard_get_char(void);
uint8_t keyboard_wait_char(void);
uint32_t keyboard_get_codepoint(void);
uint32_t keyboard_wait_codepoint(void);
int keyboard_russian_layout(void);
#endif