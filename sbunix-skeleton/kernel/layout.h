// hardcoded values for device addresses

// uart
#define UART 		0x10000000
#define UART_IRQ	10

// plic
#define PLIC_BASE       0x0C000000
#define PLIC_PRIORITY   (PLIC_BASE + 0x0)
#define PLIC_PENDING    (PLIC_BASE + 0x1000)
#define PLIC_SENABLE    (PLIC_BASE + 0x2080)
#define PLIC_SPRIORITY  (PLIC_BASE + 0x201000)
#define PLIC_SCLAIM     (PLIC_BASE + 0x201004)

