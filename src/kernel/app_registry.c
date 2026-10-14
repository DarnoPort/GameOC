#include "app_registry.h"
#include "app_loader.h"

#define APP_MAX_MEMORY (900u * 1024u)

static registered_app_t registered_apps[MAX_REGISTERED_APPS];
static int registered_count = 0;

static const uint8_t *external_appfs = NULL;
static uint32_t external_appfs_size = 0;

static void kstr_copy(char *dst, const char *src, uint32_t max_len) {
    uint32_t i = 0;
    if (max_len == 0) return;
    while (i + 1 < max_len && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int kstr_equal(const char *a, const char *b) {
    uint32_t i = 0;
    while (i < 32 && a[i] == b[i]) {
        if (a[i] == '\0') return 1;
        i++;
    }
    return 0;
}

static int validate_cbx(const uint8_t *data, uint32_t size) {
    if (data == NULL || size < sizeof(cbx_header_t)) return 0;

    const cbx_header_t *hdr = (const cbx_header_t *)data;
    if (hdr->magic != CBX_MAGIC || hdr->version != 1 || hdr->code_size == 0) {
        return 0;
    }

    const uint32_t payload_size = size - (uint32_t)sizeof(cbx_header_t);
    if (hdr->code_size > payload_size ||
        hdr->data_size > payload_size - hdr->code_size ||
        hdr->entry_offset >= hdr->code_size) {
        return 0;
    }

    const uint32_t file_memory = hdr->code_size + hdr->data_size;
    if (file_memory > APP_MAX_MEMORY || hdr->bss_size > APP_MAX_MEMORY - file_memory) {
        return 0;
    }

    return 1;
}

/* Возвращает число записей при корректном AppFS, иначе отрицательный код. */
static int validate_appfs(const uint8_t *fs_data, uint32_t fs_size,
                          uint32_t *count_out) {
    if (fs_data == NULL || fs_size < sizeof(appfs_header_t)) return -1;

    const appfs_header_t *hdr = (const appfs_header_t *)fs_data;
    if (hdr->magic != APPFS_MAGIC) return -2;
    if (hdr->count > MAX_REGISTERED_APPS) return -3;

    const uint32_t table_bytes = fs_size - (uint32_t)sizeof(appfs_header_t);
    if (hdr->count > table_bytes / (uint32_t)sizeof(appfs_entry_t)) return -3;

    *count_out = hdr->count;
    return 0;
}

/* Добавляет одну запись; 1=добавлена, 0=невалидна/дубликат, -1=место кончилось. */
static int append_entry(const uint8_t *fs_data, uint32_t fs_size,
                        uint32_t table_end, const appfs_entry_t *entry,
                        int removable) {
    if (registered_count >= MAX_REGISTERED_APPS) return -1;

    /* Запрещаем ссылки на заголовок/каталог и проверяем границы без переполнения. */
    if (entry->offset < table_end ||
        entry->offset > fs_size ||
        entry->size > fs_size - entry->offset) {
        return 0;
    }

    const uint8_t *cbx_data = fs_data + entry->offset;
    if (!validate_cbx(cbx_data, entry->size)) return 0;

    char filename[32];
    char name[32];
    kstr_copy(filename, entry->filename, sizeof(filename));
    kstr_copy(name, entry->name, sizeof(name));

    /* Не добавляем повторно уже установленное приложение. */
    for (int i = 0; i < registered_count; i++) {
        if (kstr_equal(registered_apps[i].filename, filename) ||
            kstr_equal(registered_apps[i].name, name)) {
            return 0;
        }
    }

    registered_app_t *app = &registered_apps[registered_count];
    kstr_copy(app->filename, entry->filename, sizeof(app->filename));
    kstr_copy(app->name, entry->name, sizeof(app->name));
    kstr_copy(app->description, entry->description, sizeof(app->description));
    app->size = entry->size;
    app->data = cbx_data;
    app->removable = removable ? 1 : 0;
    registered_count++;
    return 1;
}

int app_registry_init(const uint8_t *fs_data, uint32_t fs_size) {
    registered_count = 0;
    external_appfs = NULL;
    external_appfs_size = 0;

    uint32_t count = 0;
    int result = validate_appfs(fs_data, fs_size, &count);
    if (result != 0) return result;

    const appfs_entry_t *entries =
        (const appfs_entry_t *)(fs_data + sizeof(appfs_header_t));
    uint32_t table_end = (uint32_t)sizeof(appfs_header_t) +
                        count * (uint32_t)sizeof(appfs_entry_t);

    for (uint32_t i = 0; i < count; i++) {
        append_entry(fs_data, fs_size, table_end, &entries[i], 0);
    }

    return registered_count;
}

int app_registry_set_external_source(const uint8_t *fs_data, uint32_t fs_size) {
    uint32_t count = 0;
    int result = validate_appfs(fs_data, fs_size, &count);
    if (result != 0) {
        external_appfs = NULL;
        external_appfs_size = 0;
        return result;
    }

    /*
     * Application execution uses 0x00400000..0x004F0000. A boot module that
     * overlaps this fixed region would be overwritten when an app is launched.
     */
    uint32_t source_start = (uint32_t)fs_data;
    uint32_t source_end = source_start + fs_size;
    if (source_end < source_start ||
        (source_start < APP_STACK_TOP && source_end > APP_BASE_ADDRESS)) {
        external_appfs = NULL;
        external_appfs_size = 0;
        return -4;
    }

    external_appfs = fs_data;
    external_appfs_size = fs_size;
    return (int)count;
}

int app_registry_external_available(void) {
    return external_appfs != NULL && external_appfs_size != 0;
}

int app_registry_install_external(void) {
    if (!app_registry_external_available()) return -1;

    uint32_t count = 0;
    int result = validate_appfs(external_appfs, external_appfs_size, &count);
    if (result != 0) return -2;

    const appfs_entry_t *entries =
        (const appfs_entry_t *)(external_appfs + sizeof(appfs_header_t));
    uint32_t table_end = (uint32_t)sizeof(appfs_header_t) +
                        count * (uint32_t)sizeof(appfs_entry_t);
    int added = 0;

    for (uint32_t i = 0; i < count; i++) {
        int item_result = append_entry(external_appfs, external_appfs_size,
                                       table_end, &entries[i], 1);
        if (item_result > 0) added++;
    }

    return added;
}

int app_registry_get_count(void) {
    return registered_count;
}

const registered_app_t *app_registry_get(int index) {
    if (index < 0 || index >= registered_count) return NULL;
    return &registered_apps[index];
}

/* Встроенные приложения нельзя удалить из списка во время работы ОС. */
int app_registry_remove(int index) {
    if (index < 0 || index >= registered_count) return -1;
    if (!registered_apps[index].removable) return -2;

    for (int i = index; i + 1 < registered_count; i++) {
        registered_apps[i] = registered_apps[i + 1];
    }

    registered_count--;
    registered_apps[registered_count].filename[0] = '\0';
    registered_apps[registered_count].name[0] = '\0';
    registered_apps[registered_count].description[0] = '\0';
    registered_apps[registered_count].size = 0;
    registered_apps[registered_count].data = NULL;
    registered_apps[registered_count].removable = 0;
    return 0;
}

int app_registry_launch(int index) {
    const registered_app_t *app = app_registry_get(index);
    if (!app || !app->data) return -1;
    return app_load_and_run(app->data, app->size);
}
