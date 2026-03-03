#include "layout.h"

#define UART_THR  (UART+0x00)
#define UART_RHR 	(UART+0x00) // receive holding register for input bytes
#define UART_IER	(UART+0x01)	// transmit for output bytes
#define UART_FCR	(UART+0x02)	// fifo control register
#define UART_ISR	(UART+0x02)	// interrupt status register
#define UART_LSR	(UART+0x05)	// line status register

#define UART_IER_RX_ENABLE	(1<<0)
#define UART_IER_TX_ENABLE	(1<<1)

#define UART_LSR_TX_IDLE 	(1<<5)

#define READ(reg)	(*((volatile unsigned char *)reg))
#define WRITE(reg, v)	(READ(reg) = (v))

// ring buffer for receiving characters
// TODO: implement this in console.c or some console handler when processes are implemented
#define BUF_SIZE 128
struct {
	char buf[BUF_SIZE];
	volatile unsigned int head;
	volatile unsigned int tail;
} uart_rx, uart_tx;

void uartinit(void)
{
	unsigned char fifo_enable = 1 << 0;
	unsigned char fifo_clear = (1 << 1) | (1 << 2); // clears two fifos
	
	unsigned char ier_enable = UART_IER_RX_ENABLE | UART_IER_TX_ENABLE;
	
	//disable interrupts
	WRITE(UART_IER, 0x00);
	
	// reset and enable FIFOs
	WRITE(UART_FCR, fifo_enable | fifo_clear);

	// enable transmit and receive interrupts
	WRITE(UART_IER, ier_enable);
}

void uart_interrupt(void)
{
	unsigned char rx_ready = 1 << 0; // input is waiting to be read from RHR

	READ(UART_ISR); // clear any pending interrupt conditions
	
	// RX
	while((READ(UART_LSR) & rx_ready))
	{
		char c = READ(UART_RHR);
		char next = (uart_rx.head + 1) % BUF_SIZE;
		if (next != uart_rx.tail)
		{
			uart_rx.buf[uart_rx.head] = c;
			uart_rx.head = next;
		}
	}

	// TX
	while ((READ(UART_LSR) & UART_LSR_TX_IDLE) && uart_tx.tail != uart_tx.head)
	{
			WRITE(UART_THR, uart_tx.buf[uart_tx.tail]);
			uart_tx.tail = (uart_tx.tail + 1) % BUF_SIZE;
	}
}

// returns -1 if no character available
int uart_getc(void)
{
	if (uart_rx.tail == uart_rx.head)
		return -1;

	char c = uart_rx.buf[uart_rx.tail];
	uart_rx.tail = (uart_rx.tail + 1) % BUF_SIZE;
	return c;
}

void uart_putc(char c)
{
    unsigned int next = (uart_tx.head + 1) % BUF_SIZE;

    // spin if buffer full
    while (next == uart_tx.tail)
        ;

    uart_tx.buf[uart_tx.head] = c;
    uart_tx.head = next;

    // kickstart: if THR is empty, send first byte now
    if (READ(UART_LSR) & UART_LSR_TX_IDLE) {
        if (uart_tx.tail != uart_tx.head) {
            WRITE(UART_THR, uart_tx.buf[uart_tx.tail]);
            uart_tx.tail = (uart_tx.tail + 1) % BUF_SIZE;
        }
    }
}
