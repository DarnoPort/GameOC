#include "projectfs.h"
#include "ide.h"

#define PROJECTFS_VERSION       1u
#define PROJECTFS_DIR_LBA       1u
#define PROJECTFS_DIR_SECTORS   8u
#define PROJECTFS_DATA_LBA      16u
#define PROJECTFS_SLOT_SECTORS  128u
#define PROJECTFS_REQUIRED_SECTORS (PROJECTFS_DATA_LBA + PROJECTFS_MAX_FILES * PROJECTFS_SLOT_SECTORS)

#define FS_ENTRY_USED 1u

typedef struct __attribute__((packed)) {
    char magic[4];
    uint32_t version;
    uint32_t total_sectors;
    uint32_t dir_lba;
    uint32_t dir_entries;
    uint32_t dir_entry_size;
    uint32_t data_lba;
    uint32_t slot_sectors;
    uint32_t slot_count;
} projectfs_superblock_t;

typedef struct __attribute__((packed)) {
    char name[20];
    uint32_t size;
    uint32_t crc32;
    uint8_t slot;
    uint8_t flags;
    uint16_t reserved;
} projectfs_entry_t;

typedef char projectfs_entry_must_be_32_bytes[(sizeof(projectfs_entry_t) == 32) ? 1 : -1];
typedef char projectfs_superblock_must_fit_sector[(sizeof(projectfs_superblock_t) <= IDE_SECTOR_SIZE) ? 1 : -1];

static projectfs_entry_t directory[PROJECTFS_MAX_FILES];
static int filesystem_ready = 0;
static uint8_t sector_buffer[IDE_SECTOR_SIZE];

static void bytes_copy(void *dest, const void *src, uint32_t count) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < count; i++) d[i] = s[i];
}

static void bytes_zero(void *dest, uint32_t count) {
    uint8_t *d = (uint8_t *)dest;
    for (uint32_t i = 0; i < count; i++) d[i] = 0;
}

static uint32_t string_length_bounded(const char *str, uint32_t limit) {
    uint32_t length = 0;
    if (str == NULL) return limit + 1u;
    while (length < limit && str[length] != '\0') length++;
    return length;
}

static int valid_name(const char *name) {
    uint32_t length = string_length_bounded(name, PROJECTFS_NAME_MAX + 1u);
    if (length == 0 || length > PROJECTFS_NAME_MAX) return 0;

    for (uint32_t i = 0; i < length; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.')) {
            return 0;
        }
    }
    return 1;
}

