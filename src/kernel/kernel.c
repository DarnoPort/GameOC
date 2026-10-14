#include "types.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "gfx.h"
#include "app_registry.h"
#include "app_loader.h"
#include "projectfs.h"
#include "launcher.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_INFO_MEMORY      (1u << 0)
#define MULTIBOOT_INFO_MODULES     (1u << 3)
#define MULTIBOOT_INFO_MMAP        (1u << 6)
#define MULTIBOOT_MAX_MODULES      32u
#define MULTIBOOT_INFO_LEGACY_SIZE 116u

#define RAM_STORE_MAX_SIZE (12u * 1024u * 1024u)
#define RAM_STORE_GUARD    (1u * 1024u * 1024u)
#define RAM_STORE_ALIGN    0x00010000u

typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t symbols[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} multiboot_info_prefix_t;

typedef struct __attribute__((packed)) {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;

extern const uint8_t embedded_appfs_start[];
extern const uint8_t embedded_appfs_end[];
extern uint8_t _kernel_end[];

/* Round to 64 KiB, avoiding overflow. */
static uint32_t align_store_start(uint32_t address) {
    if (address > 0xFFFF0000u) return 0;
    return (address + RAM_STORE_ALIGN - 1u) & ~(RAM_STORE_ALIGN - 1u);
}

/*
 * Reserve a small OS-managed store above the kernel and every Multiboot module.
 * External CBX files are copied here on import, so removing one can really free
 * its bytes without modifying the bootloader-owned package.
 */
static void configure_app_store(uint32_t magic, uint32_t info_addr) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || info_addr == 0) return;

    const multiboot_info_prefix_t *info =
        (const multiboot_info_prefix_t *)info_addr;
    if (!(info->flags & MULTIBOOT_INFO_MEMORY)) return;

    if (info->mem_upper > (0xFFFFFFFFu - 0x00100000u) / 1024u) return;
    uint32_t memory_top = 0x00100000u + info->mem_upper * 1024u;
    if (memory_top < 0x00100000u) return;

    uint32_t highest_used = (uint32_t)_kernel_end;

    /* Account for boot information and module descriptors themselves. */
    uint32_t info_end = info_addr + MULTIBOOT_INFO_LEGACY_SIZE;
    if (info_end >= info_addr && info_end > highest_used) highest_used = info_end;

    if (info->flags & MULTIBOOT_INFO_MODULES) {
        if (info->mods_count > MULTIBOOT_MAX_MODULES || info->mods_addr == 0) {
            return;
        }

        uint32_t module_table_end =
            info->mods_addr + info->mods_count * (uint32_t)sizeof(multiboot_module_t);
        if (module_table_end < info->mods_addr) return;
        if (module_table_end > highest_used) highest_used = module_table_end;

        const multiboot_module_t *modules =
            (const multiboot_module_t *)info->mods_addr;
        for (uint32_t i = 0; i < info->mods_count; i++) {
            if (modules[i].mod_end > modules[i].mod_start &&
                modules[i].mod_end > highest_used) {
                highest_used = modules[i].mod_end;
            }
        }
    }

    if ((info->flags & MULTIBOOT_INFO_MMAP) && info->mmap_addr != 0) {
        uint32_t mmap_end = info->mmap_addr + info->mmap_length;
        if (mmap_end < info->mmap_addr) return;
        if (mmap_end > highest_used) highest_used = mmap_end;
    }

    /* App code and its dedicated stack occupy the 4 MiB..4.94 MiB arena. */
    if (highest_used < APP_STACK_TOP) highest_used = APP_STACK_TOP;

    if (highest_used > 0xFFFFFFFFu - RAM_STORE_GUARD) return;
    uint32_t store_start = align_store_start(highest_used + RAM_STORE_GUARD);
    if (store_start == 0 || store_start >= memory_top) return;

    uint32_t store_size = memory_top - store_start;
    if (store_size > RAM_STORE_MAX_SIZE) store_size = RAM_STORE_MAX_SIZE;
    store_size &= ~0xFFFu; /* page-align the pool size */

    if (store_size != 0) {
        app_registry_set_storage((uint8_t *)store_start, store_size);
    }
}

/* QEMU -initrd or GRUB modules are visible as Multiboot modules at boot. */
static void discover_external_appfs(uint32_t magic, uint32_t info_addr) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || info_addr == 0) return;

    const multiboot_info_prefix_t *info =
        (const multiboot_info_prefix_t *)info_addr;
    if (!(info->flags & MULTIBOOT_INFO_MODULES) ||
        info->mods_count == 0 || info->mods_addr == 0 ||
        info->mods_count > MULTIBOOT_MAX_MODULES) {
        return;
    }

    const multiboot_module_t *modules =
        (const multiboot_module_t *)info->mods_addr;

    for (uint32_t i = 0; i < info->mods_count; i++) {
        const multiboot_module_t *module = &modules[i];
        if (module->mod_end <= module->mod_start) continue;

        uint32_t size = module->mod_end - module->mod_start;
        if (size < sizeof(appfs_header_t)) continue;

        const uint8_t *data = (const uint8_t *)module->mod_start;
        const appfs_header_t *header = (const appfs_header_t *)data;
        if (header->magic != APPFS_MAGIC) continue;

        if (app_registry_set_external_source(data, size) >= 0) return;
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

    /* 2. Инициализация графики */
    gfx_init();

    /* 2.5. Инициализация постоянного диска. Ошибка не должна останавливать ОС. */
    (void)projectfs_init();

    /* 3. Регистрация встроенных приложений. */
    uint32_t appfs_size =
        (uint32_t)(embedded_appfs_end - embedded_appfs_start);
    app_registry_init(embedded_appfs_start, appfs_size);

    /* 4. Подготовка RAM и поиск внешнего пакета приложений. */
    configure_app_store(magic, mb_info_addr);
    discover_external_appfs(magic, mb_info_addr);

    /* 5. Запуск лаунчера. */
    launcher_run();

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}
