#ifndef _RISCV_H
#define _RISCV_H

#ifndef __ASSEMBLER__

//types

typedef unsigned long uint64;
typedef long int64;
typedef unsigned int uint32;


// readability
#define NULLPTR (void*)0

// page size and alignment
#define PGSIZE  4096
#define PGSHIFT 12

#define PGROUNDUP(a)   (((a) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) ((a) & ~(PGSIZE - 1))


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


// satp register (supervisor address translation and protection)
#define SATP_SV39 (8L << 60)
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)(pagetable)) >> 12))

static inline void satp_write(uint64 satp)
{
    asm volatile("csrw satp, %0" : : "r"(satp));
}

static inline uint64 satp_read(void)
{
    uint64 satp;
    asm volatile("csrr %0, satp" : "=r"(satp));
    return satp;
}

// sscratch, which holds trapframe VA when in U-mode
static inline uint64 sscratch_read(void)
{
    uint64 val;
    asm volatile("csrr %0, sscratch" : "=r"(val));
    return val;
}

static inline void sscratch_write(uint64 val)
{
    asm volatile("csrw sscratch, %0" : : "r"(val));
}

// tlb flush
static inline void sfence_vma(void)
{
    asm volatile("sfence.vma zero, zero");
}


// Sv39 page table entry (PTE) flags
#define PTE_V (1L << 0)  // valid
#define PTE_R (1L << 1)  // readable
#define PTE_W (1L << 2)  // writable
#define PTE_X (1L << 3)  // executable
#define PTE_U (1L << 4)  // user accessible
#define PTE_G (1L << 5)  // global
#define PTE_A (1L << 6)  // accessed
#define PTE_D (1L << 7)  // dirty

typedef uint64 pte_t;
typedef uint64 *pagetable_t;  // pointer to a page of 512 PTEs

// PTE <-> physical address conversion
// PA bits [55:12] are stored in PTE bits [53:10]
#define PA2PTE(pa)     ((((uint64)(pa)) >> 12) << 10)
#define PTE2PA(pte)    (((pte) >> 10) << 12)
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// extract the three 9-bit page table indices from a virtual address
#define PXMASK  0x1FF
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va)  ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)

// Maximum user virtual address under Sv39 (low half)
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

// Sv39 canonical address check: bits 63:39 must all equal bit 38
#define SV39_CANONICAL(va) \
  (((int64)(va) << 25 >> 25) == (int64)(va))


#endif // __ASSEMBLER__

#endif // _RISCV_H

