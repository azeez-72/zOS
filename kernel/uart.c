#include "layout.h"
#include "riscv.h"
#include <stdint.h>

#define UART_BASE_VA PA_TO_KVA(UART)
#define UART_THR  (UART_BASE_VA+0x00)   // transmitter holding register
#define UART_RHR  (UART_BASE_VA+0x00)   // receiver holding register
#define UART_IER  (UART_BASE_VA+0x01)   // interrupt enable register
#define UART_FCR  (UART_BASE_VA+0x02)   // fifo control register
#define UART_ISR  (UART_BASE_VA+0x02)   // interrupt status register
#define UART_LSR  (UART_BASE_VA+0x05)   // line status register

#define UART_IER_RX_ENABLE  (1<<0)
#define UART_LSR_RX_READY   (1<<0)
#define UART_LSR_TX_IDLE    (1<<5)

#define READ(reg)       (*((volatile unsigned char *)(reg)))
#define WRITE(reg, v)   (READ(reg) = (v))

// ─── RX ring buffer ───────────────────────────────────────────────────────

#define BUF_SIZE 128

struct {
    char buf[BUF_SIZE];
    volatile unsigned int head;
    volatile unsigned int tail;
} uart_rx;

// ─── init ─────────────────────────────────────────────────────────────────

void uartinit(void) {
    unsigned char fifo_enable = 1 << 0;
    unsigned char fifo_clear  = (1 << 1) | (1 << 2);  // clear RX and TX FIFOs

    WRITE(UART_IER, 0x00);                      // disable all interrupts
    WRITE(UART_FCR, fifo_enable | fifo_clear);  // reset and enable FIFOs
    WRITE(UART_IER, UART_IER_RX_ENABLE);        // enable RX interrupt only
}

// ─── interrupt handler ────────────────────────────────────────────────────
// called by trap.c on UART IRQ

void uart_interrupt(void) {
    READ(UART_ISR);  // clear interrupt

    while (READ(UART_LSR) & UART_LSR_RX_READY) {
        char c = READ(UART_RHR);
        // sanity check — ignore null bytes and garbage
        if (c == 0) break;
        uint32_t next = (uart_rx.head + 1) % BUF_SIZE;
        if (next != uart_rx.tail) {
            uart_rx.buf[uart_rx.head] = c;
            uart_rx.head = next;
        }
    }
}

// ─── getc ─────────────────────────────────────────────────────────────────
// returns (uint32_t)-1 if no character available

uint32_t uart_getc(void) {
    if (uart_rx.tail == uart_rx.head)
        return (uint32_t)-1;
    char c     = uart_rx.buf[uart_rx.tail];
    uart_rx.tail = (uart_rx.tail + 1) % BUF_SIZE;
    return (uint32_t)(unsigned char)c;
}

// ─── putc ─────────────────────────────────────────────────────────────────
// synchronous — spins until UART is ready, then sends directly

void uart_putc(char c) {
    while (!(READ(UART_LSR) & UART_LSR_TX_IDLE));
    WRITE(UART_THR, c);
}