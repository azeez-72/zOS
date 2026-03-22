#include "include/pagetable.h"
#include "layout.h"
#include "riscv.h"
#include "include/stdint.h"
#include "include/string.h"
#include "include/util.h"
#include "include/printk.h"

// translate user virtual address to physical address
// returns 0 if not mapped or not user-accessible
uint64_t walkaddr(pagetable_t pgtbl, uint64_t va) {
    if (va >= MAXVA) return 0;
    pte_t *pte = walk(pgtbl, va, 0);
    if (pte == 0)              return 0;
    if ((*pte & PTE_V) == 0)   return 0;
    if ((*pte & PTE_U) == 0)   return 0;
    return PTE2PA(*pte);
}

// copy len bytes from user virtual address srcva into kernel buffer dst
int copyin(pagetable_t pgtbl, char *dst, uint64_t srcva, uint64_t len) {
    while (len > 0) {
        // get physical address of this page
        uint64_t pa = walkaddr(pgtbl, srcva);
        if (pa == 0) return -1;

        // how many bytes until end of this page
        uint64_t offset = srcva % PGSIZE;
        uint64_t n      = PGSIZE - offset;
        if (n > len) n = len;

        memcpy(dst, (void*)PA_TO_KVA(pa + offset), n);
        dst   += n;
        srcva += n;
        len   -= n;
    }
    return 0;
}

// copy len bytes from kernel buffer src to user virtual address dstva
int copyout(pagetable_t pgtbl, uint64_t dstva, char *src, uint64_t len) {
    // printk("copyout: dstva=%lx len=%d\n", dstva, len);
    while (len > 0) {
        uint64_t pa = walkaddr(pgtbl, dstva);
        // printk("copyout: walkaddr(%lx) = %lx\n", dstva, pa);
        if (pa == 0) return -1;

        uint64_t offset = dstva % PGSIZE;
        uint64_t n      = PGSIZE - offset;
        if (n > len) n = len;

        memcpy((void*)PA_TO_KVA(pa + offset), src, n);
        src   += n;
        dstva += n;
        len   -= n;
    }
    return 0;
}
