#include <printk.h>
#include "include/uart.h"
#include "include/plic.h"
#include "include/trap.h"
#include "include/proc.h"
#include "include/console.h"
#include <string.h>
#include "include/page.h"
#include "include/pagetable.h"

#define RUN_UMODE_TEST 0
#define USTACK_VA 0x80000000ULL


#include "include/vm.h"
#include "layout.h"
#include "riscv.h"

// Minimal U-mode test exercising vm_fault demand paging.
//
// Creates a vm_space with a code region but NO pre-installed PTEs.
// When the CPU enters U-mode and fetches from 0x1000, it triggers an
// instruction page fault (scause 12). vm_fault allocates a zero page
// and installs the PTE. The CPU retries, fetches 0x00000000 (illegal
// instruction), and traps with scause 2 — proving the demand-paged
// page was mapped and executed from.
#if RUN_UMODE_TEST
static void run_umode_test(void)
{
    printk("umode test: starting\n");

    // create virtual address space
    vm_space_t *space = vm_space_create();
    if (!space) panic("umode test: vm_space_create failed");

    // allocate and map trapframe
    vm_page_t *tf_pg = page_alloc_zero();
    if (!tf_pg) panic("umode test: alloc trapframe failed");
    uint64 tf_pa = page_to_pa(tf_pg);
    mappages(space->pgtbl, TRAPFRAME, PGSIZE, tf_pa, PTE_R | PTE_W);
    struct trapframe *tf = (struct trapframe *)PA_TO_KVA(tf_pa);

    // map utest code + rodata (4 pages to cover both sections)
    extern void utest(void);
    uint64_t utest_kva         = (uint64_t)utest;
    uint64_t utest_kva_aligned = utest_kva & ~(PGSIZE - 1);
    uint64_t utest_pa          = KVA_TO_PA(utest_kva_aligned);
    uint64_t offset            = utest_kva & (PGSIZE - 1);

    for (int i = 0; i < 4; i++) {
        uint64_t va = 0x1000 + (i * PGSIZE);
        uint64_t pa = utest_pa + (i * PGSIZE);
        mappages(space->pgtbl, va, PGSIZE, pa, PTE_R | PTE_X | PTE_U);
    }

    // allocate kernel stack
    vm_page_t *kstack_pg = page_alloc_zero();
    if (!kstack_pg) panic("umode test: alloc kstack failed");
    uint64_t kstack_top = PA_TO_KVA(page_to_pa(kstack_pg)) + PGSIZE;

    // allocate user stack
    vm_page_t *ustack_pg = page_alloc_zero();
    if (!ustack_pg) panic("umode test: alloc ustack failed");
    uint64_t ustack_pa = page_to_pa(ustack_pg);
    mappages(space->pgtbl, USTACK_VA, PGSIZE, ustack_pa, PTE_R | PTE_W | PTE_U);

    // fill trapframe
    tf->epc       = 0x1000 + offset;
    tf->kernel_sp = kstack_top;
    tf->sp        = USTACK_VA + PGSIZE;

    uint64_t user_satp = MAKE_SATP(KVA_TO_PA((uint64_t)space->pgtbl));
    current_vm_space = space;

    printk("umode test: epc=%lx sp=%lx\n", tf->epc, tf->sp);
    usertrapret(tf, user_satp);

    panic("umode test: usertrapret returned");
}
#endif

