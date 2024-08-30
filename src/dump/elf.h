#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELFMAG0        0x7f
#define ELFMAG1        'E'
#define ELFMAG2        'L'
#define ELFMAG3        'F'
#define ELFCLASS64     2
#define ELFDATA2LSB    1
#define EV_CURRENT     1
#define ELFOSABI_NONE  0
#define EM_X86_64      62

#define ET_EXEC  2
#define ET_DYN   3

#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3

#define PF_X  1
#define PF_W  2
#define PF_R  4

#define BASE_ADDR  0x400000
#define PAGE_SIZE  0x1000

typedef struct {
	struct {
		uint8_t mag[4];
		uint8_t class;
		uint8_t data;
		uint8_t version;
		uint8_t osabi;
		uint8_t abiversion;
		uint8_t pad[7];
	} ident;
	
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint64_t entry;
	uint64_t phoff;
	uint64_t shoff;
	uint32_t flags;
	uint16_t ehsize;
	uint16_t phentsize;
	uint16_t phnum;
	uint16_t shentsize;
	uint16_t shnum;
	uint16_t shstrndx;
} ElfHeader;

typedef struct {
	uint32_t type;
	uint32_t flags;
	uint64_t offset;
	uint64_t vaddr;
	uint64_t paddr;
	uint64_t filesz;
	uint64_t memsz;
	uint64_t align;
} ElfProgramHeader;

typedef struct {
	uint32_t name;
	uint32_t type;
	uint64_t flags;
	uint64_t addr;
	uint64_t offset;
	uint64_t size;
	uint32_t link;
	uint32_t info;
	uint64_t addralign;
	uint64_t entsize;
} ElfSectionHeader;

typedef struct {
	int64_t tag;
	union {
		uint64_t val;
		uint64_t ptr;
	} un;
} ElfDynamic;

typedef struct {
	ElfHeader hdr;
	ElfProgramHeader phdr_text;
	ElfProgramHeader phdr_rodata;
	ElfProgramHeader phdr_data;
	void *text;
	void *rodata;
	void *data;
} Elf;

Elf *new_elf();
void *elf_set_text(Elf *elf, void *text, uint64_t size);
void *elf_set_rodata(Elf *elf, void *rodata, uint64_t size);
void *elf_set_data(Elf *elf, void *data, uint64_t size);
void *elf_save(Elf *elf, char *filename);

#endif
