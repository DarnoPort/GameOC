#include "vn_package.h"

#include <stdio.h>
#include <string.h>

static uint8_t template_bytes[VN_CBX_HEADER_BYTES + 28u];
static uint8_t output_bytes[VN_MAX_CBX_FILE_BYTES];
static uint8_t too_long_source[48u * 1024u + 1u];
static unsigned checks = 0;
static unsigned failures = 0;

#define CHECK(condition, description) do { \
    checks++; \
    if (!(condition)) { \
        failures++; \
        printf("FAIL: %s (line %d)\n", description, __LINE__); \
    } \
} while (0)

static uint32_t text_length(const char *text) {
    uint32_t n = 0;
    while (text[n] != '\0') n++;
    return n;
}

static void make_template(void) {
    memset(template_bytes, 0, sizeof(template_bytes));
    vn_cbx_header_t *header = (vn_cbx_header_t *)template_bytes;
    header->magic = VN_CBX_MAGIC;
    header->version = 1;
    header->flags = 1;
    memcpy(header->name, "NovelPlayer", 12);
    memcpy(header->description, "Generic player template", 24);
    header->entry_offset = 0;
    header->code_size = 16;
    header->data_size = 12;
    header->bss_size = 64u * 1024u;
    header->stack_size = 16u * 1024u;
    for (uint32_t i = VN_CBX_HEADER_BYTES; i < sizeof(template_bytes); i++) {
        template_bytes[i] = (uint8_t)(i ^ 0x5Au);
    }
}

static void test_pack_round_trip(void) {
    static const char source[] =
        "label начало:\n"
        "Добро пожаловать;\n"
        "return;\n";
    uint32_t source_size = text_length(source);
    uint32_t output_size = 0;
    make_template();

    int result = vn_pack_player_cbx(template_bytes, sizeof(template_bytes),
        source, source_size, "My Novel", "Interactive novel",
        output_bytes, sizeof(output_bytes), &output_size);
    CHECK(result == 0, "pack a player template and UTF-8 scenario");
    if (result != 0) return;

    const vn_cbx_header_t *header = (const vn_cbx_header_t *)output_bytes;
    uint32_t resource_size = VN_RESOURCE_HEADER_SIZE + source_size + 1u;
    CHECK(output_size == sizeof(template_bytes) + resource_size,
          "output file contains template plus package");
    CHECK(header->data_size == 12u + resource_size,
          "package length is added to file-backed data");
    CHECK(header->bss_size == 64u * 1024u - resource_size,
          "BSS shrinks by the same byte count");
    CHECK(strcmp(header->name, "My Novel") == 0,
          "application display name is written to CBX header");
    CHECK(strcmp(header->description, "Interactive novel") == 0,
          "application description is written to CBX header");
    CHECK(memcmp(output_bytes + VN_CBX_HEADER_BYTES,
                 template_bytes + VN_CBX_HEADER_BYTES, 28u) == 0,
          "original player image is unchanged");

    const char *recovered = NULL;
    uint32_t recovered_size = 0;
    int validation = vn_resource_validate(
        output_bytes + VN_CBX_HEADER_BYTES + 28u,
        resource_size, &recovered, &recovered_size);
    CHECK(validation == 0, "resource validates after packaging");
    CHECK(recovered_size == source_size, "resource source size survives round trip");
    CHECK(recovered != NULL && memcmp(recovered, source, source_size) == 0,
          "UTF-8 source bytes survive round trip");
    CHECK(recovered != NULL && recovered[source_size] == '\0',
          "resource source is NUL terminated for runtime use");
}

static void test_reject_invalid_template_and_capacity(void) {
    static const char source[] = "label start:\nHello;\nreturn;\n";
    uint32_t output_size = 0;
    make_template();

    vn_cbx_header_t *header = (vn_cbx_header_t *)template_bytes;
    uint32_t saved_magic = header->magic;
    header->magic = 0;
    CHECK(vn_pack_player_cbx(template_bytes, sizeof(template_bytes),
        source, text_length(source), "Name", "Description",
        output_bytes, sizeof(output_bytes), &output_size) != 0,
        "reject a template with the wrong CBX signature");
    header->magic = saved_magic;

    header->bss_size = 1;
    CHECK(vn_pack_player_cbx(template_bytes, sizeof(template_bytes),
        source, text_length(source), "Name", "Description",
        output_bytes, sizeof(output_bytes), &output_size) != 0,
        "reject a template without enough BSS space");
    header->bss_size = 64u * 1024u;

    CHECK(vn_pack_player_cbx(template_bytes, sizeof(template_bytes),
        source, 0, "Name", "Description",
        output_bytes, sizeof(output_bytes), &output_size) != 0,
        "reject an empty scenario");

    CHECK(vn_pack_player_cbx(template_bytes, sizeof(template_bytes),
        source, text_length(source), "Name", "Description",
        output_bytes, 100, &output_size) != 0,
        "reject an output buffer that is too small");

    memset(too_long_source, 'x', sizeof(too_long_source));
    CHECK(vn_pack_player_cbx(template_bytes, sizeof(template_bytes),
        (const char *)too_long_source, sizeof(too_long_source),
        "Name", "Description", output_bytes, sizeof(output_bytes),
        &output_size) != 0,
        "reject a source beyond the scenario maximum");
}

static void test_reject_corrupt_resources(void) {
    const char *source = NULL;
    uint32_t length = 0;
    uint8_t resource[VN_RESOURCE_HEADER_SIZE + 5u];
    vn_resource_header_t header;
    header.magic = VN_RESOURCE_MAGIC;
    header.version = VN_RESOURCE_VERSION;
    header.source_length = 4;
    memcpy(resource, &header, sizeof(header));
    memcpy(resource + sizeof(header), "test", 4);
    resource[sizeof(header) + 4] = '\0';
    header.source_crc32 = vn_crc32(resource + sizeof(header), 4);
    memcpy(resource, &header, sizeof(header));

    CHECK(vn_resource_validate(resource, sizeof(resource), &source, &length) == 0,
          "accept valid packaged data");
    CHECK(source != NULL && length == 4 && memcmp(source, "test", 4) == 0,
          "valid package gives the expected source slice");

    resource[sizeof(header)] ^= 1u;
    CHECK(vn_resource_validate(resource, sizeof(resource), &source, &length) != 0,
          "reject a source with a mismatched CRC");
    CHECK(vn_resource_validate(resource, VN_RESOURCE_HEADER_SIZE,
          &source, &length) != 0, "reject a truncated resource");
}

int main(void) {
    test_pack_round_trip();
    test_reject_invalid_template_and_capacity();
    test_reject_corrupt_resources();
    if (failures != 0) {
        printf("vn_package_test: %u/%u checks failed.\n", failures, checks);
        return 1;
    }
    printf("vn_package_test: %u checks passed.\n", checks);
    return 0;
}
