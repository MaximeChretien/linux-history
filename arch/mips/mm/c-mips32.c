/*
 * Kevin D. Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * This program is free software; you can distribute it and/or modify it
 * under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * MIPS32 CPU variant specific MMU/Cache routines.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/bcache.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/mmu_context.h>

/* CP0 hazard avoidance. */
#define BARRIER __asm__ __volatile__(".set noreorder\n\t" \
				     "nop; nop; nop; nop; nop; nop;\n\t" \
				     ".set reorder\n\t")

/* Primary cache parameters. */
int icache_size, dcache_size; 			/* Size in bytes */
int ic_lsize, dc_lsize;				/* LineSize in bytes */

/* Secondary cache (if present) parameters. */
unsigned int scache_size, sc_lsize;		/* Again, in bytes */

#include <asm/cacheops.h>
#include <asm/mips32_cache.h>

#undef DEBUG_CACHE

/*
 * Dummy cache handling routines for machines without boardcaches
 */
static void no_sc_noop(void) {}

static struct bcache_ops no_sc_ops = {
	(void *)no_sc_noop, (void *)no_sc_noop,
	(void *)no_sc_noop, (void *)no_sc_noop
};

struct bcache_ops *bcops = &no_sc_ops;

static inline void mips32_flush_cache_all_sc(void)
{
	unsigned long flags;

	__save_and_cli(flags);
	blast_dcache(); blast_icache(); blast_scache();
	__restore_flags(flags);
}

static inline void mips32_flush_cache_all_pc(void)
{
	unsigned long flags;

	__save_and_cli(flags);
	blast_dcache(); blast_icache();
	__restore_flags(flags);
}

static void
mips32_flush_cache_range_sc(struct mm_struct *mm,
			 unsigned long start,
			 unsigned long end)
{
	struct vm_area_struct *vma;
	unsigned long flags;

	if(mm->context == 0)
		return;

	start &= PAGE_MASK;
#ifdef DEBUG_CACHE
	printk("crange[%d,%08lx,%08lx]", (int)mm->context, start, end);
#endif
	vma = find_vma(mm, start);
	if(vma) {
		if(mm->context != current->mm->context) {
			mips32_flush_cache_all_sc();
		} else {
			pgd_t *pgd;
			pmd_t *pmd;
			pte_t *pte;

			__save_and_cli(flags);
			while(start < end) {
				pgd = pgd_offset(mm, start);
				pmd = pmd_offset(pgd, start);
				pte = pte_offset(pmd, start);

				if(pte_val(*pte) & _PAGE_VALID)
					blast_scache_page(start);
				start += PAGE_SIZE;
			}
			__restore_flags(flags);
		}
	}
}

static void mips32_flush_cache_range_pc(struct mm_struct *mm,
				     unsigned long start,
				     unsigned long end)
{
	if(mm->context != 0) {
		unsigned long flags;

#ifdef DEBUG_CACHE
		printk("crange[%d,%08lx,%08lx]", (int)mm->context, start, end);
#endif
		__save_and_cli(flags);
		blast_dcache(); blast_icache();
		__restore_flags(flags);
	}
}

/*
 * On architectures like the Sparc, we could get rid of lines in
 * the cache created only by a certain context, but on the MIPS
 * (and actually certain Sparc's) we cannot.
 */
static void mips32_flush_cache_mm_sc(struct mm_struct *mm)
{
	if(mm->context != 0) {
#ifdef DEBUG_CACHE
		printk("cmm[%d]", (int)mm->context);
#endif
		mips32_flush_cache_all_sc();
	}
}

static void mips32_flush_cache_mm_pc(struct mm_struct *mm)
{
	if(mm->context != 0) {
#ifdef DEBUG_CACHE
		printk("cmm[%d]", (int)mm->context);
#endif
		mips32_flush_cache_all_pc();
	}
}

static void mips32_flush_cache_page_sc(struct vm_area_struct *vma,
				    unsigned long page)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long flags;
	pgd_t *pgdp;
	pmd_t *pmdp;
	pte_t *ptep;

	/*
	 * If ownes no valid ASID yet, cannot possibly have gotten
	 * this page into the cache.
	 */
	if (mm->context == 0)
		return;

