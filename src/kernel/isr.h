#ifndef ISR_H
#define ISR_H

#include "types.h"

/* Контекст регистров процессора, сохраняемый при входе в прерывание */
typedef struct registers {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

typedef void (*isr_handler_t)(registers_t *);

void isr_init(void);
void register_interrupt_handler(uint8_t n, isr_handler_t handler);

#endif