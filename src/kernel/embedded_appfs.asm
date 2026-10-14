[bits 32]

section .rodata
global embedded_appfs_start
global embedded_appfs_end

embedded_appfs_start:
    incbin "build/appfs.img"
embedded_appfs_end: