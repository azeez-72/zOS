#include <page.h>
#include <util.h>
#include <printk.h>

#define GARBAGE 0xDE

// global page array
vm_page_t  *pages;

// global page queues
page_queue_t free_list;

// global stats
page_stats_t stats;


static void queue_init(page_queue_t *queue)
{
  queue->head = NULLPTR;
  queue->tail = NULLPTR;
  queue->count = 0;
}

// append page to tail of queue
// right now, we only have global page queues for free lists
// but we'll expand if we used other page queues so idk
static void queue_push(page_queue_t *q, vm_page_t *page)
{
  page->next = NULLPTR;
  page->prev = q->tail;

  if (q->tail)
    q->tail->next = page;
  else
    q->head = page;  // queue was empty

  q->tail = page;
  q->count++;
}


// pop page from head
static struct vm_page *queue_pop(struct page_queue *queue)
{
  struct vm_page *pg = queue->head;
  if(!pg)
    return NULLPTR;

  queue->head = pg->next;

  if (queue->head)
    queue->head->prev = NULLPTR;
  else
    queue->tail = NULLPTR;

  pg->next = NULLPTR;
  pg->prev = NULLPTR;
  queue->count--;

  return pg;
}


__attribute__((unused))
static void queue_remove(struct page_queue *queue, struct vm_page *pg)
{
  if (pg->prev)
    pg->prev->next = pg->next;
  else
    queue->head = pg->next;

  if(pg->next)
    pg->next->prev = pg->prev;
  else
    queue->tail = pg->prev;

  pg->next = NULLPTR;
  pg->prev = NULLPTR;
  queue->count--;
}

// ============================================================
// page_init
// ============================================================
//
// Memory layout BEFORE page_init:
//
//   0x80000000  ┌────────────────────────┐  RAM_BASE
//               │ OpenSBI firmware       │  don't touch
//   0x80200000  ├────────────────────────┤  KERN_START
//               │ .text                  │  kernel code
//               ├────────────────────────┤
//               │ .rodata                │  constants, strings
//               ├────────────────────────┤
//               │ .data                  │  initialized globals
//               ├────────────────────────┤
//               │ .bss                   │  uninitialized globals
//   end ------> ├────────────────────────┤  linker symbol
//               │                        │
//               │  unclaimed RAM         │
//               │                        │
//   0x88000000  └────────────────────────┘  PHYSTOP
//
//
// Memory layout AFTER page_init:
//
//   0x80000000  ┌────────────────────────┐  RAM_BASE
//               │ OpenSBI firmware       │  WIRED
//   0x80200000  ├────────────────────────┤  KERN_START
//               │ kernel image           │  WIRED (.text/.rodata/.data/.bss)
//   end ------> ├────────────────────────┤
//               │ (padding to page       │  PGROUNDUP(end)
//               │  boundary)             │
//   pages ----> ├────────────────────────┤  page array starts here
//               │ struct page[0]         │
//               │ struct page[1]         │
//               │ ...                    │
//               │ struct page[32767]     │
//               │ (NPAGES × 40 bytes     │
//               │  ≈ 1.25 MB)            │
//               ├────────────────────────┤  PGROUNDUP(pages + NPAGES)
//   free_start  │                        │
//               │ FREE PAGES             │  this is what page_alloc()
//               │                        │  hands out
//   0x87FFFFFF  └────────────────────────┘
//
// Everything from RAM_BASE to free_start is WIRED.
// Everything from free_start to PHYSTOP goes on free_list.

extern char _kernel_end[];

