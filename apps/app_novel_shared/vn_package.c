#include "vn_package.h"
#include <stddef.h>

static uint32_t safe_cstr_length(const char *text, uint32_t maximum) {
    uint32_t length = 0;
    if (text == NULL) return maximum + 1u;
    while (length < maximum && text[length] != '\0') length++;
    return length;
}

static void bytes_copy(uint8_t *destination, const uint8_t *source, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) destination[i] = source[i];
}

static void bytes_zero(uint8_t *destination, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) destination[i] = 0;
}

uint32_t vn_crc32(const uint8_t *bytes, uint32_t length) {
    if (bytes == NULL && length != 0) return 0;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint32_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

int vn_pack_player_cbx(const uint8_t *template_bytes, uint32_t template_size,
                       const char *source, uint32_t source_length,
                       const char *app_name, const char *description,
                       uint8_t *output, uint32_t output_capacity,
                       uint32_t *output_size) {
    if (template_bytes == NULL || source == NULL || app_name == NULL ||
        description == NULL || output == NULL || output_size == NULL) return -1;
    if (template_size < VN_CBX_HEADER_BYTES) return -2;
    if (source_length == 0 || source_length > 48u * 1024u) return -3;

    const vn_cbx_header_t *template_header =
        (const vn_cbx_header_t *)template_bytes;
    if (template_header->magic != VN_CBX_MAGIC || template_header->version != 1u ||
        template_header->code_size == 0u) return -4;

    uint32_t base_payload_size = template_header->code_size + template_header->data_size;
    if (base_payload_size < template_header->code_size ||
        base_payload_size > template_size - VN_CBX_HEADER_BYTES ||
        template_size != VN_CBX_HEADER_BYTES + base_payload_size) return -5;

    uint32_t resource_size = VN_RESOURCE_HEADER_SIZE + source_length + 1u;
    if (resource_size < source_length || template_header->bss_size < resource_size) return -6;
    if (base_payload_size > 0xFFFFFFFFu - resource_size) return -7;
    uint32_t new_payload_size = base_payload_size + resource_size;
    if (new_payload_size > output_capacity - (output_capacity >= VN_CBX_HEADER_BYTES ? VN_CBX_HEADER_BYTES : output_capacity)) return -8;
    if (VN_CBX_HEADER_BYTES + new_payload_size > VN_MAX_CBX_FILE_BYTES) return -9;
    if (output_capacity < VN_CBX_HEADER_BYTES + new_payload_size) return -10;

    uint32_t name_length = safe_cstr_length(app_name, sizeof(template_header->name) - 1u);
    uint32_t desc_length = safe_cstr_length(description, sizeof(template_header->description) - 1u);
    if (name_length > sizeof(template_header->name) - 1u ||
        desc_length > sizeof(template_header->description) - 1u) return -11;

    bytes_copy(output, template_bytes, VN_CBX_HEADER_BYTES + base_payload_size);
    vn_cbx_header_t *output_header = (vn_cbx_header_t *)output;
    output_header->data_size += resource_size;
    output_header->bss_size -= resource_size;
    bytes_zero((uint8_t *)output_header->name, sizeof(output_header->name));
    bytes_zero((uint8_t *)output_header->description, sizeof(output_header->description));
    bytes_copy((uint8_t *)output_header->name, (const uint8_t *)app_name, name_length);
    bytes_copy((uint8_t *)output_header->description, (const uint8_t *)description, desc_length);

    vn_resource_header_t resource_header;
    resource_header.magic = VN_RESOURCE_MAGIC;
    resource_header.version = VN_RESOURCE_VERSION;
    resource_header.source_length = source_length;
    resource_header.source_crc32 = vn_crc32((const uint8_t *)source, source_length);

    uint8_t *resource = output + VN_CBX_HEADER_BYTES + base_payload_size;
    bytes_copy(resource, (const uint8_t *)&resource_header, sizeof(resource_header));
    bytes_copy(resource + sizeof(resource_header), (const uint8_t *)source, source_length);
    resource[sizeof(resource_header) + source_length] = 0;

    *output_size = VN_CBX_HEADER_BYTES + new_payload_size;
    return 0;
}

int vn_resource_validate(const uint8_t *resource, uint32_t resource_capacity,
                         const char **source_out, uint32_t *source_length_out) {
    if (resource == NULL || source_out == NULL || source_length_out == NULL) return -1;
    if (resource_capacity < VN_RESOURCE_HEADER_SIZE + 1u) return -2;

    const vn_resource_header_t *header = (const vn_resource_header_t *)resource;
    if (header->magic != VN_RESOURCE_MAGIC ||
        header->version != VN_RESOURCE_VERSION ||
        header->source_length == 0 ||
        header->source_length > 48u * 1024u) return -3;

    uint32_t required = VN_RESOURCE_HEADER_SIZE + header->source_length + 1u;
    if (required > resource_capacity) return -4;
    const char *source = (const char *)(resource + VN_RESOURCE_HEADER_SIZE);
    if ((uint8_t)source[header->source_length] != 0u) return -5;
    if (vn_crc32((const uint8_t *)source, header->source_length) != header->source_crc32) return -6;

    *source_out = source;
    *source_length_out = header->source_length;
    return 0;
}
