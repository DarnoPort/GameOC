#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83

void    keyboard_init(void);
int     keyboard_has_char(void);
uint8_t keyboard_get_char(void);
uint8_t keyboard_wait_char(void);

#endif