static int names_equal(const char *a, const char *b) {
    for (uint32_t i = 0; i <= PROJECTFS_NAME_MAX; i++) {
        if (a[i] != b[i]) return 0;
        if (a[i] == '\0') return 1;
    }
    return 0;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

static int find_entry(const char *name) {
    for (uint32_t i = 0; i < PROJECTFS_MAX_FILES; i++) {
        if (directory[i].flags == FS_ENTRY_USED &&
            names_equal(directory[i].name, name)) return (int)i;
    }
    return -1;
}

static int find_free_entry(void) {
    for (uint32_t i = 0; i < PROJECTFS_MAX_FILES; i++) {
        if (directory[i].flags == 0) return (int)i;
    }
    return -1;
}

static int slot_is_used(uint8_t slot) {
    for (uint32_t i = 0; i < PROJECTFS_MAX_FILES; i++) {
        if (directory[i].flags == FS_ENTRY_USED && directory[i].slot == slot) return 1;
    }
    return 0;
}

static int find_free_slot(void) {
    for (uint32_t i = 0; i < PROJECTFS_MAX_FILES; i++) {
        if (!slot_is_used((uint8_t)i)) return (int)i;
    }
    return -1;
}

static int commit_entry(uint32_t index, const projectfs_entry_t *new_entry) {
    uint32_t byte_offset = index * (uint32_t)sizeof(projectfs_entry_t);
    uint32_t sector_lba = PROJECTFS_DIR_LBA + byte_offset / IDE_SECTOR_SIZE;
    uint32_t sector_offset = byte_offset % IDE_SECTOR_SIZE;

    if (ide_read_sector(sector_lba, sector_buffer) != 0) return -1;
    bytes_copy(sector_buffer + sector_offset, new_entry, sizeof(projectfs_entry_t));
    if (ide_write_sector(sector_lba, sector_buffer) != 0) return -2;
    if (ide_flush() != 0) return -3;

    directory[index] = *new_entry;
    return 0;
}

static int validate_directory(void) {
    for (uint32_t i = 0; i < PROJECTFS_MAX_FILES; i++) {
        const projectfs_entry_t *entry = &directory[i];
        if (entry->flags == 0) continue;
        if (entry->flags != FS_ENTRY_USED) return -1;
        if (entry->name[0] == '\0' ||
            string_length_bounded(entry->name, sizeof(entry->name)) >= sizeof(entry->name)) return -2;
        if (!valid_name(entry->name)) return -3;
        if (entry->size > PROJECTFS_MAX_FILE_SIZE || entry->slot >= PROJECTFS_MAX_FILES) return -4;
        for (uint32_t j = 0; j < i; j++) {
            if (directory[j].flags == FS_ENTRY_USED &&
                (names_equal(directory[j].name, entry->name) ||
                 directory[j].slot == entry->slot)) return -5;
        }
    }
    return 0;
}

int projectfs_init(void) {
    filesystem_ready = 0;
    bytes_zero(directory, sizeof(directory));

    if (ide_init() != 0) return -1;
    if (ide_read_sector(0, sector_buffer) != 0) return -2;

    const projectfs_superblock_t *sb = (const projectfs_superblock_t *)sector_buffer;
    if (sb->magic[0] != 'C' || sb->magic[1] != 'F' ||
        sb->magic[2] != 'S' || sb->magic[3] != '1') return -3;
    if (sb->version != PROJECTFS_VERSION ||
        sb->total_sectors < PROJECTFS_REQUIRED_SECTORS ||
        sb->dir_lba != PROJECTFS_DIR_LBA ||
        sb->dir_entries != PROJECTFS_MAX_FILES ||
        sb->dir_entry_size != sizeof(projectfs_entry_t) ||
        sb->data_lba != PROJECTFS_DATA_LBA ||
        sb->slot_sectors != PROJECTFS_SLOT_SECTORS ||
        sb->slot_count != PROJECTFS_MAX_FILES) return -4;

    for (uint32_t s = 0; s < PROJECTFS_DIR_SECTORS; s++) {
        if (ide_read_sector(PROJECTFS_DIR_LBA + s, sector_buffer) != 0) return -5;
        bytes_copy((uint8_t *)directory + s * IDE_SECTOR_SIZE,
                   sector_buffer, IDE_SECTOR_SIZE);
    }

    if (validate_directory() != 0) {
        bytes_zero(directory, sizeof(directory));
        return -6;
    }

    filesystem_ready = 1;
    return 0;
}

int projectfs_is_ready(void) {
    return filesystem_ready;
}

int projectfs_count(void) {
    if (!filesystem_ready) return -1;
    int count = 0;
    for (uint32_t i = 0; i < PROJECTFS_MAX_FILES; i++) {
        if (directory[i].flags == FS_ENTRY_USED) count++;
    }
    return count;
}

int projectfs_name_at(uint32_t index, char *name_out, uint32_t capacity) {
    if (!filesystem_ready || name_out == NULL || capacity == 0) return -1;
    uint32_t found = 0;
    for (uint32_t i = 0; i < PROJECTFS_MAX_FILES; i++) {
        if (directory[i].flags != FS_ENTRY_USED) continue;
        if (found++ != index) continue;

        uint32_t length = string_length_bounded(directory[i].name, sizeof(directory[i].name));
        if (length + 1u > capacity) return -2;
        bytes_copy(name_out, directory[i].name, length);
        name_out[length] = '\0';
        return 0;
    }
    return -3;
}

int32_t projectfs_size(const char *name) {
    if (!filesystem_ready || !valid_name(name)) return -1;
    int index = find_entry(name);
    if (index < 0) return -2;
    return (int32_t)directory[index].size;
}

int32_t projectfs_read(const char *name, void *buffer, uint32_t capacity) {
    if (!filesystem_ready || !valid_name(name)) return -1;
    int index = find_entry(name);
    if (index < 0) return -2;

    const projectfs_entry_t *entry = &directory[index];
    if (entry->size > capacity) return -3;
    if (entry->size > 0 && buffer == NULL) return -4;

    uint32_t remaining = entry->size;
    uint32_t copied = 0;
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t lba = PROJECTFS_DATA_LBA + (uint32_t)entry->slot * PROJECTFS_SLOT_SECTORS;
    uint8_t *out = (uint8_t *)buffer;

    for (uint32_t sector = 0; remaining > 0; sector++) {
        if (ide_read_sector(lba + sector, sector_buffer) != 0) return -5;
        uint32_t take = remaining > IDE_SECTOR_SIZE ? IDE_SECTOR_SIZE : remaining;
        crc = crc32_update(crc, sector_buffer, take);
        bytes_copy(out + copied, sector_buffer, take);
        copied += take;
        remaining -= take;
    }

    crc ^= 0xFFFFFFFFu;
    if (crc != entry->crc32) return -6;
    return (int32_t)entry->size;
}

int projectfs_write(const char *name, const void *buffer, uint32_t size) {
    if (!filesystem_ready || !valid_name(name)) return -1;
    if (size > PROJECTFS_MAX_FILE_SIZE) return -2;
    if (size > 0 && buffer == NULL) return -3;

    int old_index = find_entry(name);
    int entry_index = old_index >= 0 ? old_index : find_free_entry();
    if (entry_index < 0) return -4;

    /* Never overwrite the current slot before a replacement is verified. */
    int new_slot = find_free_slot();
    if (new_slot < 0) return -5;

    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t expected_crc = crc32_update(0xFFFFFFFFu, src, size) ^ 0xFFFFFFFFu;
    uint32_t remaining = size;
    uint32_t offset = 0;
    uint32_t lba = PROJECTFS_DATA_LBA + (uint32_t)new_slot * PROJECTFS_SLOT_SECTORS;

    for (uint32_t sector = 0; remaining > 0; sector++) {
        bytes_zero(sector_buffer, IDE_SECTOR_SIZE);
        uint32_t take = remaining > IDE_SECTOR_SIZE ? IDE_SECTOR_SIZE : remaining;
        bytes_copy(sector_buffer, src + offset, take);
        if (ide_write_sector(lba + sector, sector_buffer) != 0) return -6;
        offset += take;
        remaining -= take;
    }

    if (ide_flush() != 0) return -7;

    /* Read back and verify the bytes before changing the directory entry. */
    remaining = size;
    uint32_t verified_crc = 0xFFFFFFFFu;
    for (uint32_t sector = 0; remaining > 0; sector++) {
        if (ide_read_sector(lba + sector, sector_buffer) != 0) return -8;
        uint32_t take = remaining > IDE_SECTOR_SIZE ? IDE_SECTOR_SIZE : remaining;
        verified_crc = crc32_update(verified_crc, sector_buffer, take);
        remaining -= take;
    }
    verified_crc ^= 0xFFFFFFFFu;
    if (verified_crc != expected_crc) return -9;

    projectfs_entry_t replacement;
    bytes_zero(&replacement, sizeof(replacement));
    uint32_t name_length = string_length_bounded(name, PROJECTFS_NAME_MAX);
    bytes_copy(replacement.name, name, name_length);
    replacement.size = size;
    replacement.crc32 = expected_crc;
    replacement.slot = (uint8_t)new_slot;
    replacement.flags = FS_ENTRY_USED;

    if (commit_entry((uint32_t)entry_index, &replacement) != 0) return -10;
    return 0;
}

int projectfs_delete(const char *name) {
    if (!filesystem_ready || !valid_name(name)) return -1;
    int index = find_entry(name);
    if (index < 0) return -2;

    projectfs_entry_t empty;
    bytes_zero(&empty, sizeof(empty));
    return commit_entry((uint32_t)index, &empty);
}

int projectfs_rename(const char *old_name, const char *new_name) {
    if (!filesystem_ready || !valid_name(old_name) || !valid_name(new_name)) return -1;
    int old_index = find_entry(old_name);
    if (old_index < 0) return -2;
    if (find_entry(new_name) >= 0) return -3;

    projectfs_entry_t replacement = directory[old_index];
    bytes_zero(replacement.name, sizeof(replacement.name));
    bytes_copy(replacement.name, new_name, string_length_bounded(new_name, PROJECTFS_NAME_MAX));
    return commit_entry((uint32_t)old_index, &replacement);
}
