#ifndef APP_REGISTRY_H
#define APP_REGISTRY_H

#include "types.h"
#include "cbx.h"

#define APPFS_MAGIC          0x31534641 // "AFS1"
#define MAX_REGISTERED_APPS  16

/* Запись в каталоге AppFS на диске */
typedef struct {
    char     filename[32];
    char     name[32];
    char     description[64];
    uint32_t offset;
    uint32_t size;
} __attribute__((packed)) appfs_entry_t;

/* Заголовок AppFS */
typedef struct {
    uint32_t magic;
    uint32_t count;
} __attribute__((packed)) appfs_header_t;

/* Зарегистрированное приложение в оперативной памяти */
typedef struct {
    char     filename[32];
    char     name[32];
    char     description[64];
    uint32_t size;
    const uint8_t *data;
} registered_app_t;

int  app_registry_init(const uint8_t *fs_data, uint32_t fs_size);
int  app_registry_get_count(void);
const registered_app_t *app_registry_get(int index);
int  app_registry_launch(int index);

#endif