void page_init(void)
{
  queue_init(&free_list);

  // _kernel_end is resolved PC-relative. Since we're running at high VA,
  // it gives a KVA. Convert to PA for arithmetic with physical addresses.
  uint64 kernel_end_pa = KVA_TO_PA(PGROUNDUP((uint64)_kernel_end));

  // place page struct array right after kernel image (in PA space)
  uint64 array_pa = kernel_end_pa;
  uint64 arr_bytes = NPAGES * sizeof(struct vm_page);
  uint64 free_start_pa = PGROUNDUP(array_pa + arr_bytes);

  if (free_start_pa >= PHYSTOP)
    panic("get more ram :sob:");

  // access the array through its KVA
  pages = (struct vm_page *)PA_TO_KVA(array_pa);
  memset(pages, 0, arr_bytes);

  // initialize page descriptors
  stats.total = NPAGES;
  stats.free = 0;
  stats.wired = 0;
  stats.allocated = 0;

  for (uint64 i = 0; i < NPAGES; i++) {
    struct vm_page *pg = &pages[i];
    uint64 pa = PFN_TO_PA(i);

    pg->pa = pa;

    if (pa < free_start_pa) {
      pg->flags = PAGE_WIRED;
      pg->wire_count = 1;
      stats.wired++;
    } else {
      memset((void *)PA_TO_KVA(pa), GARBAGE, PGSIZE);
      pg->flags = PAGE_FREE;
      queue_push(&free_list, pg);
      stats.free++;
    }
  }

  // sanity checks
  if (stats.wired + stats.free != stats.total)
    panic("page_init: wired + free != total");
  if (free_list.count != stats.free)
    panic("page_init: free_list count mismatch");
  if (stats.free == 0)
    panic("page_init: no free pages");

  printk("page_init: total  = 0x%lx\n", (uint64)stats.total);
  printk("page_init: wired  = 0x%lx\n", (uint64)stats.wired);
  printk("page_init: free   = 0x%lx\n", (uint64)stats.free);
  printk("page_init: pages@ = 0x%lx\n", (uint64)pages);
  printk("page_init: free@  = 0x%lx\n", PA_TO_KVA(free_start_pa));
  printk("page_init: end@   = 0x%lx\n", (uint64)PHYSTOP);
}


// ============================================================
// page_alloc — returns vm_page_t *, or NULLPTR on OOM
// ============================================================

vm_page_t *
page_alloc(void)
{
  vm_page_t *pg = queue_pop(&free_list);
  if (!pg)
    return NULLPTR;

  pg->flags = PAGE_ALLOCATED;
  stats.free--;
  stats.allocated++;

  return pg;
}

// ============================================================
// page_alloc_zero — returns a zeroed page
// ============================================================

vm_page_t *
page_alloc_zero(void)
{
  vm_page_t *pg = page_alloc();
  if (!pg)
    return NULLPTR;

  memset((void *)PA_TO_KVA(pg->pa), 0, PGSIZE);
  return pg;
}

// ============================================================
// page_free
// ============================================================

void
page_free(vm_page_t *pg)
{
  if (!pg)
    panic("page_free: null page");

  if (!PAGE_VALID(pg))
    panic("page_free: pg not in page array");

  if (pg->flags == PAGE_FREE)
    panic("page_free: double free");

  if (pg->flags == PAGE_WIRED)
    panic("page_free: freeing wired page");

  if (pg->flags != PAGE_ALLOCATED)
    panic("page_free: unexpected state");

  if (pg->wire_count != 0)
    panic("page_free: wire_count != 0 on allocated page");

  memset((void *)PA_TO_KVA(pg->pa), GARBAGE, PGSIZE);

  pg->flags = PAGE_FREE;
  queue_push(&free_list, pg);
  stats.allocated--;
  stats.free++;
}

// ============================================================
// page_wire / page_unwire
// ============================================================

void
page_wire(vm_page_t *pg)
{
  if (!pg)
    panic("page_wire: null page");

  if (!PAGE_VALID(pg))
    panic("page_wire: pg not in page array");

  if (pg->flags == PAGE_FREE)
    panic("page_wire: wiring a free page");

  if (pg->flags == PAGE_ALLOCATED && pg->wire_count == 0) {
    stats.allocated--;
    stats.wired++;
  }

  pg->flags = PAGE_WIRED;
  pg->wire_count++;
}

void
page_unwire(vm_page_t *pg)
{
  if (!pg)
    panic("page_unwire: null page");

  if (!PAGE_VALID(pg))
    panic("page_unwire: pg not in page array");

  if (pg->flags != PAGE_WIRED)
    panic("page_unwire: page not wired");

  if (pg->wire_count == 0)
    panic("page_unwire: wire_count already 0");

  pg->wire_count--;

  if (pg->wire_count == 0) {
    pg->flags = PAGE_ALLOCATED;
    stats.wired--;
    stats.allocated++;
  }
}
