#include <stdlib.h>
#include <stdio.h>
#include "elf.h"

static uint64_t round_up_pot(uint64_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	v++;
	return v;
}

static uint64_t align_up_page(uint64_t v)
{
	return (v + 0xfff) >> 12 << 12;
}

Elf *new_elf()
{
	Elf *elf = malloc(sizeof(Elf));
	elf->hdr.ident.mag[0] = ELFMAG0;
	elf->hdr.ident.mag[1] = ELFMAG1;
	elf->hdr.ident.mag[2] = ELFMAG2;
	elf->hdr.ident.mag[3] = ELFMAG3;
	elf->hdr.ident.class = ELFCLASS64;
	elf->hdr.ident.data = ELFDATA2LSB;
	elf->hdr.ident.version = EV_CURRENT;
	elf->hdr.ident.osabi = ELFOSABI_NONE;
	elf->hdr.ident.abiversion = 0;
	elf->hdr.ident.pad[0] = 0;
	elf->hdr.ident.pad[1] = 0;
	elf->hdr.ident.pad[2] = 0;
	elf->hdr.ident.pad[3] = 0;
	elf->hdr.ident.pad[4] = 0;
	elf->hdr.ident.pad[5] = 0;
	elf->hdr.ident.pad[6] = 0;
	elf->hdr.type = ET_EXEC;
	elf->hdr.machine = EM_X86_64;
	elf->hdr.version = EV_CURRENT;
	elf->hdr.entry = BASE_ADDR + PAGE_SIZE;
	elf->hdr.phoff = sizeof(ElfHeader);
	elf->hdr.shoff = 0;
	elf->hdr.flags = 0;
	elf->hdr.ehsize = sizeof(ElfHeader);
	elf->hdr.phentsize = sizeof(ElfProgramHeader);
	elf->hdr.phnum = 3;
	elf->hdr.shentsize = sizeof(ElfSectionHeader);
	elf->hdr.shnum = 0;
	elf->hdr.shstrndx = 0;
	elf->phdr_text.type = PT_LOAD;
	elf->phdr_text.flags = PF_X | PF_R;
	elf->phdr_text.offset = PAGE_SIZE;
	elf->phdr_text.vaddr = BASE_ADDR + elf->phdr_text.offset;
	elf->phdr_text.paddr = elf->phdr_text.vaddr;
	elf->phdr_text.filesz = 0;
	elf->phdr_text.memsz = 0;
	elf->phdr_text.align = PAGE_SIZE;
	elf->phdr_rodata.type = PT_LOAD;
	elf->phdr_rodata.flags = PF_R;
	elf->phdr_rodata.offset = 0;
	elf->phdr_rodata.vaddr = 0;
	elf->phdr_rodata.paddr = 0;
	elf->phdr_rodata.filesz = 0;
	elf->phdr_rodata.memsz = 0;
	elf->phdr_rodata.align = PAGE_SIZE;
	elf->phdr_data.type = PT_LOAD;
	elf->phdr_data.flags = PF_W | PF_R;
	elf->phdr_data.offset = 0;
	elf->phdr_data.vaddr = 0;
	elf->phdr_data.paddr = 0;
	elf->phdr_data.filesz = 0;
	elf->phdr_data.memsz = 0;
	elf->phdr_data.align = PAGE_SIZE;
}

void *elf_set_text(Elf *elf, void *text, uint64_t size)
{
	elf->text = text;
	elf->phdr_text.filesz = size;
	elf->phdr_text.memsz = size;
}

void *elf_set_rodata(Elf *elf, void *rodata, uint64_t size)
{
	elf->rodata = rodata;
	elf->phdr_rodata.filesz = size;
	elf->phdr_rodata.memsz = size;
}

void *elf_set_data(Elf *elf, void *data, uint64_t size)
{
	elf->data = data;
	elf->phdr_data.filesz = size;
	elf->phdr_data.memsz = size;
}

void *elf_save(Elf *elf, char *filename)
{
	elf->phdr_rodata.offset = elf->phdr_text.offset + elf->phdr_text.filesz;
	elf->phdr_rodata.offset = align_up_page(elf->phdr_rodata.offset);
	elf->phdr_rodata.vaddr = BASE_ADDR + elf->phdr_rodata.offset;
	elf->phdr_rodata.paddr = elf->phdr_rodata.vaddr;
	
	elf->phdr_data.offset = elf->phdr_rodata.offset + elf->phdr_rodata.filesz;
	elf->phdr_data.offset = align_up_page(elf->phdr_data.offset);
	elf->phdr_data.vaddr = BASE_ADDR + elf->phdr_data.offset;
	elf->phdr_data.paddr = elf->phdr_data.vaddr;
	
	FILE *fs = fopen(filename, "wb");
	fwrite(&elf->hdr, 1, sizeof(elf->hdr), fs);
	fwrite(&elf->phdr_text, 1, sizeof(elf->phdr_text), fs);
	fwrite(&elf->phdr_rodata, 1, sizeof(elf->phdr_rodata), fs);
	fwrite(&elf->phdr_data, 1, sizeof(elf->phdr_data), fs);
	for(uint64_t i = ftell(fs); i < elf->phdr_text.offset; i++) fputc(0, fs);
	fwrite(elf->text, 1, elf->phdr_text.filesz, fs);
	for(uint64_t i = ftell(fs); i < elf->phdr_rodata.offset; i++) fputc(0, fs);
	fwrite(elf->rodata, 1, elf->phdr_rodata.filesz, fs);
	for(uint64_t i = ftell(fs); i < elf->phdr_data.offset; i++) fputc(0, fs);
	fwrite(elf->data, 1, elf->phdr_data.filesz, fs);
	fclose(fs);
}
