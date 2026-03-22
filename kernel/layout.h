#ifndef _LAYOUT_H
#define _LAYOUT_H

// kernel virtual address layout (Sv39 high-half)

#define KERN_VA_BASE    0xFFFFFFC000000000UL

#define PA_TO_KVA(pa)   ((uint64)(pa) + KERN_VA_BASE)
#define KVA_TO_PA(va)   ((uint64)(va) - KERN_VA_BASE)
#define IS_KVA(va)      ((uint64)(va) >= KERN_VA_BASE)

#define KERNEL_L2_INDEX 256    // VPN[2] of KERN_VA_BASE

// physical device addresses

#define CLINT       0x02000000L
#define VIRTIO0     0x10001000L

#define UART        0x10000000
#define UART_IRQ    10

#define PLIC_BASE       0x0C000000
#define PLIC_PRIORITY   (PLIC_BASE + 0x0)
#define PLIC_PENDING    (PLIC_BASE + 0x1000)
#define PLIC_SENABLE    (PLIC_BASE + 0x2080)
#define PLIC_SPRIORITY  (PLIC_BASE + 0x201000)
#define PLIC_SCLAIM     (PLIC_BASE + 0x201004)

// Physical memory layout

#define RAM_BASE   0x80000000L
#define KERN_START 0x80200000L
#define PHYSTOP    0x88000000L   // 128 MB RAM

#define PGSIZE 4096
#define NPAGES     ((PHYSTOP - RAM_BASE) / PGSIZE)  // 32768 pages

// User address space top, which is Sv39 lower half
// MAXVA = 1L << 38 = 0x4000000000, defined in riscv.h
// These are mapped in user page tables without PTE_U, kernel-only pages
// at fixed VAs in the user half
#define TRAMPOLINE  (0x4000000000UL - 0x1000UL)  // MAXVA - PGSIZE
#define TRAPFRAME   (TRAMPOLINE - 0x1000UL)       // TRAMPOLINE - PGSIZE

#endif // _LAYOUT_H
