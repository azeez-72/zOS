#pragma once

#include <stdint.h>
#include "vm.h"

// Very simple process states for now.
enum proc_state {
    P_UNUSED = 0,
    P_RUNNABLE,
    P_RUNNING,
    P_SLEEPING,
    P_ZOMBIE
};

// Saved context for a kernel thread / process.
// Only callee-saved registers plus sp and ra.
struct context {
    uint64_t ra;    // return address
    uint64_t sp;    // stack pointer

    // callee-saved registers
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
};

// Trapframe: saved user register state for U-mode trap entry/exit.
// The trampoline assembly references these fields by byte offset,
// so the layout must not change without updating trampoline.S.
//
// Offsets:
//   0   kernel_satp      kernel page table for satp
//   8   kernel_sp        kernel stack pointer
//  16   kernel_trap      address of user_trap_handler
//  24   epc              saved user program counter
//  32   kernel_hartid    saved kernel tp
//  40   ra               x1
//  48   sp               x2
//  56   gp               x3
//  64   tp               x4
//  72   t0               x5
//  80   t1               x6
//  88   t2               x7
//  96   s0               x8
// 104   s1               x9
// 112   a0               x10
// 120   a1               x11
// 128   a2               x12
// 136   a3               x13
// 144   a4               x14
// 152   a5               x15
// 160   a6               x16
// 168   a7               x17
// 176   s2               x18
// 184   s3               x19
// 192   s4               x20
// 200   s5               x21
// 208   s6               x22
// 216   s7               x23
// 224   s8               x24
// 232   s9               x25
// 240   s10              x26
// 248   s11              x27
// 256   t3               x28
// 264   t4               x29
// 272   t5               x30
// 280   t6               x31
struct trapframe {
    // kernel state (filled by usertrapret before entering U-mode)
    uint64_t kernel_satp;       //   0
    uint64_t kernel_sp;         //   8
    uint64_t kernel_trap;       //  16
    uint64_t epc;               //  24
    uint64_t kernel_hartid;     //  32

    // user registers (saved by uservec on trap entry)
    uint64_t ra;                //  40
    uint64_t sp;                //  48
    uint64_t gp;                //  56
    uint64_t tp;                //  64
    uint64_t t0;                //  72
    uint64_t t1;                //  80
    uint64_t t2;                //  88
    uint64_t s0;                //  96
    uint64_t s1;                // 104
    uint64_t a0;                // 112
    uint64_t a1;                // 120
    uint64_t a2;                // 128
    uint64_t a3;                // 136
    uint64_t a4;                // 144
    uint64_t a5;                // 152
    uint64_t a6;                // 160
    uint64_t a7;                // 168
    uint64_t s2;                // 176
    uint64_t s3;                // 184
    uint64_t s4;                // 192
    uint64_t s5;                // 200
    uint64_t s6;                // 208
    uint64_t s7;                // 216
    uint64_t s8;                // 224
    uint64_t s9;                // 232
    uint64_t s10;               // 240
    uint64_t s11;               // 248
    uint64_t t3;                // 256
    uint64_t t4;                // 264
    uint64_t t5;                // 272
    uint64_t t6;                // 280
};

// Combined "process + thread" structure.
struct proc {
    int              pid;
    enum proc_state  state;
    struct context   ctx;
    uint64_t         kstack;       // top of kernel stack (for ctx.sp)
    void            *channel;      // if non-zero, sleeping on channel
    struct trapframe *tf;          // NULL for S-mode, trapframe for U-mode
    uint64_t         user_satp;    // 0 for S-mode, page table for U-mode
    vm_space_t      *vm_space;     // NULL for S-mode, address space for U-mode
};

// Context switch: save current context to old, load new, then "return" into new.
void swtch(struct context *old, struct context *new);

// Process subsystem API.
void procinit(void);
void userinit(void);
void scheduler(void);
void yield(void);
void sleep(void *channel);
void wakeup(void *channel);

extern struct proc *current_proc;
extern struct context scheduler_ctx;