#ifdef DEBUG_CACHE
	printk("cpage[%d,%08lx]", (int)mm->context, page);
#endif
	__save_and_cli(flags);
	page &= PAGE_MASK;
	pgdp = pgd_offset(mm, page);
	pmdp = pmd_offset(pgdp, page);
	ptep = pte_offset(pmdp, page);

	/*
	 * If the page isn't marked valid, the page cannot possibly be
	 * in the cache.
	 */
	if (!(pte_val(*ptep) & _PAGE_VALID))
		goto out;

	/*
	 * Doing flushes for another ASID than the current one is
	 * too difficult since R4k caches do a TLB translation
	 * for every cache flush operation.  So we do indexed flushes
	 * in that case, which doesn't overly flush the cache too much.
	 */
	if (mm->context != current->active_mm->context) {
		/*
		 * Do indexed flush, too much work to get the (possible)
		 * tlb refills to work correctly.
		 */
		page = (KSEG0 + (page & (scache_size - 1)));
		blast_dcache_page_indexed(page);
		blast_scache_page_indexed(page);
	} else
		blast_scache_page(page);
out:
	__restore_flags(flags);
}

static void mips32_flush_cache_page_pc(struct vm_area_struct *vma,
				    unsigned long page)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long flags;
	pgd_t *pgdp;
	pmd_t *pmdp;
	pte_t *ptep;

	/*
	 * If ownes no valid ASID yet, cannot possibly have gotten
	 * this page into the cache.
	 */
	if (mm->context == 0)
		return;

#ifdef DEBUG_CACHE
	printk("cpage[%d,%08lx]", (int)mm->context, page);
#endif
	__save_and_cli(flags);
	page &= PAGE_MASK;
	pgdp = pgd_offset(mm, page);
	pmdp = pmd_offset(pgdp, page);
	ptep = pte_offset(pmdp, page);

	/*
	 * If the page isn't marked valid, the page cannot possibly be
	 * in the cache.
	 */
	if (!(pte_val(*ptep) & _PAGE_VALID))
		goto out;

	/*
	 * Doing flushes for another ASID than the current one is
	 * too difficult since Mips32 caches do a TLB translation
	 * for every cache flush operation.  So we do indexed flushes
	 * in that case, which doesn't overly flush the cache too much.
	 */
	if (mm == current->active_mm) {
		blast_dcache_page(page);
	} else {
		/* Do indexed flush, too much work to get the (possible)
		 * tlb refills to work correctly.
		 */
		page = (KSEG0 + (page & (dcache_size - 1)));
		blast_dcache_page_indexed(page);
	}
out:
	__restore_flags(flags);
}

/* If the addresses passed to these routines are valid, they are
 * either:
 *
 * 1) In KSEG0, so we can do a direct flush of the page.
 * 2) In KSEG2, and since every process can translate those
 *    addresses all the time in kernel mode we can do a direct
 *    flush.
 * 3) In KSEG1, no flush necessary.
 */
static void mips32_flush_page_to_ram_sc(struct page *page)
{
	blast_scache_page((unsigned long)page_address(page));
}

static void mips32_flush_page_to_ram_pc(struct page *page)
{
	blast_dcache_page((unsigned long)page_address(page));
}

static void
mips32_flush_icache_page_s(struct vm_area_struct *vma, struct page *page)
{
	/*
	 * We did an scache flush therefore PI is already clean.
	 */
}

static void
mips32_flush_icache_range(unsigned long start, unsigned long end)
{
	flush_cache_all();
}

static void
mips32_flush_icache_page(struct vm_area_struct *vma, struct page *page)
{
	int address;

	if (!(vma->vm_flags & VM_EXEC))
		return;

	address = KSEG0 + ((unsigned long)page_address(page) & PAGE_MASK & (dcache_size - 1));
	blast_icache_page_indexed(address);
}

/*
 * Writeback and invalidate the primary cache dcache before DMA.
 */
