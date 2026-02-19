#include <printk.h>
#include <consoleinput.h>

void boot(void) {
    char name[100];

    printk("Enter your name: ");
    console_readline(name, 100);

    printk("\nHello ");
    printk(name);
    printk("\n");

    while (1) {}
}
