#include "app_registry.h"
#include "app_loader.h"

#define APP_MAX_MEMORY (900u * 1024u)

static registered_app_t registered_apps[MAX_REGISTERED_APPS];
static int registered_count = 0;

static const uint8_t *external_appfs = NULL;
static uint32_t external_appfs_size = 0;

/*
 * OS-owned copy of imported external CBX files. The kernel selects this region
 * from RAM reported by Multiboot, beyond the kernel and boot modules.
 */
static uint8_t *app_store_base = NULL;
static uint32_t app_store_size = 0;
static uint32_t app_store_used = 0;

static int ranges_overlap(uint32_t a_start, uint32_t a_end,
                          uint32_t b_start, uint32_t b_end) {
    return a_start < b_end && b_start < a_end;
}

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
    if (file_memory > APP_MAX_MEMORY ||
        hdr->bss_size > APP_MAX_MEMORY - file_memory) {
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

int app_registry_set_storage(uint8_t *base, uint32_t size) {
    if (base == NULL || size == 0) return -1;

    uint32_t start = (uint32_t)base;
    uint32_t end = start + size;
    if (end < start) return -2;

    /* Never put installable files in the fixed application image/stack arena. */
    if (ranges_overlap(start, end, APP_BASE_ADDRESS, APP_STACK_TOP)) return -3;

    /* Do not overwrite the bootloader-owned source package while copying apps. */
    if (external_appfs != NULL) {
        uint32_t source_start = (uint32_t)external_appfs;
        uint32_t source_end = source_start + external_appfs_size;
        if (source_end < source_start ||
            ranges_overlap(start, end, source_start, source_end)) {
            return -4;
        }
    }

    for (int i = 0; i < registered_count; i++) {
        if (registered_apps[i].removable) return -5;
    }

    app_store_base = base;
    app_store_size = size;
    app_store_used = 0;
    return 0;
}

/* Adds one entry: 1=added, 0=invalid/duplicate, -1=capacity or storage full. */
static int append_entry(const uint8_t *fs_data, uint32_t fs_size,
                        uint32_t table_end, const appfs_entry_t *entry,
                        int removable) {
    if (registered_count >= MAX_REGISTERED_APPS) return -1;

    /* Reject directory/self-references and validate arithmetic without overflow. */
    if (entry->offset < table_end ||
        entry->offset > fs_size ||
        entry->size > fs_size - entry->offset) {
        return 0;
    }

    const uint8_t *source_cbx = fs_data + entry->offset;
    if (!validate_cbx(source_cbx, entry->size)) return 0;

    char filename[32];
    char name[32];
    kstr_copy(filename, entry->filename, sizeof(filename));
    kstr_copy(name, entry->name, sizeof(name));

    for (int i = 0; i < registered_count; i++) {
        if (kstr_equal(registered_apps[i].filename, filename) ||
            kstr_equal(registered_apps[i].name, name)) {
            return 0;
        }
    }

    uint32_t storage_offset = 0;
    const uint8_t *app_data = source_cbx;

    if (removable) {
        if (app_store_base == NULL ||
            app_store_used > app_store_size ||
            entry->size > app_store_size - app_store_used) {
            return -1;
        }

        storage_offset = app_store_used;
        for (uint32_t i = 0; i < entry->size; i++) {
            app_store_base[storage_offset + i] = source_cbx[i];
        }
        app_data = app_store_base + storage_offset;
        app_store_used += entry->size;
    }

    registered_app_t *app = &registered_apps[registered_count];
    kstr_copy(app->filename, entry->filename, sizeof(app->filename));
    kstr_copy(app->name, entry->name, sizeof(app->name));
    kstr_copy(app->description, entry->description, sizeof(app->description));
    app->size = entry->size;
    app->data = app_data;
    app->removable = removable ? 1 : 0;
    app->storage_offset = storage_offset;
    registered_count++;
    return 1;
}

int app_registry_init(const uint8_t *fs_data, uint32_t fs_size) {
    registered_count = 0;
    external_appfs = NULL;
    external_appfs_size = 0;
    app_store_used = 0;

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

    uint32_t source_start = (uint32_t)fs_data;
    uint32_t source_end = source_start + fs_size;
    if (source_end < source_start ||
        ranges_overlap(source_start, source_end, APP_BASE_ADDRESS, APP_STACK_TOP)) {
        external_appfs = NULL;
        external_appfs_size = 0;
        return -4;
    }

    if (app_store_base != NULL) {
        uint32_t store_start = (uint32_t)app_store_base;
        uint32_t store_end = store_start + app_store_size;
        if (store_end < store_start ||
            ranges_overlap(source_start, source_end, store_start, store_end)) {
            external_appfs = NULL;
            external_appfs_size = 0;
            return -4;
        }
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
    if (app_store_base == NULL || app_store_size == 0) return -3;
    if (registered_count >= MAX_REGISTERED_APPS) return -4;

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
        if (item_result < 0) {
            if (added > 0) return added;
            return (registered_count >= MAX_REGISTERED_APPS) ? -4 : -3;
        }
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

/* External apps are copies in OS-managed RAM; removing one reclaims its bytes. */
int app_registry_remove(int index) {
    if (index < 0 || index >= registered_count) return -1;
    if (!registered_apps[index].removable) return -2;

    for (int i = index; i + 1 < registered_count; i++) {
        registered_apps[i] = registered_apps[i + 1];
    }
    registered_count--;

    /* Compact the remaining external binaries and update all their pointers. */
    uint32_t new_used = 0;
    for (int i = 0; i < registered_count; i++) {
        registered_app_t *app = &registered_apps[i];
        if (!app->removable) continue;

        uint32_t old_offset = app->storage_offset;
        if (old_offset != new_used) {
            for (uint32_t j = 0; j < app->size; j++) {
                app_store_base[new_used + j] = app_store_base[old_offset + j];
            }
        }

        app->storage_offset = new_used;
        app->data = app_store_base + new_used;
        new_used += app->size;
    }

    for (uint32_t i = new_used; i < app_store_used; i++) {
        app_store_base[i] = 0;
    }
    app_store_used = new_used;

    registered_apps[registered_count].filename[0] = '\0';
    registered_apps[registered_count].name[0] = '\0';
    registered_apps[registered_count].description[0] = '\0';
    registered_apps[registered_count].size = 0;
    registered_apps[registered_count].data = NULL;
    registered_apps[registered_count].removable = 0;
    registered_apps[registered_count].storage_offset = 0;
    return 0;
}

int app_registry_launch(int index) {
    const registered_app_t *app = app_registry_get(index);
    if (!app || !app->data) return -1;
    return app_load_and_run(app->data, app->size);
}