static void
mips32_dma_cache_wback_inv_pc(unsigned long addr, unsigned long size)
{
	unsigned long end, a;
	unsigned int flags;

	if (size >= dcache_size) {
		flush_cache_all();
	} else {
	        __save_and_cli(flags);
		a = addr & ~(dc_lsize - 1);
		end = (addr + size) & ~(dc_lsize - 1);
		while (1) {
			flush_dcache_line(a); /* Hit_Writeback_Inv_D */
			if (a == end) break;
			a += dc_lsize;
		}
		__restore_flags(flags);
	}
	bc_wback_inv(addr, size);
}

static void
mips32_dma_cache_wback_inv_sc(unsigned long addr, unsigned long size)
{
	unsigned long end, a;

	if (size >= scache_size) {
		flush_cache_all();
		return;
	}

	a = addr & ~(sc_lsize - 1);
	end = (addr + size) & ~(sc_lsize - 1);
	while (1) {
		flush_scache_line(a);	/* Hit_Writeback_Inv_SD */
		if (a == end) break;
		a += sc_lsize;
	}
}

static void
mips32_dma_cache_inv_pc(unsigned long addr, unsigned long size)
{
	unsigned long end, a;
	unsigned int flags;

	if (size >= dcache_size) {
		flush_cache_all();
	} else {
	        __save_and_cli(flags);
		a = addr & ~(dc_lsize - 1);
		end = (addr + size) & ~(dc_lsize - 1);
		while (1) {
			flush_dcache_line(a); /* Hit_Writeback_Inv_D */
			if (a == end) break;
			a += dc_lsize;
		}
		__restore_flags(flags);
	}

	bc_inv(addr, size);
}

static void
mips32_dma_cache_inv_sc(unsigned long addr, unsigned long size)
{
	unsigned long end, a;

	if (size >= scache_size) {
		flush_cache_all();
		return;
	}

	a = addr & ~(sc_lsize - 1);
	end = (addr + size) & ~(sc_lsize - 1);
	while (1) {
		flush_scache_line(a); /* Hit_Writeback_Inv_SD */
		if (a == end) break;
		a += sc_lsize;
	}
}

static void
mips32_dma_cache_wback(unsigned long addr, unsigned long size)
{
	panic("mips32_dma_cache called - should not happen.");
}

/*
 * While we're protected against bad userland addresses we don't care
 * very much about what happens in that case.  Usually a segmentation
 * fault will dump the process later on anyway ...
 */
static void mips32_flush_cache_sigtramp(unsigned long addr)
{
	protected_writeback_dcache_line(addr & ~(dc_lsize - 1));
	protected_flush_icache_line(addr & ~(ic_lsize - 1));
}

static void mips32_flush_icache_all(void)
{
	if (mips_cpu.cputype == CPU_20KC) {
		blast_icache();
	}
}

/* Detect and size the various caches. */
static void __init probe_icache(unsigned long config)
{
        unsigned long config1;
	unsigned int lsize;

        if (!(config & (1 << 31))) {
	        /* 
		 * Not a MIPS32 complainant CPU. 
		 * Config 1 register not supported, we assume R4k style.
		 */
	        icache_size = 1 << (12 + ((config >> 9) & 7));
		ic_lsize = 16 << ((config >> 5) & 1);
		mips_cpu.icache.linesz = ic_lsize;
		
		/* 
		 * We cannot infer associativity - assume direct map
		 * unless probe template indicates otherwise
		 */
		if(!mips_cpu.icache.ways) mips_cpu.icache.ways = 1;
		mips_cpu.icache.sets = 
			(icache_size / ic_lsize) / mips_cpu.icache.ways;
	} else {
	       config1 = read_mips32_cp0_config1(); 

	       if ((lsize = ((config1 >> 19) & 7)))
		       mips_cpu.icache.linesz = 2 << lsize;
	       else 
		       mips_cpu.icache.linesz = lsize;
	       mips_cpu.icache.sets = 64 << ((config1 >> 22) & 7);
	       mips_cpu.icache.ways = 1 + ((config1 >> 16) & 7);

	       ic_lsize = mips_cpu.icache.linesz;
	       icache_size = mips_cpu.icache.sets * mips_cpu.icache.ways * 
		             ic_lsize;
	}
	printk("Primary instruction cache %dkb, linesize %d bytes (%d ways)\n",
	       icache_size >> 10, ic_lsize, mips_cpu.icache.ways);
}

