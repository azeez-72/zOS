#ifndef __ASSEMBLER__

//types

typedef unsigned long uint64;
typedef unsigned int uint32;


// ssr, sstatus
#define SSTATUS_SIE (1L << 1)	// supervisor interrupt enable
#define SSTATUS_SPIE (1L << 5)	// Supervisor Previous interrupt enable
#define SSTATUS_SPP (1L << 8)	// prev mode, 1 = s, 0 = u

static inline uint64 sstatus_read(void)
{
    uint64 sstatus;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    return sstatus;
}

static inline void sstatus_write(uint64 sstatus)
{
    asm volatile("csrw sstatus, %0" : : "r"(sstatus));
}


// supervisor interrupt enable
#define SIE_SSIE (1 << 1)
#define SIE_STIE (1 << 5)
#define SIE_SEIE (1 << 9)

static inline uint64 sie_read(void)
{
    uint64 sie;
    asm volatile("csrr %0, sie" : "=r"(sie));
    return sie;
}

static inline void sie_write(uint64 sie)
{
    asm volatile("csrw sie, %0" : : "r"(sie));
}


// scause
#define SCAUSE_INTERRUPT (1L << 63)
#define SCAUSE_CODE      0xFF

static inline uint64 scause_read(void)
{
    uint64 scause;
    asm volatile("csrr %0, scause" : "=r"(scause));
    return scause;
}


// supervisor exception program counter
static inline uint64 sepc_read(void)
{
    uint64 sepc;
    asm volatile("csrr %0, sepc" : "=r"(sepc));
    return sepc;
}

static inline void sepc_write(uint64 sepc)
{
    asm volatile("csrw sepc, %0" : : "r"(sepc));
}


// supervisor trap value
static inline uint64 stval_read(void)
{
    uint64 stval;
    asm volatile("csrr %0, stval" : "=r"(stval));
    return stval;
}


// supervisor trap vector, holds base address for s mode trap handlers
static inline void stvec_write(uint64 stvec)
{
    asm volatile("csrw stvec, %0" : : "r"(stvec));
}

static inline uint64 stvec_read()
{
    uint64 stvec;
    asm volatile("csrr %0, stvec" : "=r"(stvec));
    return stvec;
}


#endif // __ASSEMBLER__

