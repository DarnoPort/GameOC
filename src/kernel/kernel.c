#include "types.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "gfx.h"
#include "app_registry.h"
#include "launcher.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_INFO_MODULES     (1u << 3)
#define MULTIBOOT_MAX_MODULES      32u

/* Multiboot 1 boot information fields through mods_addr (offsets 0..24). */
typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} multiboot_info_prefix_t;

/* Multiboot 1 module descriptor. Addresses are physical addresses. */
typedef struct __attribute__((packed)) {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;

extern const uint8_t embedded_appfs_start[];
extern const uint8_t embedded_appfs_end[];

/*
 * QEMU's -initrd (or a GRUB module entry) hands a file to a Multiboot kernel
 * before kmain starts. We accept the first module whose contents identify it
 * as an AppFS image; actual CBX validation is performed by the registry.
 */
static void discover_external_appfs(uint32_t magic, uint32_t info_addr) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || info_addr == 0) return;

    const multiboot_info_prefix_t *info =
        (const multiboot_info_prefix_t *)info_addr;

    if (!(info->flags & MULTIBOOT_INFO_MODULES) ||
        info->mods_count == 0 || info->mods_addr == 0) {
        return;
    }

    uint32_t count = info->mods_count;
    if (count > MULTIBOOT_MAX_MODULES) count = MULTIBOOT_MAX_MODULES;

    const multiboot_module_t *modules =
        (const multiboot_module_t *)info->mods_addr;

    for (uint32_t i = 0; i < count; i++) {
        const multiboot_module_t *module = &modules[i];

        if (module->mod_end <= module->mod_start) continue;

        uint32_t size = module->mod_end - module->mod_start;
        if (size < sizeof(appfs_header_t)) continue;

        const uint8_t *data = (const uint8_t *)module->mod_start;
        const appfs_header_t *header = (const appfs_header_t *)data;
        if (header->magic != APPFS_MAGIC) continue;

        if (app_registry_set_external_source(data, size) >= 0) {
            return;
        }
    }
}

void kmain(uint32_t magic, uint32_t mb_info_addr) {
    /* 1. Инициализация низкоуровневых систем */
    idt_init();
    isr_init();
    pic_init();
    timer_init(100);
    keyboard_init();
    __asm__ volatile ("sti");

    /* 2. Инициализация графической подсистемы */
    gfx_init();

    /* 3. Регистрация встроенных приложений. */
    uint32_t appfs_size =
        (uint32_t)(embedded_appfs_end - embedded_appfs_start);
    app_registry_init(embedded_appfs_start, appfs_size);

    /* 4. Поиск отдельного AppFS-модуля, переданного загрузчиком. */
    discover_external_appfs(magic, mb_info_addr);

    /* 5. Запуск лаунчера. Встроенные приложения остаются доступными всегда. */
    launcher_run();

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}