static void __init probe_dcache(unsigned long config)
{
        unsigned long config1;
	unsigned int lsize;

        if (!(config & (1 << 31))) {
	        /* 
		 * Not a MIPS32 complainant CPU. 
		 * Config 1 register not supported, we assume R4k style.
		 */  
		dcache_size = 1 << (12 + ((config >> 6) & 7));
		dc_lsize = 16 << ((config >> 4) & 1);
		mips_cpu.dcache.linesz = dc_lsize;
		/* 
		 * We cannot infer associativity - assume direct map
		 * unless probe template indicates otherwise
		 */
		if(!mips_cpu.dcache.ways) mips_cpu.dcache.ways = 1;
		mips_cpu.dcache.sets = 
			(dcache_size / dc_lsize) / mips_cpu.dcache.ways;
	} else {
	        config1 = read_mips32_cp0_config1();

		if ((lsize = ((config1 >> 10) & 7)))
		        mips_cpu.dcache.linesz = 2 << lsize;
		else 
		        mips_cpu.dcache.linesz= lsize;
		mips_cpu.dcache.sets = 64 << ((config1 >> 13) & 7);
		mips_cpu.dcache.ways = 1 + ((config1 >> 7) & 7);

		dc_lsize = mips_cpu.dcache.linesz;
		dcache_size = 
			mips_cpu.dcache.sets * mips_cpu.dcache.ways
			* dc_lsize;
	}
	printk("Primary data cache %dkb, linesize %d bytes (%d ways)\n",
	       dcache_size >> 10, dc_lsize, mips_cpu.dcache.ways);
}


/* If you even _breathe_ on this function, look at the gcc output
 * and make sure it does not pop things on and off the stack for
 * the cache sizing loop that executes in KSEG1 space or else
 * you will crash and burn badly.  You have been warned.
 */
static int __init probe_scache(unsigned long config)
{
	extern unsigned long stext;
	unsigned long flags, addr, begin, end, pow2;
	int tmp;

	if (mips_cpu.scache.flags == MIPS_CACHE_NOT_PRESENT)
		return 0;

	tmp = ((config >> 17) & 1);
	if(tmp)
		return 0;
	tmp = ((config >> 22) & 3);
	switch(tmp) {
	case 0:
		sc_lsize = 16;
		break;
	case 1:
		sc_lsize = 32;
		break;
	case 2:
		sc_lsize = 64;
		break;
	case 3:
		sc_lsize = 128;
		break;
	}

	begin = (unsigned long) &stext;
	begin &= ~((4 * 1024 * 1024) - 1);
	end = begin + (4 * 1024 * 1024);

	/* This is such a bitch, you'd think they would make it
	 * easy to do this.  Away you daemons of stupidity!
	 */
	__save_and_cli(flags);

	/* Fill each size-multiple cache line with a valid tag. */
	pow2 = (64 * 1024);
	for(addr = begin; addr < end; addr = (begin + pow2)) {
		unsigned long *p = (unsigned long *) addr;
		__asm__ __volatile__("nop" : : "r" (*p)); /* whee... */
		pow2 <<= 1;
	}

	/* Load first line with zero (therefore invalid) tag. */
	set_taglo(0);
	set_taghi(0);
	__asm__ __volatile__("nop; nop; nop; nop;"); /* avoid the hazard */
	__asm__ __volatile__("\n\t.set noreorder\n\t"
			     ".set mips3\n\t"
			     "cache 8, (%0)\n\t"
			     ".set mips0\n\t"
			     ".set reorder\n\t" : : "r" (begin));
	__asm__ __volatile__("\n\t.set noreorder\n\t"
			     ".set mips3\n\t"
			     "cache 9, (%0)\n\t"
			     ".set mips0\n\t"
			     ".set reorder\n\t" : : "r" (begin));
	__asm__ __volatile__("\n\t.set noreorder\n\t"
			     ".set mips3\n\t"
			     "cache 11, (%0)\n\t"
			     ".set mips0\n\t"
			     ".set reorder\n\t" : : "r" (begin));

	/* Now search for the wrap around point. */
	pow2 = (128 * 1024);
	tmp = 0;
	for(addr = (begin + (128 * 1024)); addr < (end); addr = (begin + pow2)) {
		__asm__ __volatile__("\n\t.set noreorder\n\t"
				     ".set mips3\n\t"
				     "cache 7, (%0)\n\t"
				     ".set mips0\n\t"
				     ".set reorder\n\t" : : "r" (addr));
		__asm__ __volatile__("nop; nop; nop; nop;"); /* hazard... */
		if(!get_taglo())
			break;
		pow2 <<= 1;
	}
	__restore_flags(flags);
	addr -= begin;
	printk("Secondary cache sized at %dK linesize %d bytes.\n",
	       (int) (addr >> 10), sc_lsize);
	scache_size = addr;
	return 1;
}

