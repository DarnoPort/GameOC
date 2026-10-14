[bits 32]

global app_invoke
global app_exit_trampoline

section .data
kernel_saved_esp: dd 0
kernel_saved_ebp: dd 0

section .text

; int app_invoke(uint32_t entry_point, const void *api, uint32_t app_stack);
app_invoke:
    push ebp
    mov ebp, esp

    ; Сохраняем callee-saved регистры ядра
    push ebx
    push esi
    push edi

    ; Запоминаем контекст стека ядра
    mov [kernel_saved_esp], esp
    mov [kernel_saved_ebp], ebp

    mov edx, [ebp + 8]   ; Точка входа приложения
    mov ecx, [ebp + 12]  ; Указатель на структуру API
    mov eax, [ebp + 16]  ; Вершина стека приложения

    ; Переключаемся на изолированный стек приложения
    mov esp, eax

    ; Помещаем аргумент api на стек приложения (cdecl)
    push ecx

    ; Запуск программы
    call edx

    ; Точка нормального возврата (когда app_main выполнило ret)
global app_return_from_call
app_return_from_call:
    ; В EAX уже находится код завершения
    ; Восстанавливаем оригинальный стек ядра
    mov esp, [kernel_saved_esp]
    mov ebp, [kernel_saved_ebp]

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; void app_exit_trampoline(int exit_code)
app_exit_trampoline:
    mov eax, [esp + 4]   ; Загружаем exit_code в EAX
    jmp app_return_from_call