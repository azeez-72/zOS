#include <printk.h>

int console_readline(char *buf, int max) {
    int i = 0;

    while (i < max - 1) {
        char c = getc();   // already exists in printk.c

        putc(c);           // echo back

        if (c == '\n' || c == '\r')
            break;

        buf[i++] = c;
    }

    buf[i] = '\0';
    return i;
}