static void __init setup_noscache_funcs(void)
{
	_clear_page = (void *)mips32_clear_page_dc;
	_copy_page = (void *)mips32_copy_page_dc;
	_flush_cache_all = mips32_flush_cache_all_pc;
	___flush_cache_all = mips32_flush_cache_all_pc;
	_flush_cache_mm = mips32_flush_cache_mm_pc;
	_flush_cache_range = mips32_flush_cache_range_pc;
	_flush_cache_page = mips32_flush_cache_page_pc;
	_flush_page_to_ram = mips32_flush_page_to_ram_pc;

	_flush_icache_page = mips32_flush_icache_page;

	_dma_cache_wback_inv = mips32_dma_cache_wback_inv_pc;
	_dma_cache_wback = mips32_dma_cache_wback;
	_dma_cache_inv = mips32_dma_cache_inv_pc;
}

static void __init setup_scache_funcs(void)
{
        _flush_cache_all = mips32_flush_cache_all_sc;
        ___flush_cache_all = mips32_flush_cache_all_sc;
	_flush_cache_mm = mips32_flush_cache_mm_sc;
	_flush_cache_range = mips32_flush_cache_range_sc;
	_flush_cache_page = mips32_flush_cache_page_sc;
	_flush_page_to_ram = mips32_flush_page_to_ram_sc;
	_clear_page = (void *)mips32_clear_page_sc;
	_copy_page = (void *)mips32_copy_page_sc;

	_flush_icache_page = mips32_flush_icache_page_s;

	_dma_cache_wback_inv = mips32_dma_cache_wback_inv_sc;
	_dma_cache_wback = mips32_dma_cache_wback;
	_dma_cache_inv = mips32_dma_cache_inv_sc;
}

typedef int (*probe_func_t)(unsigned long);

static inline void __init setup_scache(unsigned int config)
{
	probe_func_t probe_scache_kseg1;
	int sc_present = 0;

	/* Maybe the cpu knows about a l2 cache? */
	probe_scache_kseg1 = (probe_func_t) (KSEG1ADDR(&probe_scache));
	sc_present = probe_scache_kseg1(config);

	if (sc_present) {
	  	mips_cpu.scache.linesz = sc_lsize;
		/* 
		 * We cannot infer associativity - assume direct map
		 * unless probe template indicates otherwise
		 */
		if(!mips_cpu.scache.ways) mips_cpu.scache.ways = 1;
		mips_cpu.scache.sets = 
		  (scache_size / sc_lsize) / mips_cpu.scache.ways;

		setup_scache_funcs();
		return;
	}

	setup_noscache_funcs();
}

void __init ld_mmu_mips32(void)
{
	unsigned long config = read_32bit_cp0_register(CP0_CONFIG);

#ifdef CONFIG_MIPS_UNCACHED
	change_cp0_config(CONF_CM_CMASK, CONF_CM_UNCACHED);
#else
	change_cp0_config(CONF_CM_CMASK, CONF_CM_CACHABLE_NONCOHERENT);
#endif

	probe_icache(config);
	probe_dcache(config);
	setup_scache(config);

	_flush_cache_sigtramp = mips32_flush_cache_sigtramp;
	_flush_icache_range = mips32_flush_icache_range;	/* Ouch */
	_flush_icache_all = mips32_flush_icache_all;

	__flush_cache_all();
}
