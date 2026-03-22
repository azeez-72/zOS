#pragma once

#include <stdint.h>

struct trapframe;

void trapinit(void);
void kernel_trap_handler(void *frame);
void user_trap_handler(void);
void usertrapret(struct trapframe *tf, uint64_t user_satp);
