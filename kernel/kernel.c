#include <printk.h>
#include "include/uart.h"
#include "include/plic.h"
#include "include/trap.h"

#define LINE_MAX 256

// TODO: unfinished console, will fix later at some point
static void console(void)
{
  char line[LINE_MAX];
  int pos = 0;

  printk("> ");

  for (;;) {
    int c = uart_getc();
    if (c == -1)
      continue;

    if (c == '\r' || c == '\n') {
      printk("\n");
      line[pos] = '\0';
      printk("You said: %s\n> ", line);
      pos = 0;
    } else if (c == 127 || c == '\b') {
      // backspace
      if (pos > 0) {
        pos--;
        printk("\b \b");
      }
    } else if (pos < LINE_MAX - 1) {
      line[pos++] = c;
      // echo character
      char s[2] = {c, 0};
      printk("%s", s);
    }
  }
}

void boot(void) {
  printk("Booting SBUnix\n");
  uartinit();
  plicinit();
  trapinit();
  printk("Finished initializing\n");
  console();
}
