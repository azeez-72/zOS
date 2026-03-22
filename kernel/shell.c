#include <ulib.h>
#define BUF_SIZE 128

static unsigned long ustrlen(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

static int ustrncmp(const char *a, const char *b, unsigned long n) {
    if (n == 0) return 0;
    while (n-- > 0) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (*a == 0)  return 0;
        a++; b++;
    }
    return 0;
}

static void strip_newline(char *buf) {
    unsigned long len = ustrlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
}

static void handle_command(char *buf) {
    if (ustrncmp(buf, "echo ", 5) == 0) {
        char *args = buf + 5;
        write(1, args, ustrlen(args));
        write(1, "\n", 1);
    } else if (ustrncmp(buf, "help", 4) == 0 &&
               (buf[4] == '\0' || buf[4] == ' ')) {
        write(1, "commands: echo, help\n", 21);
    } else if (ustrlen(buf) == 0) {
        // empty line, do nothing
    } else {
        write(1, "unknown command: ", 17);
        write(1, buf, ustrlen(buf));
        write(1, "\n", 1);
    }
}


void shell(void) {
    write(1, "\n--- SBUnix Shell ---\n", 21);
    write(1, "type 'help' for commands\n\n", 26);

    char buf[BUF_SIZE];

    while (1) {
        write(1, "$ ", 2);

        for (int i = 0; i < BUF_SIZE; i++) buf[i] = 0;
        read(0, buf, BUF_SIZE);

        strip_newline(buf);
        handle_command(buf);
    }
}