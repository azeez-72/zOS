	# right now, we just handle interrupts while in s-mode, 
	# we have to wait until we have virtual memory to start doing u-mode

.section .text.init
.global trap_entry
.align 4		# st vec requires 4-byte alignment

trap_entry:
	# allocate space to save 32 registers
	addi sp, sp, -256

	# save caller-saved registers by their ABI mapping
	 sd ra, 0(sp)
        # sd sp, 8(sp) we don't save sp, but to keep consistent with mapping, we keep this gap
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)
	# trap handler in trap.c
	call kernel_trap_handler

	# restore registers
	ld ra, 0(sp)
        # ld sp, 8(sp)
        ld gp, 16(sp)
        ld tp, 24(sp) 
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

	addi sp, sp, 256

	sret
