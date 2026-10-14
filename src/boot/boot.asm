; ==============================================================================
; CustomOS - Bootstrap Loader (Multiboot Compliant)
; ==============================================================================

MB_ALIGN        equ 1 << 0             ; Выравнивание загруженных модулей по 4 КБ
MB_MEMINFO      equ 1 << 1             ; Передача карты памяти от загрузчика
MB_FLAGS        equ MB_ALIGN | MB_MEMINFO
MB_MAGIC        equ 0x1BADB002         ; Магическое число стандарта Multiboot 1
MB_CHECKSUM     equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                         ; Выделение 16 КБ под стек ядра
stack_top:

section .text
global _start
extern kmain

_start:
    ; Отключаем прерывания до инициализации IDT ядра
    cli

    ; Устанавливаем стек ядра
    mov esp, stack_top

    ; Сбрасываем регистр флагов EFLAGS
    push dword 0
    popf

    ; Передаем параметры Multiboot в функцию kmain:
    ; ebx = указатель на структуру multiboot_info
    ; eax = магическое число загрузчика (должно быть 0x2BADB002)
    push ebx
    push eax

    ; Вызываем ядро на C
    call kmain

    ; Если ядро завершило выполнение, переводим процессор в бесконечный сон
.halt_loop:
    cli
    hlt
    jmp .halt_loop