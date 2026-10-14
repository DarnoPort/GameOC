#include "vn_package.h"
#include "vn_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_file(const char *path, uint8_t **data_out, uint32_t *size_out) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return -1;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return -2; }
    long size = ftell(file);
    if (size <= 0 || (unsigned long)size > 1024ul * 1024ul) {
        fclose(file);
        return -3;
    }
    if (fseek(file, 0, SEEK_SET) != 0) { fclose(file); return -4; }

    uint8_t *data = (uint8_t *)malloc((size_t)size);
    if (data == NULL) { fclose(file); return -5; }
    size_t got = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) { free(data); return -6; }
    *data_out = data;
    *size_out = (uint32_t)size;
    return 0;
}

static int write_file(const char *path, const uint8_t *data, uint32_t size) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) return -1;
    size_t written = fwrite(data, 1, size, file);
    int close_result = fclose(file);
    return written == size && close_result == 0 ? 0 : -2;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s player-template.cbx story.scr output.cbx app-name\n", argv[0]);
        return 2;
    }

    uint8_t *template_bytes = NULL;
    uint8_t *source_bytes = NULL;
    uint32_t template_size = 0, source_size = 0;
    int result = read_file(argv[1], &template_bytes, &template_size);
    if (result != 0) {
        fprintf(stderr, "could not read player template (%d)\n", result);
        return 3;
    }
    result = read_file(argv[2], &source_bytes, &source_size);
    if (result != 0 || source_size > VN_MAX_SOURCE_BYTES) {
        fprintf(stderr, "could not read scenario or scenario exceeds 48 KiB (%d)\n", result);
        free(template_bytes);
        free(source_bytes);
        return 4;
    }

    uint8_t *output = (uint8_t *)malloc(VN_MAX_CBX_FILE_BYTES);
    if (output == NULL) {
        free(template_bytes);
        free(source_bytes);
        return 5;
    }

    uint32_t output_size = 0;
    result = vn_pack_player_cbx(template_bytes, template_size,
        (const char *)source_bytes, source_size, argv[4],
        "Standalone Visual Novel", output, VN_MAX_CBX_FILE_BYTES, &output_size);
    if (result != 0) {
        fprintf(stderr, "scenario package failed (%d)\n", result);
        free(template_bytes);
        free(source_bytes);
        free(output);
        return 6;
    }
    result = write_file(argv[3], output, output_size);
    if (result != 0) {
        fprintf(stderr, "could not write package (%d)\n", result);
        free(template_bytes);
        free(source_bytes);
        free(output);
        return 7;
    }

    printf("[NOVEL-PACK] %u source bytes -> %u byte CBX: %s\n",
           source_size, output_size, argv[3]);
    free(template_bytes);
    free(source_bytes);
    free(output);
    return 0;
}
