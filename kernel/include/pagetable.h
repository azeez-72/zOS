#ifndef _PAGETABLE_H
#define _PAGETABLE_H

#include "../riscv.h"
#include <stdint.h>

pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
void kvm_init(void);
void vmprint(pagetable_t pagetable);
void unmappages(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);
void freewalk(pagetable_t pagetable);
void run_pagetable_selftest(void);

extern pagetable_t kernel_pagetable;
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);

uint64_t walkaddr(pagetable_t pgtbl, uint64_t va);
int      copyin(pagetable_t pgtbl, char *dst, uint64_t srcva, uint64_t len);
int      copyout(pagetable_t pgtbl, uint64_t dstva, char *src, uint64_t len);

#endif
