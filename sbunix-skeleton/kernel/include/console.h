#pragma once
#include "stdint.h"

void consoleinit(void);
void consoleintr(uint32_t c);
uint32_t  console_read(char *buf, uint32_t n);
uint32_t  console_write(const char *buf, uint32_t n);