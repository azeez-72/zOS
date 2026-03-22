#pragma once

#include "../riscv.h"
#include <stdint.h>

// protection flags (match POSIX conventions)
#define PROT_NONE   0x00
#define PROT_READ   0x01
#define PROT_WRITE  0x02
#define PROT_EXEC   0x04

// region flags
#define VM_ANON       0x01
#define VM_GROWSDOWN  0x02
#define VM_STACK      0x04

typedef struct vm_region {
    uint64_t          start;    // first VA (page-aligned, inclusive)
    uint64_t          end;      // first address past region (exclusive)
    uint32_t          prot;     // PROT_READ | PROT_WRITE | PROT_EXEC
    uint32_t          flags;    // VM_ANON, VM_STACK, etc.
    struct vm_region  *next;    // sorted by start (ascending)
    struct vm_region  *prev;
} vm_region_t;

typedef struct vm_space {
    pagetable_t       pgtbl;        // root Sv39 page table (KVA pointer)
    vm_region_t       *regions;     // sorted linked list head
    uint32_t          region_count;
    uint64_t          heap_start;
    uint64_t          heap_end;
    uint64_t          stack_top;
} vm_space_t;

void         vm_init(void);
vm_space_t  *vm_space_create(void);
void         vm_space_destroy(vm_space_t *space);
int          vm_region_insert(vm_space_t *space, uint64_t start, uint64_t end,
                              uint32_t prot, uint32_t flags);
vm_region_t *vm_region_find(vm_space_t *space, uint64_t va);
void         vm_region_remove(vm_space_t *space, vm_region_t *region);
int          vm_fault(vm_space_t *space, uint64 va, uint64 scause);
void         run_vm_selftest(void);

// Current vm_space for the active U-mode process (set before usertrapret).
extern vm_space_t *current_vm_space;
