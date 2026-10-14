[bits 32]

global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

extern isr_dispatch
extern irq_dispatch

%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0        ; Фиктивный код ошибки для единообразия стека
    push dword %1        ; Номер прерывания
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    ; Код ошибки уже помещен в стек процессором
    push dword %1        ; Номер прерывания
    jmp isr_common_stub
%endmacro

%macro IRQ_GATE 2
global irq%1
irq%1:
    cli
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

; Исключения процессора 0..31
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; Аппаратные прерывания IRQ 0..15 (векторы 32..47)
IRQ_GATE 0,  32
IRQ_GATE 1,  33
IRQ_GATE 2,  34
IRQ_GATE 3,  35
IRQ_GATE 4,  36
IRQ_GATE 5,  37
IRQ_GATE 6,  38
IRQ_GATE 7,  39
IRQ_GATE 8,  40
IRQ_GATE 9,  41
IRQ_GATE 10, 42
IRQ_GATE 11, 43
IRQ_GATE 12, 44
IRQ_GATE 13, 45
IRQ_GATE 14, 46
IRQ_GATE 15, 47

isr_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10        ; Сегмент данных ядра
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; Передаём указатель на структуру registers_t в C
    call isr_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8          ; Удаляем err_code и int_no
    iret

irq_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret