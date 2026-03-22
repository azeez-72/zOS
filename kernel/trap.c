#include "layout.h"
#include "riscv.h"
#include <printk.h>
#include <uart.h>
#include <stdint.h>
#include <proc.h>
#include <console.h>
#include <vm.h>
#include <pagetable.h>

#define TIMER_INTERVAL 1000000  // cycle between preemptions in a round robin type scheduler use
#define IRQ_S_EXTERNAL 9
#define IRQ_S_TIMER    5 

void trap_entry(void);
void set_next_timer(void);
void usertrapret(struct trapframe *tf, uint64_t user_satp);

void trapinit(void)
{
  // stvec points to trap entry
  stvec_write((uint64)trap_entry);

  // enable external interrupts in SIE register
  sie_write(sie_read() | SIE_SEIE);

  // enable supervisor timer interrupts
  sie_write(sie_read() | SIE_STIE);

  // global interrupt enable
  sstatus_write(sstatus_read() | SSTATUS_SIE);

  set_next_timer();
}

void kernel_trap_handler(void *frame)
{
    uint64 scause = scause_read();  
    uint64 sstatus = sstatus_read();

	// checks
	if ((sstatus & SSTATUS_SPP) == 0){
		printk("shit not from s mode somehow");
		return;
	}	

	// TODO: should handle this better
  	if(scause & SCAUSE_INTERRUPT){
		  uint64 code = scause & SCAUSE_CODE;
      
      switch(code)
      {
        case IRQ_S_TIMER:
          set_next_timer();
          // save before yield — another process will clobber these
          uint64_t saved_sepc    = sepc_read();
          uint64_t saved_sstatus = sstatus_read();
          yield();
          // restore after yield — so sret goes back to the right place
          sepc_write(saved_sepc);
          sstatus_write(saved_sstatus);
          break;

        case IRQ_S_EXTERNAL:
          uint32_t irq = *(volatile uint32_t*)PA_TO_KVA(PLIC_SCLAIM);
          if (irq == UART_IRQ) {
              uart_interrupt();        // fills uart_rx buffer
              uint32_t c;
              while ((c = uart_getc()) != (uint32_t)-1) {  // drain the buffer
                  consoleintr(c);                 // feed each char to console
              }
          }
          if (irq)
              *(volatile uint32_t*)PA_TO_KVA(PLIC_SCLAIM) = irq;
          break;
      }
  	} else {
      printk("something went wrong :(), nooo!! scause: %lx sepc: %lx stval: %lx",
        scause, sepc_read(), stval_read());
      for (;;) asm volatile("wfi");
    }
}

// user-mode trap handling

// The trapframe, user satp, and vm_space currently in use for U-mode transitions.
// Later, replace with per-process access.
struct trapframe *current_trapframe;
uint64_t current_user_satp;
vm_space_t *current_vm_space;

