/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997, 1998, 1999 Ralf Baechle (ralf@gnu.org)
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) 2000 Kanoj Sarcar (kanoj@sgi.com)
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/r10kcache.h>
#include <asm/system.h>
#include <asm/mmu_context.h>

static int scache_lsz64;

/*
 * This version has been tuned on an Origin.  For other machines the arguments
 * of the pref instructin may have to be tuned differently.
 */
static void andes_clear_page(void * page)
{
	__asm__ __volatile__(
		".set\tnoreorder\n\t"
		".set\tnoat\n\t"
		"daddiu\t$1,%0,%2\n"
		"1:\tpref 7,512(%0)\n\t"
		"sd\t$0,(%0)\n\t"
		"sd\t$0,8(%0)\n\t"
		"sd\t$0,16(%0)\n\t"
		"sd\t$0,24(%0)\n\t"
		"daddiu\t%0,64\n\t"
		"sd\t$0,-32(%0)\n\t"
		"sd\t$0,-24(%0)\n\t"
		"sd\t$0,-16(%0)\n\t"
		"bne\t$1,%0,1b\n\t"
		"sd\t$0,-8(%0)\n\t"
		".set\tat\n\t"
		".set\treorder"
		: "=r" (page)
		: "0" (page), "I" (PAGE_SIZE)
		: "memory");
}

/* R10000 has no Create_Dirty type cacheops.  */
static void andes_copy_page(void * to, void * from)
{
	unsigned long dummy1, dummy2, reg1, reg2, reg3, reg4;

	__asm__ __volatile__(
		".set\tnoreorder\n\t"
		".set\tnoat\n\t"
		"daddiu\t$1,%0,%8\n"
		"1:\tpref\t0,2*128(%1)\n\t"
		"pref\t1,2*128(%0)\n\t"
		"ld\t%2,(%1)\n\t"
		"ld\t%3,8(%1)\n\t"
		"ld\t%4,16(%1)\n\t"
		"ld\t%5,24(%1)\n\t"
		"sd\t%2,(%0)\n\t"
		"sd\t%3,8(%0)\n\t"
		"sd\t%4,16(%0)\n\t"
		"sd\t%5,24(%0)\n\t"
		"daddiu\t%0,64\n\t"
		"daddiu\t%1,64\n\t"
		"ld\t%2,-32(%1)\n\t"
		"ld\t%3,-24(%1)\n\t"
		"ld\t%4,-16(%1)\n\t"
		"ld\t%5,-8(%1)\n\t"
		"sd\t%2,-32(%0)\n\t"
		"sd\t%3,-24(%0)\n\t"
		"sd\t%4,-16(%0)\n\t"
		"bne\t$1,%0,1b\n\t"
		" sd\t%5,-8(%0)\n\t"
		".set\tat\n\t"
		".set\treorder"
		:"=r" (dummy1), "=r" (dummy2), "=&r" (reg1), "=&r" (reg2),
		 "=&r" (reg3), "=&r" (reg4)
		:"0" (to), "1" (from), "I" (PAGE_SIZE));
}

/* Cache operations.  These are only used with the virtual memory system,
   not for non-coherent I/O so it's ok to ignore the secondary caches.  */
static void
andes_flush_cache_l1(void)
{
	blast_dcache32(); blast_icache64();
}

/*
 * This is only used during initialization time. vmalloc() also calls
 * this, but that will be changed pretty soon.
 */
static void andes_flush_cache_l2(void)
{
	switch (sc_lsize()) {
		case 64:
			blast_scache64();
			break;
		case 128:
			blast_scache128();
			break;
		default:
			printk(KERN_EMERG "Unknown L2 line size\n");
			while(1);
	}
}

void
andes_flush_icache_page(unsigned long page)
{
	if (scache_lsz64)
		blast_scache64_page(page);
	else
		blast_scache128_page(page);
}

static void
andes_flush_cache_sigtramp(unsigned long addr)
{
	protected_writeback_dcache_line(addr & ~(dc_lsize - 1));
	protected_flush_icache_line(addr & ~(ic_lsize - 1));
}

#define NTLB_ENTRIES       64
#define NTLB_ENTRIES_HALF  32

void local_flush_tlb_all(void)
{
	unsigned long flags;
	unsigned long old_ctx;
	unsigned long entry;

#ifdef DEBUG_TLB
	printk("[tlball]");
#endif

	__save_and_cli(flags);
	/* Save old context and create impossible VPN2 value */
	old_ctx = get_entryhi() & ASID_MASK;
	set_entryhi(CKSEG0);
	set_entrylo0(0);
	set_entrylo1(0);

	entry = get_wired();

	/* Blast 'em all away. */
	while (entry < NTLB_ENTRIES) {
		set_index(entry);
		tlb_write_indexed();
		entry++;
	}
	set_entryhi(old_ctx);
	__restore_flags(flags);
}

void local_flush_tlb_mm(struct mm_struct *mm)
{
	if (CPU_CONTEXT(smp_processor_id(), mm) != 0) {
		unsigned long flags;

#ifdef DEBUG_TLB
		printk("[tlbmm<%d>]", mm->context);
#endif
		__save_and_cli(flags);
		get_new_mmu_context(mm, smp_processor_id());
		if(mm == current->mm)
			set_entryhi(CPU_CONTEXT(smp_processor_id(), mm)
				    & ASID_MASK);
		__restore_flags(flags);
	}
}

