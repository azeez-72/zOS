#ifndef _PAGE_H
#define _PAGE_H

#include "../riscv.h"
#include "../layout.h"
#include "util.h"

// page states
enum page_state {
    PAGE_FREE,
    PAGE_ALLOCATED,
    PAGE_WIRED,
};

// per-physical-page metadata, one per page in the system
typedef struct vm_page {
    struct vm_page *next;
    struct vm_page *prev;
    uint64          pa;         // physical address of this page
    uint32          flags;      // enum page_state
    uint32          wire_count;
    uint32          ref_count;
    uint32          _pad;
} vm_page_t;

// page queue using doubly linked list with count
typedef struct page_queue {
    vm_page_t *head;
    vm_page_t *tail;
    uint32     count;
} page_queue_t;

// global page array, indexed by PFN
// we index it like this: (pa - RAM_BASE) / PGSIZE
extern vm_page_t *pages;

// global page queues
extern page_queue_t free_list;

// global stats
typedef struct page_stats {
    uint64 total;
    uint64 free;
    uint64 allocated;
    uint64 wired;
} page_stats_t;

extern page_stats_t stats;

// convert between physical addresses and page structs
#define PA_TO_PFN(pa)    (((uint64)(pa) - RAM_BASE) / PGSIZE)
#define PFN_TO_PA(pfn)   (RAM_BASE + ((uint64)(pfn) * PGSIZE))
#define pa_to_page(pa)   (&pages[PA_TO_PFN(pa)])
#define page_to_pa(pg)   ((pg)->pa)
#define PA_VALID(pa)     ((pa) >= RAM_BASE && (pa) < PHYSTOP && ((pa) & (PGSIZE - 1)) == 0)

// page allocator
void        page_init(void);
vm_page_t  *page_alloc(void);
vm_page_t  *page_alloc_zero(void);
void        page_free(vm_page_t *pg);
void        page_wire(vm_page_t *pg);
void        page_unwire(vm_page_t *pg);

// validate that a vm_page_t pointer is within the page array
#define PAGE_VALID(pg) ((pg) >= pages && (pg) < pages + NPAGES)

// convenience wrapper for boot-time allocations where failure is fatal
static inline vm_page_t *page_alloc_or_panic(void)
{
    vm_page_t *pg = page_alloc();
    if (!pg)
        panic("page_alloc: out of memory");
    return pg;
}

#endif // _PAGE_H
