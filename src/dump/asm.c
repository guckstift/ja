#include <stdlib.h>
#include <string.h>
#include "asm.h"

static uint8_t *text;
static uint64_t text_size;

void *asm_get_text()
{
	return text;
}

uint64_t asm_get_text_size()
{
	return text_size;
}

void asm_start()
{
	text = 0;
	text_size = 0;
}

void *asm_uint8(uint8_t x)
{
	uint64_t s = sizeof(x);
	text = realloc(text, text_size + x);
	uint8_t *p = text + text_size;
	memcpy(p, &x, s);
	text_size += s;
	return p;
}

void *asm_uint32(uint32_t x)
{
	uint64_t s = sizeof(x);
	text = realloc(text, text_size + x);
	uint8_t *p = text + text_size;
	memcpy(p, &x, s);
	text_size += s;
	return p;
}

void asm_syscall()
{
	asm_uint8(0x0f);
	asm_uint8(0x05);
}

void asm_mov_r32_i32(uint8_t r, uint32_t i)
{
	asm_uint8(0xb8 + r);
	asm_uint32(i);
}
