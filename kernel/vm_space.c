#include "include/vm.h"
#include "include/page.h"
#include "include/pagetable.h"
#include "include/util.h"
#include <printk.h>
#include "layout.h"
#include "riscv.h"

#define VM_NSPACE    8
#define MAX_REGIONS  16

// static pools for now
static vm_space_t  space_pool[VM_NSPACE];
static vm_region_t region_pool[VM_NSPACE * MAX_REGIONS];
static vm_region_t *region_free_list;

void vm_init(void)
{
    region_free_list = NULLPTR;
    for (int i = VM_NSPACE * MAX_REGIONS - 1; i >= 0; i--) {
        region_pool[i].next = region_free_list;
        region_free_list = &region_pool[i];
    }
    // space_pool is zero-initialized; pgtbl == NULL means unused
}

static vm_region_t *region_alloc(void)
{
    if (!region_free_list)
        return NULLPTR;
    vm_region_t *r = region_free_list;
    region_free_list = r->next;
    memset(r, 0, sizeof(*r));
    return r;
}

static void region_free(vm_region_t *r)
{
    memset(r, 0, sizeof(*r));
    r->next = region_free_list;
    region_free_list = r;
}

vm_space_t *vm_space_create(void)
{
    // find an unused space slot
    vm_space_t *space = NULLPTR;
    for (int i = 0; i < VM_NSPACE; i++) {
        if (space_pool[i].pgtbl == NULLPTR) {
            space = &space_pool[i];
            break;
        }
    }
    if (!space)
        return NULLPTR;

    // allocate and wire root page table page
    vm_page_t *pg = page_alloc_zero();
    if (!pg)
        return NULLPTR;
    page_wire(pg);
    space->pgtbl = (pagetable_t)PA_TO_KVA(page_to_pa(pg));

    // map trampoline (shared kernel page, no PTE_U)
    extern char trampoline[];
    uint64 trampoline_pa = KVA_TO_PA((uint64)trampoline);
    if (mappages(space->pgtbl, TRAMPOLINE, PGSIZE, trampoline_pa, PTE_R | PTE_X) != 0) {
        page_unwire(pg);
        page_free(pg);
        space->pgtbl = NULLPTR;
        return NULLPTR;
    }

    space->regions = NULLPTR;
    space->region_count = 0;
    space->heap_start = 0;
    space->heap_end = 0;
    space->stack_top = 0;

    return space;
}

// Destroy an address space. The caller must unmap the TRAPFRAME page
// (do_free=0) before calling this if one was mapped.
void vm_space_destroy(vm_space_t *space)
{
    if (!space || !space->pgtbl)
        return;

    // free all mapped user pages in all regions
    vm_region_t *r = space->regions;
    while (r) {
        vm_region_t *next = r->next;
        for (uint64_t va = r->start; va < r->end; va += PGSIZE) {
            pte_t *pte = walk(space->pgtbl, va, 0);
            if (pte && (*pte & PTE_V)) {
                page_free(pa_to_page(PTE2PA(*pte)));
                *pte = 0;
            }
        }
        region_free(r);
        r = next;
    }
    space->regions = NULLPTR;
    space->region_count = 0;

    // unmap trampoline without freeing (shared kernel page)
    unmappages(space->pgtbl, TRAMPOLINE, 1, 0);

    // free page table tree 
    freewalk(space->pgtbl);

    // mark space as unused
    space->pgtbl = NULLPTR;
}

int vm_region_insert(vm_space_t *space, uint64_t start, uint64_t end,
                     uint32_t prot, uint32_t flags)
{
    if (start >= end)
        return -1;
    if (start & (PGSIZE - 1))
        return -1;
    if (end & (PGSIZE - 1))
        return -1;

    // reject overlaps with existing regions
    for (vm_region_t *r = space->regions; r; r = r->next) {
        if (start < r->end && end > r->start)
            return -1;
    }

    vm_region_t *nr = region_alloc();
    if (!nr)
        return -1;

    nr->start = start;
    nr->end   = end;
    nr->prot  = prot;
    nr->flags = flags;

    // insert into sorted position
    vm_region_t *prev = NULLPTR;
    vm_region_t *cur  = space->regions;
    while (cur && cur->start < start) {
        prev = cur;
        cur = cur->next;
    }

    nr->next = cur;
    nr->prev = prev;
    if (cur)  cur->prev = nr;
    if (prev) prev->next = nr;
    else      space->regions = nr;

    space->region_count++;
    return 0;
}

