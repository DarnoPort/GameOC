#include "app_registry.h"
#include "app_loader.h"

static registered_app_t registered_apps[MAX_REGISTERED_APPS];
static int registered_count = 0;

static void kstr_copy(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < (max_len - 1)) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int app_registry_init(const uint8_t *fs_data, uint32_t fs_size) {
    registered_count = 0;

    if (fs_data == NULL || fs_size < sizeof(appfs_header_t)) {
        return -1;
    }

    const appfs_header_t *hdr = (const appfs_header_t *)fs_data;
    if (hdr->magic != APPFS_MAGIC) {
        return -2; // Невалидный заголовок AppFS
    }

    uint32_t count = hdr->count;
    if (count > MAX_REGISTERED_APPS) {
        count = MAX_REGISTERED_APPS;
    }

    const appfs_entry_t *entries = (const appfs_entry_t *)(fs_data + sizeof(appfs_header_t));

    for (uint32_t i = 0; i < count; i++) {
        /* Проверка границ файла */
        if (entries[i].offset + entries[i].size > fs_size) {
            continue;
        }

        kstr_copy(registered_apps[registered_count].filename,    entries[i].filename, 32);
        kstr_copy(registered_apps[registered_count].name,        entries[i].name, 32);
        kstr_copy(registered_apps[registered_count].description, entries[i].description, 64);
        registered_apps[registered_count].size = entries[i].size;
        registered_apps[registered_count].data = fs_data + entries[i].offset;

        registered_count++;
    }

    return registered_count;
}

int app_registry_get_count(void) {
    return registered_count;
}

const registered_app_t *app_registry_get(int index) {
    if (index < 0 || index >= registered_count) {
        return NULL;
    }
    return &registered_apps[index];
}

int app_registry_launch(int index) {
    const registered_app_t *app = app_registry_get(index);
    if (!app || !app->data) {
        return -1;
    }
    return app_load_and_run(app->data, app->size);
}