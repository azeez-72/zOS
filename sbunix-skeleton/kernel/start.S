.section .text.init
.global _start
_start:
  # save arguments from opensbi
  mv s0, a0 # saving hartid
  mv s1, a1 # saving FDT pointer

  # physical address of root page table
  lla t0, initial_page_table

  # zero the root table (512 entries × 8 bytes)
  mv t2, t0
  li t3, 512
1:
  sd zero, 0(t2)
  addi t2, t2, 8
  addi t3, t3, -1
  bnez t3, 1b

  # ── Boot page table: three 1 GB gigapages at L2 level ──
  #
  # PTE format: (PPN << 10) | flags
  # Leaf PTE at L2 = 1 GB gigapage (PA must be 1 GB aligned)
  # Flags 0xCF = V|R|W|X|A|D (bits 0,1,2,3,6,7)
  #
  # root[2]   = identity map PA 0x80000000 (temporary, for boot)
  # root[256] = devices:     PA 0x00000000 → KVA 0xFFFFFFC000000000
  # root[258] = RAM:         PA 0x80000000 → KVA 0xFFFFFFC080000000

  # root[2]: identity map (temporary) — keeps PC valid during MMU enable
  # PPN = 0x80000000 >> 12 = 0x80000
  li  t2, ((0x80000 << 10) | 0xCF)
  sd  t2, (8 * 2)(t0)

  # root[256]: device region (CLINT, PLIC, UART at PA 0x00000000–0x3FFFFFFF)
  # PPN = 0, gigapage leaf
  li  t2, (0 | 0xCF)
  li  t3, (256 * 8)
  add t3, t0, t3
  sd  t2, 0(t3)

  # root[258]: RAM region (PA 0x80000000–0xBFFFFFFF)
  # PPN = 0x80000
  li  t2, ((0x80000 << 10) | 0xCF)
  li  t3, (258 * 8)
  add t3, t0, t3
  sd  t2, 0(t3)

  # enable Sv39 paging
  # satp = (mode << 60) | (root_table_PA >> 12)
  # mode 8 = Sv39
  srli t1, t0, 12   # PPN of root table
  li t2, (8 << 60)  # Sv39 mode
  or t1, t1, t2
  csrw satp, t1

  sfence.vma

  # jump to high VA
  # PC is currently at PA 0x802xxxxx
  # lla gives us the PA, add KERN_VA_BASE to get KVA
  lla t0, _start_high
  li t1, 0xFFFFFFC000000000  # KERN_VA_BASE
  add t0, t0, t1
  jr t0

_start_high:
  # running at high VA now
  # tear down identity map (root[2] = 0)
  # lla is PC-relative; PC is at high VA, so lla gives high VA
  lla t0, initial_page_table
  sd zero, (8*2)(t0)

  sfence.vma

  # set up stack
  la sp, _stack_top

  # clear bss
  la t0, _bss_start
  la t1, _kernel_end
4:
  bgeu    t0, t1, 5f
  sd      zero, 0(t0)
  addi    t0, t0, 8
  j       4b
5:
  mv a0, s0
  mv a1, s1
  call boot
  j .


# initial page table 
.section .data
.align 12
.globl initial_page_table
initial_page_table:
  .space 4096
