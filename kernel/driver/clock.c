#include <clock.h>
#include <pic_irq.h>
#include <stdio.h>
#include <trap.h>
#include <x86.h>

#define IO_TIMER1       0x040

#define TIMER_FREQ      1193182
#define TIMER_DIV(x)    ((TIMER_FREQ + (x) / 2) / (x))

#define TIMER_MODE      (IO_TIMER1 + 3)     // timer mode port
#define TIMER_SEL0      0x00
#define TIMER_RATEGEN   0x04
#define TIMER_16BIT   0x30

volatile    size_t  ticks;

void set_ticks(size_t tick) {
    ticks = tick;
}

size_t get_ticks(void) {
    return ticks;
}

void clock_init(void) {
    // set 8253 timer-chip
    // 设置定时时间为10ms
    outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
    outb(IO_TIMER1, TIMER_DIV(100) % 256);
    outb(IO_TIMER1, TIMER_DIV(100) / 256);

    // initialize time counter 'ticks' to zero
    ticks = 0;

    printk("++ setup timer interrupts\n");
    pic_enable(IRQ_TIMER);
}