#ifndef ASM_H
#define ASM_H

#include <stdint.h>

typedef enum {
	EAX = 0, EBX = 3, ECX = 1, EDX = 2,
	ESI = 6, EDI = 7, EBP = 5, ESP = 4,
} Register;

void *asm_get_text();
uint64_t asm_get_text_size();
void asm_start();
void *asm_uint8(uint8_t x);
void *asm_uint32(uint32_t x);
void asm_syscall();
void asm_mov_r32_i32(uint8_t r, uint32_t i);

#endif
