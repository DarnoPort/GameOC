ENTRY(app_main)

SECTIONS
{
    . = 0x00400000;

    .text : ALIGN(4)
    {
        *(.text.app_main)
        *(.text*)
    }

    .rodata : ALIGN(4)
    {
        *(.rodata*)
    }

    .data : ALIGN(4)
    {
        *(.data*)
    }

    .bss : ALIGN(4)
    {
        /* Player CBX resource is appended at the first byte of the original BSS. */
        *(.bss.vn_payload)
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame)
    }
}