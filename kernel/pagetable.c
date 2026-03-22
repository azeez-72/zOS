#include <page.h>
#include <util.h>
#include "layout.h"
#include "riscv.h"
#include <printk.h>
#include <pagetable.h>

pagetable_t kernel_pagetable;

void kvm_init(void) {
    vm_page_t *pg = page_alloc_zero();
    if (pg == NULLPTR)
        panic("kvm_init: alloc root failed");
    page_wire(pg);

    uint64 root_pa = page_to_pa(pg);
    kernel_pagetable = (pagetable_t)PA_TO_KVA(root_pa);

    // Device MMIO (R/W only)
    kvmmap(kernel_pagetable, PA_TO_KVA(UART), UART, PGSIZE, PTE_R | PTE_W);
    
    // so many params :(
    kvmmap(kernel_pagetable, PA_TO_KVA(PLIC_BASE), PLIC_BASE,
           PGROUNDUP((PLIC_SCLAIM - PLIC_BASE) + sizeof(uint32)), PTE_R | PTE_W);

    // Kernel + RAM: R|W|X (no text/data split without _etext linker symbol)
    kvmmap(kernel_pagetable, PA_TO_KVA(KERN_START), KERN_START,
           PHYSTOP - KERN_START, PTE_R | PTE_W | PTE_X);

    // Trampoline: one low-VA mapping so uservec/userret survive satp switches.
    // This is the only low-half mapping in the kernel page table.
    extern char trampoline[];
    kvmmap(kernel_pagetable, TRAMPOLINE, KVA_TO_PA((uint64)trampoline),
           PGSIZE, PTE_R | PTE_X);

    // Switch satp to the new page table (satp takes PA)
    satp_write(MAKE_SATP(root_pa));
    sfence_vma();

    printk("kvm_init: kernel page table installed\n");
}

// add a mapping to the kernel page table
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    // check if error in physical page allocator
    if(mappages(kpgtbl, va, sz, pa, perm) != 0)
        panic("kvmmap");
}


// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm) {

    // verify virtual address is aligned
    if((va % PGSIZE) != 0)
        panic("mappages: va not aligned");
    // verify physical address is aligned
    if((pa%PGSIZE)!=0)
        panic("mappages: pa not aligned");

    // verify size is aligned
    if((size % PGSIZE) != 0)
        panic("mappages: size not aligned");
    if(size == 0)
        panic("mappages: size");

    // number of pages to map
    int np = size / PGSIZE;

    pte_t *pte;

    for(int i=0; i<np; i++){
        // no page table entry found
        if((pte = walk(pagetable, va, 1)) == 0)
                return -1;
        // check the V flag of PTE
        if(*pte & PTE_V)
                panic("mappages: remap");
        // store pa to this PTE with permission flags + V|A|D
        *pte = PA2PTE(pa) | perm | PTE_V | PTE_A | PTE_D;

        va += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}


// alloc is a flag that tells walk to make new intermediate tables
pte_t* walk(pagetable_t pagetable, uint64 va, int alloc){
    if (!SV39_CANONICAL(va))
        panic("walk: non-canonical VA");
    for (int level = 2; level > 0; level--){
        pte_t *pte = &pagetable[PX(level,va)];

        if (*pte & PTE_V){
            // leaf (superpage) check
            // don't descend into a superpage PTE
            if (*pte & (PTE_R | PTE_W | PTE_X))
                panic("walk: unexpected superpage");
            pagetable = (pagetable_t)PA_TO_KVA(PTE2PA(*pte));
        }
        else{
            if (alloc==0)
                return NULLPTR;

            vm_page_t *vm_page = page_alloc_zero();
            if (vm_page == NULLPTR)
                return NULLPTR;

            page_wire(vm_page);
            uint64 new_pa = page_to_pa(vm_page);
            *pte = PA2PTE(new_pa) | PTE_V;
            pagetable = (pagetable_t)PA_TO_KVA(new_pa);
        }
    }
    return &pagetable[PX(0,va)];
}


