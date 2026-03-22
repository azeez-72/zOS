#pragma once
#include <stdint.h>

uint32_t write(uint32_t fd, const char *buf, uint32_t n);
uint32_t read(uint32_t fd, char *buf, uint32_t n);
void     exit(void);