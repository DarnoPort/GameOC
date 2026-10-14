#include "timer.h"
#include "isr.h"
#include "io.h"

#define PIT_OSC_FREQ 1193182
#define PIT_CMD_PORT 0x43
#define PIT_CH0_PORT 0x40

static volatile uint32_t timer_ticks = 0;
static uint32_t tick_frequency = 100;

static void timer_callback(registers_t *regs) {
    (void)regs;
    timer_ticks++;
}

void timer_init(uint32_t frequency_hz) {
    tick_frequency = frequency_hz;
    uint32_t divisor = PIT_OSC_FREQ / frequency_hz;

    /* Регистр управления: Канал 0, режим генератора меандра, чтение/запись LSB затем MSB */
    outb(PIT_CMD_PORT, 0x36);
    outb(PIT_CH0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    register_interrupt_handler(32, timer_callback);
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_sleep(uint32_t ms) {
    /* 1 тик = 1000 / tick_frequency миллисекунд */
    uint32_t target_ticks = timer_ticks + (ms * tick_frequency) / 1000;
    while (timer_ticks < target_ticks) {
        __asm__ volatile ("hlt");
    }
}