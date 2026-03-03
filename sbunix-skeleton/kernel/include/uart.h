#pragma once
void uartinit(void);
void uart_interrupt(void);
void uart_putc(char c);
int uart_getc(void);