vm_region_t *vm_region_find(vm_space_t *space, uint64_t va)
{
    for (vm_region_t *r = space->regions; r; r = r->next) {
        if (va >= r->start && va < r->end)
            return r;
        // sorted list — if we've passed va, stop early
        if (r->start > va)
            break;
    }
    return NULLPTR;
}

void vm_region_remove(vm_space_t *space, vm_region_t *region)
{
    // free any mapped user pages in this region
    for (uint64_t va = region->start; va < region->end; va += PGSIZE) {
        pte_t *pte = walk(space->pgtbl, va, 0);
        if (pte && (*pte & PTE_V)) {
            page_free(pa_to_page(PTE2PA(*pte)));
            *pte = 0;
        }
    }

    // unlink from list
    if (region->prev)
        region->prev->next = region->next;
    else
        space->regions = region->next;

    if (region->next)
        region->next->prev = region->prev;

    space->region_count--;
    region_free(region);
}

void run_vm_selftest(void)
{
    printk("vm selftest: start\n");

    uint64 free_before = stats.free;

    vm_init();

    // create a space (allocates root PT + trampoline mapping)
    vm_space_t *s = vm_space_create();
    if (!s) panic("vm selftest: create failed");

    // insert 3 non-overlapping regions (metadata only, no page mappings)
    if (vm_region_insert(s, 0x1000, 0x3000, PROT_READ | PROT_EXEC, VM_ANON) != 0)
        panic("vm selftest: insert 1 failed");
    if (vm_region_insert(s, 0x5000, 0x7000, PROT_READ | PROT_WRITE, VM_ANON) != 0)
        panic("vm selftest: insert 2 failed");
    if (vm_region_insert(s, 0x9000, 0xB000, PROT_READ | PROT_WRITE, VM_ANON | VM_STACK) != 0)
        panic("vm selftest: insert 3 failed");

    if (s->region_count != 3)
        panic("vm selftest: region_count != 3");

    // find: inside regions
    if (!vm_region_find(s, 0x1000))
        panic("vm selftest: find start of region 1 failed");
    if (!vm_region_find(s, 0x2FFF))
        panic("vm selftest: find end-1 of region 1 failed");
    if (!vm_region_find(s, 0x5000))
        panic("vm selftest: find start of region 2 failed");
    if (!vm_region_find(s, 0x9500))
        panic("vm selftest: find mid of region 3 failed");

    // find: outside regions (should return NULL)
    if (vm_region_find(s, 0x0FFF))
        panic("vm selftest: below region 1 should be NULL");
    if (vm_region_find(s, 0x3000))
        panic("vm selftest: gap between 1 and 2 should be NULL");
    if (vm_region_find(s, 0x4000))
        panic("vm selftest: gap between 1 and 2 should be NULL (2)");
    if (vm_region_find(s, 0xB000))
        panic("vm selftest: past region 3 should be NULL");

    // overlap rejection
    if (vm_region_insert(s, 0x2000, 0x4000, PROT_READ, 0) == 0)
        panic("vm selftest: overlap into region 1 not rejected");
    if (vm_region_insert(s, 0x0000, 0x2000, PROT_READ, 0) == 0)
        panic("vm selftest: overlap start of region 1 not rejected");
    if (vm_region_insert(s, 0x6000, 0xA000, PROT_READ, 0) == 0)
        panic("vm selftest: overlap spanning regions 2-3 not rejected");

    // remove all regions
    while (s->regions)
        vm_region_remove(s, s->regions);

    if (s->region_count != 0)
        panic("vm selftest: region_count != 0 after remove");

    // destroy the space
    vm_space_destroy(s);

    // verify no page leaks
    if (stats.free != free_before)
        panic("vm selftest: free count mismatch");

    printk("vm selftest: passed\n");
}