void test_string_functions(void) {
    char buf[32];

    // ─── strlen ───────────────────────────────────────────────
    printk("=== strlen ===\n");
    printk("strlen(hello)  = %d (expect 5)\n",  strlen("hello"));
    printk("strlen()       = %d (expect 0)\n",  strlen(""));        // empty string
    printk("strlen(a)      = %d (expect 1)\n",  strlen("a"));       // single char

    // ─── strcmp ───────────────────────────────────────────────
    printk("=== strcmp ===\n");
    printk("abc==abc       = %d (expect 0)\n",  strcmp("abc", "abc"));
    printk("abc<abd        = %d (expect <0)\n", strcmp("abc", "abd"));
    printk("abd>abc        = %d (expect >0)\n", strcmp("abd", "abc"));
    printk("abc<abcd       = %d (expect <0)\n", strcmp("abc", "abcd")); // prefix
    printk("abcd>abc       = %d (expect >0)\n", strcmp("abcd", "abc")); // prefix
    printk("empty==empty   = %d (expect 0)\n",  strcmp("", ""));        // both empty
    printk("empty<abc      = %d (expect <0)\n", strcmp("", "abc"));     // one empty

    // ─── strncmp ──────────────────────────────────────────────
    printk("=== strncmp ===\n");
    printk("abc==abd n=3   = %d (expect <0)\n", strncmp("abc", "abd", 3));
    printk("abc==abd n=2   = %d (expect 0)\n",  strncmp("abc", "abd", 2)); // only first 2
    printk("n=0            = %d (expect 0)\n",  strncmp("abc", "xyz", 0)); // zero length
    printk("prefix match   = %d (expect 0)\n",  strncmp("echo hello", "echo", 4)); // shell use case

    // ─── strcpy ───────────────────────────────────────────────
    printk("=== strcpy ===\n");
    strcpy(buf, "hello");
    printk("strcpy hello   = %s (expect hello)\n", buf);
    strcpy(buf, "");           // copy empty string
    printk("strcpy empty   = '%s' (expect '')\n", buf);
    strcpy(buf, "a");          // single char
    printk("strcpy a       = %s (expect a)\n", buf);

    // ─── strncpy ──────────────────────────────────────────────
    printk("=== strncpy ===\n");
    memset(buf, 0xFF, sizeof(buf));   // fill with garbage first
    strncpy(buf, "hello", 3);         // truncate
    printk("strncpy n=3    = %.3s (expect hel)\n", buf);  // only 3 chars valid
    memset(buf, 0xFF, sizeof(buf));
    strncpy(buf, "hi", 10);           // pad with nulls
    printk("strncpy pad    = %s (expect hi)\n", buf);

    // ─── atoi ─────────────────────────────────────────────────
    printk("=== atoi ===\n");
    printk("atoi 123       = %d (expect 123)\n",  atoi("123"));
    printk("atoi -456      = %d (expect -456)\n", atoi("-456"));
    printk("atoi +789      = %d (expect 789)\n",  atoi("+789"));
    printk("atoi 0         = %d (expect 0)\n",    atoi("0"));
    printk("atoi empty     = %d (expect 0)\n",    atoi(""));
    printk("atoi spaces    = %d (expect 42)\n",   atoi("  42"));   // leading spaces
    printk("atoi 12abc     = %d (expect 12)\n",   atoi("12abc"));  // stops at non-digit

    // ─── memset ───────────────────────────────────────────────
    printk("=== memset ===\n");
    memset(buf, 'A', 5);
    buf[5] = '\0';
    printk("memset A x5    = %s (expect AAAAA)\n", buf);
    memset(buf, 0, sizeof(buf));      // zero out
    printk("memset zero    = %d (expect 0)\n", buf[0]);

    // ─── memcmp ───────────────────────────────────────────────
    printk("=== memcmp ===\n");
    printk("equal          = %d (expect 0)\n",  memcmp("test", "test", 4));
    printk("less           = %d (expect <0)\n", memcmp("test", "tesu", 4));
    printk("greater        = %d (expect >0)\n", memcmp("tesu", "test", 4));
    printk("n=0            = %d (expect 0)\n",  memcmp("abc", "xyz", 0));  // zero bytes

    // ─── memmove ──────────────────────────────────────────────
    printk("=== memmove ===\n");
    strcpy(buf, "hello world");
    memmove(buf + 6, buf, 5);         // non-overlapping
    buf[11] = '\0';
    printk("memmove basic  = %s (expect hello hello)\n", buf);

    // overlap test — shift right within same buffer
    strcpy(buf, "12345");
    memmove(buf + 2, buf, 3);         // overlapping: copy "123" to position 2
    buf[5] = '\0';
    printk("memmove overlap= %s (expect 12123)\n", buf);

    // ─── strtol ───────────────────────────────────────────────
    printk("=== strtol ===\n");
    printk("decimal 123    = %ld (expect 123)\n",  strtol("123", 0, 10));
    printk("hex 0x1A       = %ld (expect 26)\n",   strtol("0x1A", 0, 16));
    printk("hex 0x1A auto  = %ld (expect 26)\n",   strtol("0x1A", 0, 0));  // base 0 auto-detect
    printk("octal 010      = %ld (expect 8)\n",    strtol("010", 0, 8));
    printk("negative -10   = %ld (expect -10)\n",  strtol("-10", 0, 10));
    printk("binary 1010    = %ld (expect 10)\n",   strtol("1010", 0, 2));

    // endptr test
    char *endp;
    strtol("123abc", &endp, 10);
    printk("endptr points  = %s (expect abc)\n", endp);

    // ─── strchr ───────────────────────────────────────────────
    printk("=== strchr ===\n");
    char *p = strchr("hello world", ' ');
    printk("strchr space   = %s (expect 'world')\n", p ? p+1 : "NULL");
    p = strchr("hello", 'z');
    printk("strchr missing = %s (expect NULL)\n", p ? p : "NULL");
    p = strchr("hello", 'h');
    printk("strchr first   = %s (expect hello)\n", p ? p : "NULL");

    printk("=== all tests done ===\n");
}

void boot(void) {
  printk("Booting SBUnix\n");
  printk("Hello from supervisor mode!\n");
  printk("Initializing kernel components...\n");
  test_string_functions();
  uartinit();
  consoleinit();
  page_init();
  kvm_init();
  vmprint(kernel_pagetable);
  run_pagetable_selftest();
  run_vm_selftest();
  plicinit();
  trapinit();

  // U-mode bring-up test and halts after the trap returns.
  #if RUN_UMODE_TEST
  run_umode_test();
  #endif

  procinit();
  printk("Process table initialized successfully!\n");
  printk("Kernel components initialized successfully!\n");
  userinit();
  scheduler();
}