// unmap npages starting at va and panics if any page is not mapped
// if do_free is set, free the backing physical page
// Does NOT free page table pages, we use freewalk() for that
void unmappages(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
    if (va % PGSIZE != 0)
        panic("unmappages: va not aligned");

    for (uint64 i = 0; i < npages; i++) {
        pte_t *pte = walk(pagetable, va, 0);
        if (pte == NULLPTR)
            panic("unmappages: walk returned null");
        if ((*pte & PTE_V) == 0)
            panic("unmappages: page not mapped");
        if ((*pte & (PTE_R | PTE_W | PTE_X)) == 0)
            panic("unmappages: not a leaf PTE");

        if (do_free) {
            uint64 pa = PTE2PA(*pte);
            page_free(pa_to_page(pa));
        }
        *pte = 0;
        va += PGSIZE;
    }
}


// recursively free page table pages, all leaf PTEs must already
// be cleared using unmappages before calling this. 
// panics on surviving leaves and frees the passed-in page table page itself
void freewalk(pagetable_t pagetable) {
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) == 0)
            continue;
        if (pte & (PTE_R | PTE_W | PTE_X))
            panic("freewalk: leaf PTE still present");

        // non-leaf: recurse into child table page
        pagetable_t child = (pagetable_t)PA_TO_KVA(PTE2PA(pte));
        freewalk(child);
        pagetable[i] = 0;
    }

    // free this page table page (unwire first, since walk() wires PT pages)
    uint64 pa = KVA_TO_PA((uint64)pagetable);
    vm_page_t *pg = pa_to_page(pa);
    page_unwire(pg);
    page_free(pg);
}

// simple test for allocating, wiring, mapping, and unmapping
void run_pagetable_selftest(void) {
    printk("pagetable selftest: start\n");

    uint64 free_before = stats.free;
    uint64 wired_before = stats.wired;
    uint64 allocated_before = stats.allocated;

    // allocate and wire a root page table page
    vm_page_t *root_pg = page_alloc_zero();
    if (!root_pg)
        panic("selftest: alloc root failed");
    page_wire(root_pg);
    pagetable_t root = (pagetable_t)PA_TO_KVA(page_to_pa(root_pg));

    // allocate 3 data pages and map them at 0x1000, 0x2000, 0x3000
    // All share L2[0] and L1[0], so walk() creates 2 intermediate PT pages.
    // Total pages consumed: 1 root + 2 intermediate + 3 data = 6
    #define TEST_NPAGES 3
    uint64 test_va = 0x1000;

    for (int i = 0; i < TEST_NPAGES; i++) {
        vm_page_t *data_pg = page_alloc_zero();
        if (!data_pg)
            panic("selftest: alloc data page failed");
        mappages(root, test_va + i * PGSIZE, PGSIZE,
                 page_to_pa(data_pg), PTE_R | PTE_W | PTE_U);
    }

    // verify allocator counts changed
    uint64 pages_consumed = free_before - stats.free;
    printk("  pages consumed: %lx (expected 6)\n", pages_consumed);
    if (pages_consumed != 6)
        panic("selftest: unexpected page count after mapping");

    // verify the mappings exist via walk()
    for (int i = 0; i < TEST_NPAGES; i++) {
        pte_t *pte = walk(root, test_va + i * PGSIZE, 0);
        if (!pte || !(*pte & PTE_V))
            panic("selftest: mapping missing after mappages");
    }

    // unmap data pages, freeing backing pages
    unmappages(root, test_va, TEST_NPAGES, 1);

    // verify leaves are gone
    for (int i = 0; i < TEST_NPAGES; i++) {
        pte_t *pte = walk(root, test_va + i * PGSIZE, 0);
        if (pte && (*pte & PTE_V))
            panic("selftest: mapping survived unmappages");
    }

    // free the page table tree
    freewalk(root);

    // verify everything returned to baseline
    if (stats.free != free_before)
        panic("selftest: free count mismatch");
    if (stats.wired != wired_before)
        panic("selftest: wired count mismatch");
    if (stats.allocated != allocated_before)
        panic("selftest: allocated count mismatch");

    printk("pagetable selftest: passed\n");
    #undef TEST_NPAGES
}


