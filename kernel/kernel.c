#include <printk.h>

void boot(void) {
  printk("Booting SBUnix\n");
  printk("Hello World!\n");
  while(1) ;
}
