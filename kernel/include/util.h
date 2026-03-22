#ifndef _UTIL_H
#define _UTIL_H

#include "../riscv.h"

void *memset(void *dst, int c, uint64 n);
void *memcpy(void *dst, const void *src, uint64 n);
void  panic(const char *msg);

#endif // _UTIL_H
