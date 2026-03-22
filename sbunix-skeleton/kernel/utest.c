// // utest.c — runs in U-mode
// // no kernel headers — only communicates via ecall

static void sys_write(const char *buf, unsigned long n) {
    asm volatile(
        "li a0, 1\n"        // SYS_write
        "li a1, 1\n"        // fd = 1
        "mv a2, %0\n"       // buf
        "mv a3, %1\n"       // n
        "ecall\n"
        :
        : "r"(buf), "r"(n)
        : "a0", "a1", "a2", "a3"
    );
}

// static unsigned long sys_read(char *buf, unsigned long n) {
//     unsigned long ret;
//     asm volatile(
//         "li a0, 2\n"
//         "li a1, 0\n"
//         "mv a2, %1\n"
//         "mv a3, %2\n"
//         "ecall\n"
//         "mv %0, a0\n"
//         : "=r"(ret)
//         : "r"(buf), "r"(n)
//         : "a0", "a1", "a2", "a3"
//     );
//     return ret;
// }

static void sys_exit(void) {
    asm volatile(
        "li a0, 3\n"        // SYS_exit
        "ecall\n"
    );
}

// void utest(void) {
//     sys_write("=== utest: hello from U-mode! ===\n", 35);

//     sys_write("type something: ", 16);
    
//     char buf[128];
//     // zero buf so compiler knows it's initialized
//     for (int i = 0; i < 128; i++) buf[i] = 0;
    
//     unsigned long n = sys_read(buf, 128);

//     sys_write("you typed: ", 11);
//     sys_write(buf, n);

//     sys_exit();
//     while(1);
// }

// proc A — just prints and exits
void utest_a(void) {
    sys_write("Hello from U-mode process A!\n", 29);
    sys_exit();
    while(1);
}

// proc B — just prints and exits
void utest_b(void) {
    sys_write("Hello from U-mode process B!\n", 29);
    sys_exit();
    while(1);
}