#pragma once
#include "stdint.h"

void uartinit(void);
void uart_interrupt(void);
uint32_t  uart_getc(void);
void uart_putc(char c);