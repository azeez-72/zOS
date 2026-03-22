#include <console.h>
#include <stdint.h>
#include <uart.h>
#include <proc.h>
#include "riscv.h"

#define INPUT_BUF_SIZE 128
#define BACKSPACE      0x100
#define C(x)           ((x) - '@')   // ctrl+x

struct {
    char buf[INPUT_BUF_SIZE];
    volatile uint32_t r;   // r -> reads from here index
    volatile uint32_t w;   // w -> write index — committed lines end here
    volatile uint32_t e;   // edit index  — user is currently typing here
} cons;

static void cons_putc(uint32_t c) {
    if (c == BACKSPACE) {
        uart_putc('\b');
        uart_putc(' ');
        uart_putc('\b');
    } else {
        uart_putc((char)c); 
    }
}

void consoleinit(void) {
    cons.r = cons.w = cons.e = 0;
}

void consoleintr(uint32_t c) {
    switch (c) {
    case C('U'):  // ctrl+u — kill line
        while (cons.e != cons.w &&
               cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n') {
            cons.e--;
            cons_putc(BACKSPACE);
        }
        break;

    case '\x7f':  // delete key
    case '\b':    // backspace
        if (cons.e != cons.w) {
            cons.e--;
            cons_putc(BACKSPACE);
        }
        break;

    default:
        if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
            c = (c == '\r') ? '\n' : c;
            cons_putc(c);
            cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;
            if (c == '\n' || cons.e - cons.r == INPUT_BUF_SIZE) {
                cons.w = cons.e;
                wakeup(&cons);
            }
        }
        break;
    }
}

uint32_t console_write(const char *buf, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        cons_putc(buf[i]);
    return n;
}

uint32_t console_read(char *buf, uint32_t n) {
    uint32_t i = 0;
    while (i < n) {
        while (cons.r == cons.w) {
            if (current_proc != NULL) {
                sleep(&cons);
            } else {
                // no process context — enable interrupts and spin
                // so UART interrupt can fire and fill the buffer
                sstatus_write(sstatus_read() | SSTATUS_SIE);
                asm volatile("wfi");
            }
        }
        char c = cons.buf[cons.r++ % INPUT_BUF_SIZE];
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i;
}