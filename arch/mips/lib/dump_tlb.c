/*
 * Dump R4x00 TLB for debugging purposes.
 *
 * Copyright (C) 1994, 1995 by Waldorf Electronics, written by Ralf Baechle.
 * Copyright (C) 1999 by Silicon Graphics, Inc.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/string.h>

#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/cachectl.h>
#include <asm/mipsregs.h>
#include <asm/page.h>
#include <asm/pgtable.h>

static inline const char *msg2str(unsigned int mask)
{
	switch (mask) {
	case PM_4K:	return "4kb";
	case PM_16K:	return "16kb";
	case PM_64K:	return "64kb";
	case PM_256K:	return "256kb";
#ifndef CONFIG_CPU_VR41XX
	case PM_1M:	return "1Mb";
	case PM_4M:	return "4Mb";
	case PM_16M:	return "16Mb";
	case PM_64M:	return "64Mb";
	case PM_256M:	return "256Mb";
#endif
	}
}

void dump_tlb(int first, int last)
{
	int	i;
	unsigned int pagemask, c0, c1, asid;
	unsigned long long entrylo0, entrylo1;
	unsigned long entryhi;

	asid = get_entryhi() & 0xff;

	printk("\n");
	for(i=first;i<=last;i++) {
		write_32bit_cp0_register(CP0_INDEX, i);
		__asm__ __volatile__(
			".set\tmips3\n\t"
			".set\tnoreorder\n\t"
			"nop;nop;nop;nop\n\t"
			"tlbr\n\t"
			"nop;nop;nop;nop\n\t"
			".set\treorder\n\t"
			".set\tmips0\n\t");
		pagemask = read_32bit_cp0_register(CP0_PAGEMASK);
		entryhi  = read_32bit_cp0_register(CP0_ENTRYHI);
		entrylo0 = get_entrylo0();
		entrylo1 = get_entrylo1();

		/* Unused entries have a virtual address in KSEG0.  */
		if ((entryhi & 0xf0000000) != 0x80000000
		    && (entryhi & 0xff) == asid) {
			/*
			 * Only print entries in use
			 */
			printk("Index: %2d pgmask=%s ", i, msg2str(pagemask));

			c0 = (entrylo0 >> 3) & 7;
			c1 = (entrylo1 >> 3) & 7;

			printk("va=%08lx asid=%02lx\n",
			       (entryhi & 0xffffe000), (entryhi & 0xff));
			printk("\t\t\t[pa=%08Lx c=%d d=%d v=%d g=%Ld]\n",
			       (entrylo0 << 6) & PAGE_MASK, c0,
			       (entrylo0 & 4) ? 1 : 0,
			       (entrylo0 & 2) ? 1 : 0,
			       (entrylo0 & 1));
			printk("\t\t\t[pa=%08Lx c=%d d=%d v=%d g=%Ld]\n",
			       (entrylo1 << 6) & PAGE_MASK, c1,
			       (entrylo1 & 4) ? 1 : 0,
			       (entrylo1 & 2) ? 1 : 0,
			       (entrylo1 & 1));
			printk("\n");
		}
	}

	set_entryhi(asid);
}

void dump_tlb_all(void)
{
	dump_tlb(0, mips_cpu.tlbsize - 1);
}

void dump_tlb_wired(void)
{
	int	wired;

	wired = read_32bit_cp0_register(CP0_WIRED);
	printk("Wired: %d", wired);
	dump_tlb(0, read_32bit_cp0_register(CP0_WIRED));
}

#define BARRIER						\
	__asm__ __volatile__(				\
		".set\tnoreorder\n\t"			\
		"nop;nop;nop;nop;nop;nop;nop\n\t"	\
		".set\treorder");

void
dump_tlb_addr(unsigned long addr)
{
	unsigned int flags, oldpid;
	int index;

	__save_and_cli(flags);
	oldpid = get_entryhi() & 0xff;
	BARRIER;
	set_entryhi((addr & PAGE_MASK) | oldpid);
	BARRIER;
	tlb_probe();
	BARRIER;
	index = get_index();
	set_entryhi(oldpid);
	__restore_flags(flags);

	if (index < 0) {
		printk("No entry for address 0x%08lx in TLB\n", addr);
		return;
	}

	printk("Entry %d maps address 0x%08lx\n", index, addr);
	dump_tlb(index, index);
}

void
dump_tlb_nonwired(void)
{
	dump_tlb(read_32bit_cp0_register(CP0_WIRED), mips_cpu.tlbsize - 1);
}

void
dump_list_process(struct task_struct *t, void *address)
{
	pgd_t	*page_dir, *pgd;
	pmd_t	*pmd;
	pte_t	*pte, page;
	unsigned int addr;
	unsigned long val;

	addr = (unsigned int) address;

	printk("Addr                 == %08x\n", addr);
	printk("task                 == %08p\n", t);
	printk("task->mm             == %08p\n", t->mm);
	//printk("tasks->mm.pgd        == %08x\n", (unsigned int) t->mm->pgd);

	if (addr > KSEG0)
		page_dir = pgd_offset_k(0);
	else
		page_dir = pgd_offset(t->mm, 0);
	printk("page_dir == %08x\n", (unsigned int) page_dir);

	if (addr > KSEG0)
		pgd = pgd_offset_k(addr);
	else
		pgd = pgd_offset(t->mm, addr);
	printk("pgd == %08x, ", (unsigned int) pgd);

	pmd = pmd_offset(pgd, addr);
	printk("pmd == %08x, ", (unsigned int) pmd);

	pte = pte_offset(pmd, addr);
	printk("pte == %08x, ", (unsigned int) pte);

	page = *pte;
#ifdef CONFIG_64BIT_PHYS_ADDR
	printk("page == %08Lx\n", (unsigned long long) pte_val(page));
#else
	printk("page == %08lx\n", (unsigned int) pte_val(page));
#endif

	val = pte_val(page);
	if (val & _PAGE_PRESENT) printk("present ");
	if (val & _PAGE_READ) printk("read ");
	if (val & _PAGE_WRITE) printk("write ");
	if (val & _PAGE_ACCESSED) printk("accessed ");
	if (val & _PAGE_MODIFIED) printk("modified ");
	if (val & _PAGE_R4KBUG) printk("r4kbug ");
	if (val & _PAGE_GLOBAL) printk("global ");
	if (val & _PAGE_VALID) printk("valid ");
	printk("\n");
}

void
dump_list_current(void *address)
{
	dump_list_process(current, address);
}

unsigned int
vtop(void *address)
{
	pgd_t	*pgd;
	pmd_t	*pmd;
	pte_t	*pte;
	unsigned int addr, paddr;

	addr = (unsigned long) address;
	pgd = pgd_offset(current->mm, addr);
	pmd = pmd_offset(pgd, addr);
	pte = pte_offset(pmd, addr);
	paddr = (KSEG1 | (unsigned int) pte_val(*pte)) & PAGE_MASK;
	paddr |= (addr & ~PAGE_MASK);

	return paddr;
}

void
dump16(unsigned long *p)
{
	int i;

	for(i=0;i<8;i++)
	{
		printk("*%08lx == %08lx, ",
		       (unsigned long)p, (unsigned long)*p++);
		printk("*%08lx == %08lx\n",
		       (unsigned long)p, (unsigned long)*p++);
	}
}
