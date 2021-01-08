#include <x86.h>
#include <elf.h>

#define PHY_ELF_ADDR	0x10000
#define SECT_SIZE	512
#define ELF_HDR		((struct Elf*) PHY_ELF_ADDR)

void read_sect(void*, uint32_t);
void read_seg(uintptr_t, uint32_t, uint32_t);


void boot_main(void)
{
	struct ProgHeader *ph, *eph;
	void (*entry)(void);

	// read 4k from 1st sect of disk to phy
	read_seg((uintptr_t) ELF_HDR, SECT_SIZE * 8, 0);

	if (ELF_HDR->e_magic != ELF_MAGIC) {
		goto bad_elf;
	}

	ph = (struct ProgHeader*)((void*)ELF_HDR +  ELF_HDR->e_phoff);
	eph = ph + ELF_HDR->e_phnum;
	for (; ph < eph; ph++) {
		read_seg(ph->p_pa, ph->p_memsz, ph->p_offset);
	}
	
	entry = (void(*)(void))(ELF_HDR->e_entry & 0xffffff);
	entry();
bad_elf:
	// 这两条指令有什么用？
	outw(0x8a00, 0x8a00);
	outw(0x8a00, 0x8e00);
}

void read_seg(uintptr_t pa, uint32_t count, uint32_t offset)
{
	uintptr_t end_pa;

	end_pa = pa + count;

	// 物理地址按512字节对齐
	pa &= ~(SECT_SIZE - 1);
	// offset 转换为磁盘扇区索引，内核的索引从磁盘的1号扇区开始
	offset = (offset / SECT_SIZE) + 1;

	while (pa < end_pa) {
		read_sect((void*)pa, offset);
		pa += SECT_SIZE;
		offset++;
	}
}

void wait_disk(void)
{
	while ((inb(0x1f7) & 0xc0) != 0x40);
}

void read_sect(void *dst, uint32_t offset)
{
	wait_disk();
		
	outb(0x1f2, 1);	// count = 1
	outb(0x1f3, offset);
	outb(0x1f4, offset >> 8);
	outb(0x1f5, offset >> 16);
	outb(0x1f6, (offset >> 24) | 0xe0);	
	outb(0x1f7, 0x20);	// cmd 0x20 -> read sectors

	wait_disk();

	insl(0x1f0, dst, SECT_SIZE / 4);
}
