#ifndef IO_H
#define IO_H

#include "types.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    /* Порт 0x80 традиционно свободен и даёт задержку ~1-2 мкс для старых контроллеров */
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif