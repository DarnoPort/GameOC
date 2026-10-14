#ifndef APP_LOADER_H
#define APP_LOADER_H

#include "types.h"
#include "cbx.h"

#define APP_BASE_ADDRESS 0x00400000 // 4 МБ физической памяти
#define APP_STACK_TOP    0x004F0000 // Вершина стека приложения

int app_load_and_run(const uint8_t *cbx_data, uint32_t cbx_size);

#endif