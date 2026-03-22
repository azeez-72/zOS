#include "riscv.h"
#include "layout.h"

void plicinit(void)
{
  // enable, any non-zero works
  *(volatile uint32 *)PA_TO_KVA(PLIC_BASE + UART_IRQ * 4) = 1;

  // enable UART0 for hart 0 supervisor mode, context 1
  *(volatile uint32 *)PA_TO_KVA(PLIC_SENABLE) = (1 << UART_IRQ);

  // accept all priority levels
  *(volatile uint32 *)PA_TO_KVA(PLIC_SPRIORITY) = 0;
}