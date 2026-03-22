#
# trampoline.S — user/kernel transition code
#
# This page is mapped at the same virtual address (TRAMPOLINE) in both
# user and kernel page tables, so execution continues across satp switches.
#
# Must fit within a single 4096-byte page.
#

# Trapframe field offsets
#define TF_KERNEL_SATP    0
#define TF_KERNEL_SP      8
#define TF_KERNEL_TRAP   16
#define TF_EPC           24
#define TF_KERNEL_HARTID 32
#define TF_RA            40
#define TF_SP            48
#define TF_GP            56
#define TF_TP            64
#define TF_T0            72
#define TF_T1            80
#define TF_T2            88
#define TF_S0            96
#define TF_S1           104
#define TF_A0           112
#define TF_A1           120
#define TF_A2           128
#define TF_A3           136
#define TF_A4           144
#define TF_A5           152
#define TF_A6           160
#define TF_A7           168
#define TF_S2           176
#define TF_S3           184
#define TF_S4           192
#define TF_S5           200
#define TF_S6           208
#define TF_S7           216
#define TF_S8           224
#define TF_S9           232
#define TF_S10          240
#define TF_S11          248
#define TF_T3           256
#define TF_T4           264
#define TF_T5           272
#define TF_T6           280

.section .text.trampoline, "ax"
.balign 4096
.globl trampoline
trampoline:

# uservec - trap entry from U-mode
#
# CPU jumps here via stvec when a trap fires in U-mode.
# The USER page table is still active.
#
# sscratch = TRAPFRAME VA (set up by userret on previous return).
# We swap a0 <-> sscratch to get the trapframe pointer.
#
.globl uservec
uservec:
        # a0 <-> sscratch: now a0 = TRAPFRAME, sscratch = user a0
        csrrw a0, sscratch, a0

        # save all user GP registers into the trapframe
        sd ra,  TF_RA(a0)
        sd sp,  TF_SP(a0)
        sd gp,  TF_GP(a0)
        sd tp,  TF_TP(a0)
        sd t0,  TF_T0(a0)
        sd t1,  TF_T1(a0)
        sd t2,  TF_T2(a0)
        sd s0,  TF_S0(a0)
        sd s1,  TF_S1(a0)
        # a0 is in sscratch — saved below
        sd a1,  TF_A1(a0)
        sd a2,  TF_A2(a0)
        sd a3,  TF_A3(a0)
        sd a4,  TF_A4(a0)
        sd a5,  TF_A5(a0)
        sd a6,  TF_A6(a0)
        sd a7,  TF_A7(a0)
        sd s2,  TF_S2(a0)
        sd s3,  TF_S3(a0)
        sd s4,  TF_S4(a0)
        sd s5,  TF_S5(a0)
        sd s6,  TF_S6(a0)
        sd s7,  TF_S7(a0)
        sd s8,  TF_S8(a0)
        sd s9,  TF_S9(a0)
        sd s10, TF_S10(a0)
        sd s11, TF_S11(a0)
        sd t3,  TF_T3(a0)
        sd t4,  TF_T4(a0)
        sd t5,  TF_T5(a0)
        sd t6,  TF_T6(a0)

        # save user a0 (stashed in sscratch) into trapframe
        csrr t0, sscratch
        sd   t0, TF_A0(a0)

        # load kernel state from trapframe
        ld sp, TF_KERNEL_SP(a0)
        ld tp, TF_KERNEL_HARTID(a0)
        ld t0, TF_KERNEL_TRAP(a0)
        ld t1, TF_KERNEL_SATP(a0)

        # switch to kernel page table
        csrw satp, t1
        sfence.vma zero, zero

        # jump to user_trap_handler (high-VA kernel address)
        jr t0

# userret — return from kernel to U-mode
#
# Called from C as:
#   ((void (*)(uint64, uint64))fn)(TRAPFRAME, user_satp);
#
# a0 = TRAPFRAME VA
# a1 = user satp value
#
.globl userret
userret:
        # switch to user page table
        csrw satp, a1
        sfence.vma zero, zero

        # put saved user a0 into sscratch so we can swap it at the end
        ld t0, TF_A0(a0)
        csrw sscratch, t0

        # restore all user GP registers except a0
        ld ra,  TF_RA(a0)
        ld sp,  TF_SP(a0)
        ld gp,  TF_GP(a0)
        ld tp,  TF_TP(a0)
        ld t0,  TF_T0(a0)
        ld t1,  TF_T1(a0)
        ld t2,  TF_T2(a0)
        ld s0,  TF_S0(a0)
        ld s1,  TF_S1(a0)
        # a0 restored via sscratch swap below
        ld a1,  TF_A1(a0)
        ld a2,  TF_A2(a0)
        ld a3,  TF_A3(a0)
        ld a4,  TF_A4(a0)
        ld a5,  TF_A5(a0)
        ld a6,  TF_A6(a0)
        ld a7,  TF_A7(a0)
        ld s2,  TF_S2(a0)
        ld s3,  TF_S3(a0)
        ld s4,  TF_S4(a0)
        ld s5,  TF_S5(a0)
        ld s6,  TF_S6(a0)
        ld s7,  TF_S7(a0)
        ld s8,  TF_S8(a0)
        ld s9,  TF_S9(a0)
        ld s10, TF_S10(a0)
        ld s11, TF_S11(a0)
        ld t3,  TF_T3(a0)
        ld t4,  TF_T4(a0)
        ld t5,  TF_T5(a0)
        ld t6,  TF_T6(a0)

        # restore user a0, put TRAPFRAME into sscratch for next trap
        csrrw a0, sscratch, a0

        # return to user mode at sepc
        sret

.balign 4096
