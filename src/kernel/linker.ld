ENTRY(_start)

SECTIONS
{
    /* Загрузка ядра выше 1 МБ физической памяти */
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
    }

    /* Used to select safe RAM for imported application storage. */
    _kernel_end = .;
}