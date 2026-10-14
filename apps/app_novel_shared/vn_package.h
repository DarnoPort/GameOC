#ifndef VN_PACKAGE_H
#define VN_PACKAGE_H

#include <stdint.h>

#define VN_RESOURCE_MAGIC       0x314B5056u /* "VPK1" */
#define VN_RESOURCE_VERSION     1u
#define VN_CBX_MAGIC            0x31584243u /* "CBX1" */
#define VN_CBX_HEADER_BYTES     124u
#define VN_MAX_CBX_FILE_BYTES   65536u
#define VN_RESOURCE_HEADER_SIZE 16u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t source_length;
    uint32_t source_crc32;
} vn_resource_header_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    char name[32];
    char description[64];
    uint32_t entry_offset;
    uint32_t code_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t stack_size;
} vn_cbx_header_t;

typedef char vn_cbx_header_size_must_be_124[(sizeof(vn_cbx_header_t) == VN_CBX_HEADER_BYTES) ? 1 : -1];
typedef char vn_resource_header_size_must_be_16[(sizeof(vn_resource_header_t) == VN_RESOURCE_HEADER_SIZE) ? 1 : -1];

uint32_t vn_crc32(const uint8_t *bytes, uint32_t length);

/*
 * Copies a clean Player CBX template, appends the VPK1 source resource into
 * the beginning of the player's .bss address range, and adjusts data_size and
 * bss_size without changing the absolute addresses of any other data.
 */
int vn_pack_player_cbx(const uint8_t *template_bytes, uint32_t template_size,
                       const char *source, uint32_t source_length,
                       const char *app_name, const char *description,
                       uint8_t *output, uint32_t output_capacity,
                       uint32_t *output_size);

int vn_resource_validate(const uint8_t *resource, uint32_t resource_capacity,
                         const char **source_out, uint32_t *source_length_out);

#endif