// ============================================================
// vmprint — print page table summary
// ============================================================

static void print_flags(uint64 flags) {
    if (flags & PTE_R) printk("R");
    if (flags & PTE_W) printk("W");
    if (flags & PTE_X) printk("X");
    if (flags & PTE_U) printk("U");
    if (flags & PTE_G) printk("G");
    if (flags & PTE_A) printk("A");
    if (flags & PTE_D) printk("D");
}

static uint64 sign_extend(uint64 va) {
    if (va & (1UL << 38))
        va |= 0xFFFFFF8000000000UL;
    return va;
}

// print a summary of L0 leaf entries: collapse contiguous ranges with same flags
static void vmprint_leaves(pagetable_t pt, uint64 va_base, const char *indent) {
    int start = -1;
    uint64 start_va = 0, start_pa = 0;
    uint64 prev_flags = 0;
    int count = 0;

    for (int i = 0; i <= 512; i++) {
        uint64 va = sign_extend(va_base | ((uint64)i << PGSHIFT));
        uint64 pa = 0, flags = 0;
        int valid = 0;

        if (i < 512) {
            pte_t pte = pt[i];
            if (pte & PTE_V) {
                pa = PTE2PA(pte);
                flags = PTE_FLAGS(pte);
                valid = 1;
            }
        }

        // continue current range if contiguous with same flags
        if (valid && start >= 0 && flags == prev_flags &&
            pa == start_pa + (uint64)(i - start) * PGSIZE) {
            count++;
            continue;
        }

        // flush previous range
        if (start >= 0) {
            printk("%s", indent);
            printk("L0[%lx", (uint64)start);
            if (count > 1)
                printk("..%lx", (uint64)(start + count - 1));
            printk("]: va=0x%lx pa=0x%lx ", start_va, start_pa);
            print_flags(prev_flags);
            if (count > 1)
                printk(" (%lx pages)", (uint64)count);
            printk("\n");
        }

        // start new range
        if (valid) {
            start = i;
            start_va = va;
            start_pa = pa;
            prev_flags = flags;
            count = 1;
        } else {
            start = -1;
        }
    }
}

static void vmprint_level(pagetable_t pt, int level, uint64 va_base) {
    for (int i = 0; i < 512; i++) {
        pte_t pte = pt[i];
        if (!(pte & PTE_V))
            continue;

        uint64 va = sign_extend(va_base | ((uint64)i << PXSHIFT(level)));
        uint64 pa = PTE2PA(pte);
        uint64 flags = PTE_FLAGS(pte);
        int is_leaf = (flags & (PTE_R | PTE_W | PTE_X)) != 0;

        // indentation
        for (int d = 0; d < (2 - level); d++)
            printk("  ");

        printk("L%lx[%lx]: va=0x%lx pa=0x%lx ", (uint64)level, (uint64)i, va, pa);
        print_flags(flags);
        if (!is_leaf) printk(" (table)");
        printk("\n");

        // for non-leaf entries, recurse
        if (!is_leaf && level > 0) {
            pagetable_t child = (pagetable_t)PA_TO_KVA(pa);
            if (level == 1) {
                // L0 level: print collapsed summary
                vmprint_leaves(child, va, "    ");
            } else {
                vmprint_level(child, level - 1, va);
            }
        }
    }
}

void vmprint(pagetable_t pagetable) {
    printk("--- page table at 0x%lx ---\n", (uint64)pagetable);
    vmprint_level(pagetable, 2, 0);
    printk("--- end ---\n");
}
