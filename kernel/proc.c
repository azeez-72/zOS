#include <stdint.h>
#include <printk.h>
#include <proc.h>
#include "riscv.h"
#include "include/console.h"
#include "include/shell.h"
#include "vm.h"
#include "pagetable.h"
#include "page.h"

#define NPROC       8
#define KSTACK_SIZE 4096

struct proc  proc_table[NPROC];
struct proc *current_proc = NULL;
struct context scheduler_ctx;

static uint8_t kstacks[NPROC][KSTACK_SIZE];
static int next_pid = 1;

// forward declarations
void shell(void);
void background(void);
void yield(void);

// void utest(void);
struct proc *alloc_umode_proc(void (*fn)(void));
void usertrapret(struct trapframe *tf, uint64_t satp);


void procinit(void) {
    for (int i = 0; i < NPROC; i++) {
        proc_table[i].state  = P_UNUSED;
        proc_table[i].pid    = 0;
        proc_table[i].kstack = (uint64_t)((uint8_t *)kstacks[i] + KSTACK_SIZE);
    }
}

static void umode_enter(void) {
    current_vm_space = current_proc->vm_space;   // ← add this
    usertrapret(current_proc->tf, current_proc->user_satp);
}

struct proc* alloc_proc(void (*fn)(void)) {
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc_table[i];
        if (p->state == P_UNUSED) {
            p->pid    = next_pid++;
            p->ctx.ra = (uint64_t)fn;
            p->ctx.sp = p->kstack;
            p->state  = P_RUNNABLE;
            return p;
        }
    }
    return NULL;
}

// void console_test(void) {
//     char buf[128];
//     console_write("console test: type something\n", 29);
//     int n = console_read(buf, 128);
//     console_write("you typed: ", 11);
//     console_write(buf, n);
//     // done
//     current_proc->state = P_ZOMBIE;
//     swtch(&current_proc->ctx, &scheduler_ctx);
// }


extern void utest_a(void);
extern void utest_b(void);

void userinit(void) {
    // // S-mode processes
    // struct proc *p = alloc_proc(shell);
    // if (p == NULL) printk("userinit: no slot for shell\n");

    // U-mode processes
    struct proc *a = alloc_umode_proc(utest_a);
    if (a == NULL) printk("userinit: no slot for utest_a\n");

    struct proc *b = alloc_umode_proc(utest_b);
    if (b == NULL) printk("userinit: no slot for utest_b\n");

    struct proc *p = alloc_umode_proc(shell);
    if (p == NULL) printk("userinit: no slot for shell\n");
}

void background(void) {
    while (1) {
        // count timer ticks
        static uint32_t ticks = 0;
        ticks++;
        // print every 100 yields
        if (ticks % 100 == 0)
            printk("background: tick %d\n", ticks);
        // yield back to scheduler
        current_proc->state = P_RUNNABLE;
        swtch(&current_proc->ctx, &scheduler_ctx);
    }
}


void scheduler(void) {
    while (1) {
        for (uint32_t i = 0; i < NPROC; i++) {
            struct proc *p = &proc_table[i];
            if (p->state != P_RUNNABLE) continue;

            current_proc = p;
            p->state = P_RUNNING;
            sstatus_write(sstatus_read() | SSTATUS_SIE);
            swtch(&scheduler_ctx, &p->ctx);   // ← same for both S and U mode
            sstatus_write(sstatus_read() & ~SSTATUS_SIE);
            current_proc = NULL;
        }
        sstatus_write(sstatus_read() | SSTATUS_SIE);
        asm volatile("wfi");
    }
}

void yield(void) {
    if(current_proc == NULL) return;
    current_proc->state = P_RUNNABLE;  // I'm not done, run me again later
    swtch(&current_proc->ctx, &scheduler_ctx);
}

void sleep(void *channel) {
    current_proc->channel = channel;
    current_proc->state   = P_SLEEPING;
    swtch(&current_proc->ctx, &scheduler_ctx);
}

void wakeup(void *channel) {
    for (uint32_t i = 0; i < NPROC; i++) {
        if (proc_table[i].state   == P_SLEEPING &&
            proc_table[i].channel == channel) {
            proc_table[i].state   = P_RUNNABLE;
            proc_table[i].channel = 0;
        }
    }
}


// syscall, fork and exit calls

// this code creates a new u mode process and initializes the page table and vm space for it, then jumps into the trampoline to start executing in user mode.
#define USTACK_VA 0x80000000ULL

struct proc *alloc_umode_proc(void (*fn)(void)) {
    // find free slot
    struct proc *p = NULL;
    for (int i = 0; i < NPROC; i++) {
        if (proc_table[i].state == P_UNUSED) {
            p = &proc_table[i];
            break;
        }
    }
    if (p == NULL) return NULL;

    // create address space
    vm_space_t *space = vm_space_create();
    if (!space) return NULL;

    // allocate and map trapframe
    vm_page_t *tf_pg = page_alloc_zero();
    if (!tf_pg) return NULL;
    mappages(space->pgtbl, TRAPFRAME, PGSIZE,
             page_to_pa(tf_pg), PTE_R | PTE_W);
    struct trapframe *tf = (struct trapframe *)PA_TO_KVA(page_to_pa(tf_pg));

    // map user code (4 pages to cover code + rodata)
    uint64_t fn_kva     = (uint64_t)fn;
    uint64_t fn_aligned = fn_kva & ~(PGSIZE - 1);
    uint64_t fn_pa      = KVA_TO_PA(fn_aligned);
    uint64_t offset     = fn_kva & (PGSIZE - 1);
    for (int i = 0; i < 4; i++) {
        mappages(space->pgtbl, 0x1000 + i * PGSIZE,
                 PGSIZE, fn_pa + i * PGSIZE,
                 PTE_R | PTE_X | PTE_U);
    }

    // allocate kernel stack
    vm_page_t *kstack_pg = page_alloc_zero();
    if (!kstack_pg) return NULL;
    uint64_t kstack_top = PA_TO_KVA(page_to_pa(kstack_pg)) + PGSIZE;

    // allocate user stack
    vm_page_t *ustack_pg = page_alloc_zero();
    if (!ustack_pg) return NULL;
    mappages(space->pgtbl, USTACK_VA, PGSIZE,
             page_to_pa(ustack_pg), PTE_R | PTE_W | PTE_U);

    // fill trapframe
    tf->epc       = 0x1000 + offset;
    tf->kernel_sp = kstack_top;
    tf->sp        = USTACK_VA + PGSIZE;

    // fill proc
    p->pid        = next_pid++;
    p->tf         = tf;
    p->user_satp  = MAKE_SATP(KVA_TO_PA((uint64_t)space->pgtbl));
    p->vm_space   = space;
    p->state      = P_RUNNABLE;
    p->ctx.ra = (uint64_t)umode_enter;
    p->ctx.sp = kstack_top;   // use kernel stack for the swtch
    return p;
}