void local_flush_tlb_range(struct mm_struct *mm, unsigned long start,
                           unsigned long end)
{
	if (CPU_CONTEXT(smp_processor_id(), mm) != 0) {
		unsigned long flags;
		int size;

#ifdef DEBUG_TLB
		printk("[tlbrange<%02x,%08lx,%08lx>]",
		       (mm->context & ASID_MASK), start, end);
#endif
		__save_and_cli(flags);
		size = (end - start + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		size = (size + 1) >> 1;
		if (size <= NTLB_ENTRIES_HALF) {
			int oldpid = (get_entryhi() & ASID_MASK);
			int newpid = (CPU_CONTEXT(smp_processor_id(), mm)
				      & ASID_MASK);

			start &= (PAGE_MASK << 1);
			end += ((PAGE_SIZE << 1) - 1);
			end &= (PAGE_MASK << 1);
			while(start < end) {
				int idx;

				set_entryhi(start | newpid);
				start += (PAGE_SIZE << 1);
				tlb_probe();
				idx = get_index();
				set_entrylo0(0);
				set_entrylo1(0);
				set_entryhi(KSEG0);
				if(idx < 0)
					continue;
				tlb_write_indexed();
			}
			set_entryhi(oldpid);
		} else {
			get_new_mmu_context(mm, smp_processor_id());
			if(mm == current->mm)
				set_entryhi(CPU_CONTEXT(smp_processor_id(), mm)
					    & ASID_MASK);
		}
		__restore_flags(flags);
	}
}

void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
	if (CPU_CONTEXT(smp_processor_id(), vma->vm_mm) != 0) {
		unsigned long flags;
		int oldpid, newpid, idx;

#ifdef DEBUG_TLB
		printk("[tlbpage<%d,%08lx>]", vma->vm_mm->context, page);
#endif
		newpid = (CPU_CONTEXT(smp_processor_id(), vma->vm_mm) &
			  ASID_MASK);
		page &= (PAGE_MASK << 1);
		__save_and_cli(flags);
		oldpid = (get_entryhi() & ASID_MASK);
		set_entryhi(page | newpid);
		tlb_probe();
		idx = get_index();
		set_entrylo0(0);
		set_entrylo1(0);
		set_entryhi(KSEG0);
		if (idx < 0)
			goto finish;
		tlb_write_indexed();

	finish:
		set_entryhi(oldpid);
		__restore_flags(flags);
	}
}

/* XXX Simplify this.  On the R10000 writing a TLB entry for an virtual
   address that already exists will overwrite the old entry and not result
   in TLB malfunction or TLB shutdown.  */
static void andes_update_mmu_cache(struct vm_area_struct * vma,
                                   unsigned long address, pte_t pte)
{
	unsigned long flags;
	pgd_t *pgdp;
	pmd_t *pmdp;
	pte_t *ptep;
	int idx, pid;

	/*
	 * Handle debugger faulting in for debugee.
	 */
	if (current->active_mm != vma->vm_mm)
		return;

	pid = get_entryhi() & ASID_MASK;

	if ((pid != (CPU_CONTEXT(smp_processor_id(), vma->vm_mm) & ASID_MASK))
	    || (CPU_CONTEXT(smp_processor_id(), vma->vm_mm) == 0)) {
		printk(KERN_WARNING
		       "%s: Wheee, bogus tlbpid mmpid=%d tlbpid=%d\n",
		       __FUNCTION__, (int) (CPU_CONTEXT(smp_processor_id(),
		       vma->vm_mm) & ASID_MASK), pid);
	}

	__save_and_cli(flags);
	address &= (PAGE_MASK << 1);
	set_entryhi(address | (pid));
	pgdp = pgd_offset(vma->vm_mm, address);
	tlb_probe();
	pmdp = pmd_offset(pgdp, address);
	idx = get_index();
	ptep = pte_offset(pmdp, address);
	set_entrylo0(pte_val(*ptep++) >> 6);
	set_entrylo1(pte_val(*ptep) >> 6);
	set_entryhi(address | (pid));
	if (idx < 0) {
		tlb_write_random();
	} else {
		tlb_write_indexed();
	}
	set_entryhi(pid);
	__restore_flags(flags);
}

void __init ld_mmu_andes(void)
{
	printk("Primary instruction cache %dkb, linesize %d bytes\n",
	       icache_size >> 10, ic_lsize);
	printk("Primary data cache %dkb, linesize %d bytes\n",
	       dcache_size >> 10, dc_lsize);
	printk("Secondary cache sized at %ldK, linesize %ld\n",
	       scache_size() >> 10, sc_lsize());

	_clear_page = andes_clear_page;
	_copy_page = andes_copy_page;

	_flush_cache_l1 = andes_flush_cache_l1;
	_flush_cache_l2 = andes_flush_cache_l2;
	_flush_cache_sigtramp = andes_flush_cache_sigtramp;

	switch (sc_lsize()) {
		case 64:
			scache_lsz64 = 1;
			break;
		case 128:
			scache_lsz64 = 0;
			break;
		default:
			printk(KERN_EMERG "Unknown L2 line size\n");
			while(1);
	}

	_update_mmu_cache = andes_update_mmu_cache;

        flush_cache_l1();

	/*
	 * You should never change this register:
	 *   - On R4600 1.7 the tlbp never hits for pages smaller than
	 *     the value in the c0_pagemask register.
	 *   - The entire mm handling assumes the c0_pagemask register to
	 *     be set for 4kb pages.
	 */
	write_32bit_cp0_register(CP0_PAGEMASK, PM_4K);
	write_32bit_cp0_register(CP0_FRAMEMASK, 0);

        /* From this point on the ARC firmware is dead.  */
	local_flush_tlb_all();

	/* Did I tell you that ARC SUCKS?  */
}