// Called by uservec (trampoline.S) after switching to the kernel
// page table and kernel stack. The trampoline jumps here with jr,
// not call, so ra is a stale user value — this function must NEVER
// return normally. It must either re-enter U-mode via usertrapret
// or halt/panic.
void user_trap_handler(void)
{
    // repoint stvec immediately so any kernel interrupt goes
    // through trapentry.S, not the trampoline
    stvec_write((uint64)trap_entry);

    uint64 scause = scause_read();
    uint64 stval  = stval_read();

    // save user PC so we can resume or advance it later
    current_trapframe->epc = sepc_read();

    if (scause & SCAUSE_INTERRUPT) {
        // device or timer interrupt while in U-mode
        uint64 code = scause & SCAUSE_CODE;
        if (code == IRQ_S_TIMER) {
            set_next_timer();
            uint64_t saved_sepc    = current_trapframe->epc;
            uint64_t saved_sstatus = sstatus_read();
            yield();
            sepc_write(saved_sepc);
            sstatus_write(saved_sstatus);
        } else if (code == IRQ_S_EXTERNAL) {
            uint32_t irq = *(volatile uint32_t *)PA_TO_KVA(PLIC_SCLAIM);
            if (irq == UART_IRQ) {
                uart_interrupt();
                uint32_t c;
                while ((c = uart_getc()) != (uint32_t)-1)
                    consoleintr(c);
            }
            if (irq)
                *(volatile uint32_t *)PA_TO_KVA(PLIC_SCLAIM) = irq;
        }
        // return to U-mode after handling the interrupt
        usertrapret(current_trapframe, current_user_satp);
    } else {
        // exception from U-mode
        switch (scause) {
            case 8: // ecall from U-mode
                current_trapframe->epc += 4;

                uint64_t num = current_trapframe->a0;

                switch (num) {
                case 1: // SYS_write
                {
                    char kbuf[256];
                    uint64_t user_buf = current_trapframe->a2;
                    uint32_t n = (uint32_t)current_trapframe->a3;
                    // printk("SYS_write: user_buf=%lx n=%d\n", user_buf, n);
                    if (n > sizeof(kbuf)) n = sizeof(kbuf);
                    if (copyin(current_vm_space->pgtbl, kbuf, user_buf, n) < 0) {
                        printk("SYS_write: copyin failed\n");
                        current_trapframe->a0 = (uint64_t)-1;
                        break;
                    }
                    current_trapframe->a0 = console_write(kbuf, n);
                    break;
                }

                case 2: // SYS_read
                {
                    // printk("SYS_read: user_buf=%lx n=%d\n", 
        //    current_trapframe->a2, (uint32_t)current_trapframe->a3);
                    char kbuf[256];
                    uint64_t user_buf = current_trapframe->a2;
                    uint32_t n = (uint32_t)current_trapframe->a3;
                    if (n > sizeof(kbuf)) n = sizeof(kbuf);
                    uint32_t bytes = console_read(kbuf, n);
                    if (copyout(current_vm_space->pgtbl, user_buf, kbuf, bytes) < 0) {
                        printk("SYS_read: copyout failed\n");
                        current_trapframe->a0 = (uint64_t)-1;
                        break;
                    }
                    current_trapframe->a0 = bytes;
                    break;
                }

                case 3: // SYS_exit
                    printk("pid %d exited\n", current_proc->pid);
                    current_proc->state = P_ZOMBIE;
                    // save/restore sepc and sstatus around swtch
                    uint64_t saved_sepc    = current_trapframe->epc;
                    uint64_t saved_sstatus = sstatus_read();
                    swtch(&current_proc->ctx, &scheduler_ctx);
                    sepc_write(saved_sepc);
                    sstatus_write(saved_sstatus);
                    break;

                default:
                    printk("unknown syscall %d\n", num);
                    current_trapframe->a0 = (uint64_t)-1;
                    break;
                }

                // return to U-mode
                usertrapret(current_trapframe, current_user_satp);
                break;
            
        case 12: // instruction page fault
        case 13: // load page fault
        case 15: // store/AMO page fault
            if (current_vm_space && vm_fault(current_vm_space, stval, scause) == 0) {
                // fault resolved — return to U-mode to retry the instruction
                usertrapret(current_trapframe, current_user_satp);
            }
            // vm_fault failed: no region, wrong permissions, or OOM
            printk("user fault: unresolvable scause=%lx epc=%lx stval=%lx\n",
                   scause, current_trapframe->epc, stval);
            for (;;) asm volatile("wfi");
            break;

        default:
            printk("user trap: unhandled scause=%lx epc=%lx stval=%lx\n",
                   scause, current_trapframe->epc, stval);
            for (;;) asm volatile("wfi");
            break;
        }
    }
}

// Set up the trapframe and CPU state, then jump through the
// trampoline's userret to enter U-mode.
//
// Caller must set tf->epc (user entry point) and tf->kernel_sp
// (kernel stack top for this process) before calling.
void usertrapret(struct trapframe *tf, uint64_t user_satp)
{
    sstatus_write(sstatus_read() & ~SSTATUS_SIE);

    extern char trampoline[], uservec[], userret[];
    uint64 uservec_va = TRAMPOLINE + ((uint64)uservec - (uint64)trampoline);
    uint64 userret_va = TRAMPOLINE + ((uint64)userret - (uint64)trampoline);

    // printk("usertrapret: trampoline=%lx uservec=%lx userret=%lx\n", (uint64)trampoline, (uint64)uservec, (uint64)userret);
    // printk("usertrapret: uservec_va=%lx userret_va=%lx\n", uservec_va, userret_va);

    stvec_write(uservec_va);

    tf->kernel_satp   = satp_read();
    tf->kernel_trap   = (uint64_t)user_trap_handler;

    uint64 tp;
    asm volatile("mv %0, tp" : "=r"(tp));
    tf->kernel_hartid = tp;

    current_trapframe = tf;
    current_user_satp = user_satp;

    sepc_write(tf->epc);

    uint64 sstatus = sstatus_read();
    sstatus &= ~SSTATUS_SPP;
    sstatus |= SSTATUS_SPIE;
    sstatus_write(sstatus);

    // ← ADD THIS — initialize sscratch to TRAPFRAME
    // uservec swaps a0 with sscratch to get the trapframe pointer
    // without this, sscratch=0 and uservec writes registers to address 0
    // just before the inline asm in usertrapret
    asm volatile("csrw sscratch, %0" : : "r"((uint64_t)TRAPFRAME));

    uint64_t verify;
    asm volatile("csrr %0, sscratch" : "=r"(verify));
    // printk("sscratch after set = %lx (expect %lx)\n", verify, (uint64_t)TRAPFRAME);
    // asm volatile("csrw sscratch, %0" : : "r"((uint64_t)TRAPFRAME));

    asm volatile(
        "mv a0, %0\n"
        "mv a1, %1\n"
        "jr %2\n"
        :
        : "r"((uint64_t)TRAPFRAME),
          "r"(user_satp),
          "r"(userret_va)
        : "a0", "a1"
    );
}

void set_next_timer(void) {
    uint64_t now;
    asm volatile("csrr %0, time" : "=r"(now));
    // SBI call to set stimecmp
    asm volatile(
        "li a7, 0x54494D45\n"  // SBI extension: TIME
        "li a6, 0\n"           // function: set_timer
        "mv a0, %0\n"
        "ecall\n"
        : : "r"(now + TIMER_INTERVAL)
        : "a0", "a6", "a7"
    );
}
