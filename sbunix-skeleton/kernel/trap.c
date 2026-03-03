#include "layout.h"
#include "riscv.h"
#include "include/printk.h"
#include "include/uart.h"

#define IRQ_S_EXTERNAL 9

void trap_entry(void);

void trapinit(void)
{
  // stvec points to trap entry
  stvec_write((uint64)trap_entry);

  // enable external interrupts in SIE register
  sie_write(sie_read() | SIE_SEIE);

  // global interrupt enable
  sstatus_write(sstatus_read() | SSTATUS_SIE);
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
        case IRQ_S_EXTERNAL:
          uint32 irq = *(uint32 *)(PLIC_SCLAIM);
          if (irq == UART_IRQ)
            uart_interrupt();
          
          if (irq)
            *(uint32 *)(PLIC_SCLAIM) = irq;
          break;
      }
  	} else {
      printk("something went wrong :(), nooo!! scause: %lx sepc: %lx stval: %lx",
        scause, sepc_read(), stval_read());
      for (;;) asm volatile("wfi");
    }
}
