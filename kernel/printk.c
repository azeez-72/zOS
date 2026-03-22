#include <printk.h>
#include <stdint.h>
#include "../layout.h"
#include "../riscv.h"

#define UART_VA         PA_TO_KVA(UART)
#define UART_THR        (unsigned char*)(UART_VA+0x00)
#define UART_LSR        (unsigned char*)(UART_VA+0x05)
#define UART_LSR_EMPTY_MASK 0x40
#define UART_LSR_RX_READY   0x01
#define UART_RHR        (unsigned char*)(UART+0x00)

// ─── raw char I/O ─────────────────────────────────────────────────────────

char getc(void) {
    while ((*UART_LSR & UART_LSR_RX_READY) == 0);
    return *UART_RHR;
}

int putc(const char c) {
    while ((*UART_LSR & UART_LSR_EMPTY_MASK) == 0);
    return *UART_THR = c;
}

// ─── helpers ──────────────────────────────────────────────────────────────

static void print_str(const char *s) {
    if (s == 0) s = "(null)";
    while (*s) putc(*s++);
}

static void print_uint(uint64_t n, int base) {
    static const char digits[] = "0123456789abcdef";
    char buf[64];
    int i = 0;

    if (n == 0) {
        putc('0');
        return;
    }

    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }

    // buf is reversed, print it backwards
    while (i-- > 0)
        putc(buf[i]);
}

static void print_int(int64_t n) {
    if (n < 0) {
        putc('-');
        // careful: -INT64_MIN would overflow, cast to uint first
        print_uint((uint64_t)(-(n + 1)) + 1, 10);
    } else {
        print_uint((uint64_t)n, 10);
    }
}

// ─── printk ───────────────────────────────────────────────────────────────
// supported: %c  %s  %d  %u  %x  %lx  %ld  %lu  %p  %%

void printk(const char *fmt, ...) {
    // manual va_list using RISC-V calling convention
    // arguments are passed in a0-a7, then spill to stack
    // we use __builtin_va_list for portability
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            putc(*fmt++);
            continue;
        }

        fmt++; // skip '%'

        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            putc((char)__builtin_va_arg(ap, int));
            break;

        case 's':
            print_str(__builtin_va_arg(ap, char*));
            break;

        case 'd':
            if (is_long)
                print_int(__builtin_va_arg(ap, int64_t));
            else
                print_int((int64_t)__builtin_va_arg(ap, int));
            break;

        case 'u':
            if (is_long)
                print_uint(__builtin_va_arg(ap, uint64_t), 10);
            else
                print_uint((uint64_t)__builtin_va_arg(ap, unsigned int), 10);
            break;

        case 'x':
            if (is_long)
                print_uint(__builtin_va_arg(ap, uint64_t), 16);
            else
                print_uint((uint64_t)__builtin_va_arg(ap, unsigned int), 16);
            break;

        case 'p':
            print_str("0x");
            print_uint((uint64_t)__builtin_va_arg(ap, void*), 16);
            break;

        case '%':
            putc('%');
            break;

        default:
            putc('%');
            if (is_long) putc('l');
            putc(*fmt);
            break;
        }

        fmt++;
    }

    __builtin_va_end(ap);
}