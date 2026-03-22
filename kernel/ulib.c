#include <ulib.h>
#include <syscall_nums.h>

// raw ecall — puts syscall number in a0, args in a1-a3
static int64_t syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    int64_t ret;
    asm volatile(
        "mv a0, %1\n"
        "mv a1, %2\n"
        "mv a2, %3\n"
        "mv a3, %4\n"
        "ecall\n"
        "mv %0, a0\n"
        : "=r"(ret)
        : "r"(num), "r"(a1), "r"(a2), "r"(a3)
        : "a0", "a1", "a2", "a3"
    );
    return ret;
}

uint32_t write(uint32_t fd, const char *buf, uint32_t n) {
    return syscall(SYS_write, fd, (uint64_t)buf, n);
}

uint32_t read(uint32_t fd, char *buf, uint32_t n) {
    return syscall(SYS_read, fd, (uint64_t)buf, n);
}

void exit(void) {
    syscall(SYS_exit, 0, 0, 0);
}