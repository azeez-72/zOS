#include <printk.h>
#include <uart.h>
#include <plic.h>
#include <trap.h>
#include <proc.h>
#include <console.h>
#include <page.h>
#include <pagetable.h>
#include <vm.h>
#include "layout.h"
#include "riscv.h"

void main(void) {
    printk("Booting SBUnix\n");
    uartinit();
    consoleinit();
    page_init();
    kvm_init();
    plicinit();
    trapinit();
    procinit();
    printk("Kernel initialized\n");
    userinit();
    scheduler();
}
