[bits 32]

section .rodata
global embedded_app_test_start
global embedded_app_test_end

embedded_app_test_start:
    incbin "build/apps/app_test.cbx"
embedded_app_test_end: