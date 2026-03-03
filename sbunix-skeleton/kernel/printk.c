#include <printk.h>
#include "riscv.h"

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

#define UART        0x10000000
#define UART_THR    (unsigned char*)(UART+0x00) // THR:transmitter holding register
#define UART_LSR    (unsigned char*)(UART+0x05) // LSR:line status register
#define UART_LSR_EMPTY_MASK 0x40          // LSR Bit 6: Transmitter empty; both the THR and LSR are empty

int putc(const char c)
{
	while((*UART_LSR & UART_LSR_EMPTY_MASK) == 0);
	return *UART_THR = c;
}

static void print_string(const char *s)
{
	while(*s)
		putc(*s++);	
}

static void print_hex(uint64 x)
{
	const char *digits = "0123456789abcdef";
	char buf[16];
	int i = 0;

	if (x == 0) {
		putc('0');
		return;
	}

	while(x) {
		buf[i++] = digits[x & 0xF];
		x >>= 4;
	}

	while (i > 0)
		putc(buf[--i]);
}

void printk(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	for(; *fmt; fmt++)
	{
		if (*fmt != '%'){
			putc(*fmt);
			continue;
		}

		fmt++;

		if(*fmt == 's')
		{
			print_string(va_arg(args, const char *));
		}
		else if(*fmt == '%')
		{
			putc(*fmt);
		}
		else if(*fmt == 'l' && *(fmt+1) == 'x')
		{
			fmt++;
			print_hex(va_arg(args, uint64));
		}
	}
}

