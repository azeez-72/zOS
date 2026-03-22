#include <printk.h>
#include "riscv.h"

void *memset(void *dst, int c, uint64 n)
{
  char *d = (char *)dst;
  for (uint64 i = 0; i < n; i++)
    d[i] = (char)c;
  return dst;
}

void *memcpy(void *dst, const void *src, uint64 n)
{
  char *d = (char *)dst;
  const char *s = (const char *)src;
  for (uint64 i = 0; i < n; i++)
    d[i] = s[i];
  return dst;
}

void panic(const char *msg)
{
  printk("PANIC: %s\n", msg);
  for (;;)
    ;
}
