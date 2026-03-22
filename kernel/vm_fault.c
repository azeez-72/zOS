#include <vm.h>
#include <page.h>
#include <pagetable.h>
#include "layout.h"
#include "riscv.h"
#include <printk.h>

// Handle a user page fault. Returns 0 if the fault was resolved
// (caller should retry the instruction), or -1 if the access is
// invalid (caller should kill the process or halt).
//
// scause values:
//   12 — instruction page fault (needs PROT_EXEC)
//   13 — load page fault        (needs PROT_READ)
//   15 — store/AMO page fault   (needs PROT_WRITE)
int vm_fault(vm_space_t *space, uint64 va, uint64 scause)
{
    if (!space)
        return -1;

    uint64 fault_va = PGROUNDDOWN(va);

    // find the region containing this address
    vm_region_t *region = vm_region_find(space, fault_va);
    if (!region)
        return -1;

    // permission check: does the region allow this type of access?
    if (scause == 12 && !(region->prot & PROT_EXEC))
        return -1;
    if (scause == 13 && !(region->prot & PROT_READ))
        return -1;
    if (scause == 15 && !(region->prot & PROT_WRITE))
        return -1;

    // already mapped? (spurious fault — just return success)
    pte_t *pte = walk(space->pgtbl, fault_va, 0);
    if (pte && (*pte & PTE_V))
        return 0;

    // allocate a zeroed physical page
    vm_page_t *pg = page_alloc_zero();
    if (!pg)
        return -1;

    // convert region prot flags to PTE flags
    int pte_flags = PTE_U;
    if (region->prot & PROT_READ)  pte_flags |= PTE_R;
    if (region->prot & PROT_WRITE) pte_flags |= PTE_W;
    if (region->prot & PROT_EXEC)  pte_flags |= PTE_X;

    // install the mapping
    if (mappages(space->pgtbl, fault_va, PGSIZE, page_to_pa(pg), pte_flags) != 0) {
        page_free(pg);
        return -1;
    }

    sfence_vma();

    printk("vm_fault: va=%lx scause=%lx -> mapped page\n", fault_va, scause);
    return 0;
}
