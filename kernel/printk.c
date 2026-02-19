#include <printk.h>

#define UART        0x10000000
#define UART_THR    (unsigned char*)(UART+0x00) // THR:transmitter holding register
#define UART_LSR    (unsigned char*)(UART+0x05) // LSR:line status register
#define UART_LSR_EMPTY_MASK 0x40          // LSR Bit 6: Transmitter empty; both the THR and LSR are empty

#define UART_LSR_RX_READY 0x01
#define UART_RHR    (unsigned char*)(UART+0x00) // RHR:receiver holding register

char getc(void)
{
    while((*UART_LSR & UART_LSR_RX_READY) == 0);
    return *UART_RHR;
}

int putc(const char c)
{
	while((*UART_LSR & UART_LSR_EMPTY_MASK) == 0);
	return *UART_THR = c;
}

void printk(const char *fmt, ...) {
	while(*fmt)
	{
		putc(*fmt++);
	}	
}

