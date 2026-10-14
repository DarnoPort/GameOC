#include "types.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "gfx.h"
#include "app_registry.h"
#include "launcher.h"

extern const uint8_t embedded_appfs_start[];
extern const uint8_t embedded_appfs_end[];

void kmain(uint32_t magic, uint32_t mb_info_addr) {
    (void)magic;
    (void)mb_info_addr;

    /* 1. Инициализация низкоуровневых систем */
    idt_init();
    isr_init();
    pic_init();
    timer_init(100);
    keyboard_init();
    __asm__ volatile ("sti");

    /* 2. Инициализация графической подсистемы 2 bpp */
    gfx_init();

    /* 3. Монтирование виртуального диска приложений AppFS */
    uint32_t appfs_size = (uint32_t)(embedded_appfs_end - embedded_appfs_start);
    app_registry_init(embedded_appfs_start, appfs_size);

    /* 4. Запуск графического интерфейса лаунчера CustomOS */
    launcher_run();

    /* Остановка процессора в случае выхода */
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}