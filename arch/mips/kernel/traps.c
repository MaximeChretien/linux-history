/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999, 2000, 01 Ralf Baechle
 * Modified for R3000 by Paul M. Antoine, 1995, 1996
 * Complete output from die() by Ulf Carlsson, 1998
 * Copyright (C) 1999 Silicon Graphics, Inc.
 *
 * Kevin D. Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000, 01 MIPS Technologies, Inc.
 * Copyright (C) 2002  Maciej W. Rozycki
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>

#include <asm/bootinfo.h>
#include <asm/branch.h>
#include <asm/cpu.h>
#include <asm/cachectl.h>
#include <asm/inst.h>
#include <asm/jazz.h>
#include <asm/module.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/siginfo.h>
#include <asm/watch.h>
#include <asm/types.h>
#include <asm/system.h>
#include <asm/traps.h>
#include <asm/uaccess.h>
#include <asm/mmu_context.h>

/*
 * Machine specific interrupt handlers
 */
extern asmlinkage void acer_pica_61_handle_int(void);
extern asmlinkage void decstation_handle_int(void);
extern asmlinkage void deskstation_rpc44_handle_int(void);
extern asmlinkage void deskstation_tyne_handle_int(void);
extern asmlinkage void mips_magnum_4000_handle_int(void);

extern asmlinkage void handle_mod(void);
extern asmlinkage void handle_tlbl(void);
extern asmlinkage void handle_tlbs(void);
extern asmlinkage void handle_adel(void);
extern asmlinkage void handle_ades(void);
extern asmlinkage void handle_ibe(void);
extern asmlinkage void handle_dbe(void);
extern asmlinkage void handle_sys(void);
extern asmlinkage void handle_bp(void);
extern asmlinkage void handle_ri(void);
extern asmlinkage void handle_cpu(void);
extern asmlinkage void handle_ov(void);
extern asmlinkage void handle_tr(void);
extern asmlinkage void handle_fpe(void);
extern asmlinkage void handle_watch(void);
extern asmlinkage void handle_mcheck(void);
extern asmlinkage void handle_reserved(void);

extern int fpu_emulator_cop1Handler(int xcptno, struct pt_regs *xcp,
	struct mips_fpu_soft_struct *ctx);

char watch_available = 0;

int (*be_board_handler)(struct pt_regs *regs, int is_fixup);

int kstack_depth_to_print = 24;

/*
 * These constant is for searching for possible module text segments.
 * MODULE_RANGE is a guess of how much space is likely to be vmalloced.
 */
#define MODULE_RANGE (8*1024*1024)

/*
 * This stuff is needed for the userland ll-sc emulation for R2300
 */

#define OPCODE 0xfc000000
#define BASE   0x03e00000
#define RT     0x001f0000
#define OFFSET 0x0000ffff
#define LL     0xc0000000
#define SC     0xe0000000

/*
 * The ll_bit is cleared by r*_switch.S
 */

unsigned long ll_bit;
#ifdef CONFIG_PROC_FS
extern unsigned long ll_ops;
extern unsigned long sc_ops;
#endif

static struct task_struct *ll_task = NULL;

static inline void simulate_ll(struct pt_regs *regp, unsigned int opcode)
{
	unsigned long value, *vaddr;
	long offset;
	int signal = 0;

	/*
	 * analyse the ll instruction that just caused a ri exception
	 * and put the referenced address to addr.
	 */

	/* sign extend offset */
	offset = opcode & OFFSET;
	offset <<= 16;
	offset >>= 16;

	vaddr = (unsigned long *)((long)(regp->regs[(opcode & BASE) >> 21]) + offset);

#ifdef CONFIG_PROC_FS
	ll_ops++;
#endif

	if ((unsigned long)vaddr & 3)
		signal = SIGBUS;
	else if (get_user(value, vaddr))
		signal = SIGSEGV;
	else {
		if (ll_task == NULL || ll_task == current) {
			ll_bit = 1;
		} else {
			ll_bit = 0;
		}
		ll_task = current;
		regp->regs[(opcode & RT) >> 16] = value;
	}
	if (compute_return_epc(regp))
		return;
	if (signal)
		send_sig(signal, current, 1);
}

static inline void simulate_sc(struct pt_regs *regp, unsigned int opcode)
{
	unsigned long *vaddr, reg;
	long offset;
	int signal = 0;

	/*
	 * analyse the sc instruction that just caused a ri exception
	 * and put the referenced address to addr.
	 */

	/* sign extend offset */
	offset = opcode & OFFSET;
	offset <<= 16;
	offset >>= 16;

	vaddr = (unsigned long *)((long)(regp->regs[(opcode & BASE) >> 21]) + offset);
	reg = (opcode & RT) >> 16;

#ifdef CONFIG_PROC_FS
	sc_ops++;
#endif

	if ((unsigned long)vaddr & 3)
		signal = SIGBUS;
	else if (ll_bit == 0 || ll_task != current)
		regp->regs[reg] = 0;
	else if (put_user(regp->regs[reg], vaddr))
		signal = SIGSEGV;
	else
		regp->regs[reg] = 1;
	if (compute_return_epc(regp))
		return;
	if (signal)
		send_sig(signal, current, 1);
}

/*
 * If the address is either in the .text section of the
 * kernel, or in the vmalloc'ed module regions, it *may*
 * be the address of a calling routine
 */

#ifdef CONFIG_MODULES

extern struct module *module_list;
extern struct module kernel_module;

static inline int kernel_text_address(long addr)
{
	extern char _stext, _etext;
	int retval = 0;
	struct module *mod;

	if (addr >= (long) &_stext && addr <= (long) &_etext)
		return 1;

	for (mod = module_list; mod != &kernel_module; mod = mod->next) {
		/* mod_bound tests for addr being inside the vmalloc'ed
		 * module area. Of course it'd be better to test only
		 * for the .text subset... */
		if (mod_bound(addr, 0, mod)) {
			retval = 1;
			break;
		}
	}

	return retval;
}

#else

static inline int kernel_text_address(long addr)
{
	extern char _stext, _etext;

	return (addr >= (long) &_stext && addr <= (long) &_etext);
}

#endif

/*
 * This routine abuses get_user()/put_user() to reference pointers
 * with at least a bit of error checking ...
 */
void show_stack(long *sp)
{
	int i;
	long stackdata;

	sp = sp ? sp : (long *)&sp;

	printk("Stack:   ");
	i = 1;
	while ((long) sp & (PAGE_SIZE - 1)) {
		if (i && ((i % 8) == 0))
			printk("\n");
		if (i > 40) {
			printk(" ...");
			break;
		}

		if (__get_user(stackdata, sp++)) {
			printk(" (Bad stack address)");
			break;
		}

		printk(" %08lx", stackdata);
		i++;
	}
	printk("\n");
}

void show_trace(long *sp)
{
	int i;
	long addr;

	sp = sp ? sp : (long *) &sp;

	printk("Call Trace:  ");
	i = 1;
	while ((long) sp & (PAGE_SIZE - 1)) {

		if (__get_user(addr, sp++)) {
			if (i && ((i % 6) == 0))
				printk("\n");
			printk(" (Bad stack address)\n");
			break;
		}

		/*
		 * If the address is either in the text segment of the
		 * kernel, or in the region which contains vmalloc'ed
		 * memory, it *may* be the address of a calling
		 * routine; if so, print it so that someone tracing
		 * down the cause of the crash will be able to figure
		 * out the call path that was taken.
		 */

		if (kernel_text_address(addr)) {
			if (i && ((i % 6) == 0))
				printk("\n");
			if (i > 40) {
				printk(" ...");
				break;
			}

			printk(" [<%08lx>]", addr);
			i++;
		}
	}
	printk("\n");
}

void show_trace_task(struct task_struct *tsk)
{
	show_trace((long *)tsk->thread.reg29);
}

void show_code(unsigned int *pc)
{
	long i;

	printk("\nCode:");

	for(i = -3 ; i < 6 ; i++) {
		unsigned long insn;
		if (__get_user(insn, pc + i)) {
			printk(" (Bad address in epc)\n");
			break;
		}
		printk("%c%08lx%c",(i?' ':'<'),insn,(i?' ':'>'));
	}
}

void show_regs(struct pt_regs * regs)
{
	/*
	 * Saved main processor registers
	 */
	printk("$0 : %08x %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       0,             regs->regs[1], regs->regs[2], regs->regs[3],
	       regs->regs[4], regs->regs[5], regs->regs[6], regs->regs[7]);
	printk("$8 : %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       regs->regs[8],  regs->regs[9],  regs->regs[10], regs->regs[11],
               regs->regs[12], regs->regs[13], regs->regs[14], regs->regs[15]);
	printk("$16: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       regs->regs[16], regs->regs[17], regs->regs[18], regs->regs[19],
               regs->regs[20], regs->regs[21], regs->regs[22], regs->regs[23]);
	printk("$24: %08lx %08lx                   %08lx %08lx %08lx %08lx\n",
	       regs->regs[24], regs->regs[25],
	       regs->regs[28], regs->regs[29], regs->regs[30], regs->regs[31]);
	printk("Hi : %08lx\n", regs->hi);
	printk("Lo : %08lx\n", regs->lo);

	/*
	 * Saved cp0 registers
	 */
	printk("epc  : %08lx    %s\nStatus: %08lx\nCause : %08lx\n",
	       regs->cp0_epc, print_tainted(), regs->cp0_status,
	       regs->cp0_cause);
}

void show_registers(struct pt_regs *regs)
{
	show_regs(regs);
	printk("Process %s (pid: %d, stackpage=%08lx)\n",
		current->comm, current->pid, (unsigned long) current);
	show_stack((long *) regs->regs[29]);
	show_trace((long *) regs->regs[29]);
	show_code((unsigned int *) regs->cp0_epc);
	printk("\n");
}

static spinlock_t die_lock = SPIN_LOCK_UNLOCKED;

void __die(const char * str, struct pt_regs * regs, const char * file,
	   const char * func, unsigned long line)
{
	console_verbose();
	spin_lock_irq(&die_lock);
	printk("%s", str);
	if (file && func)
		printk(" in %s:%s, line %ld", file, func, line);
	printk(":\n");
	show_registers(regs);
	spin_unlock_irq(&die_lock);
	do_exit(SIGSEGV);
}

void __die_if_kernel(const char * str, struct pt_regs * regs,
		     const char * file, const char * func, unsigned long line)
{
	if (!user_mode(regs))
		__die(str, regs, file, func, line);
}

extern const struct exception_table_entry __start___dbe_table[];
extern const struct exception_table_entry __stop___dbe_table[];

void __declare_dbe_table(void)
{
	__asm__ __volatile__(
	".section\t__dbe_table,\"a\"\n\t"
	".previous"
	);
}

static inline unsigned long
search_one_table(const struct exception_table_entry *first,
		 const struct exception_table_entry *last,
		 unsigned long value)
{
	const struct exception_table_entry *mid;
	long diff;

	while (first < last) {
		mid = (last - first) / 2 + first;
		diff = mid->insn - value;
		if (diff < 0)
			first = mid + 1;
		else
			last = mid;
	}
	return (first == last && first->insn == value) ? first->nextinsn : 0;
}

extern spinlock_t modlist_lock;

static inline unsigned long
search_dbe_table(unsigned long addr)
{
	unsigned long ret = 0;

#ifndef CONFIG_MODULES
	/* There is only the kernel to search.  */
	ret = search_one_table(__start___dbe_table, __stop___dbe_table-1, addr);
	return ret;
#else
	unsigned long flags;

	/* The kernel is the last "module" -- no need to treat it special.  */
	struct module *mp;
	struct archdata *ap;

	spin_lock_irqsave(&modlist_lock, flags);
	for (mp = module_list; mp != NULL; mp = mp->next) {
		if (!mod_member_present(mp, archdata_end) ||
        	    !mod_archdata_member_present(mp, struct archdata,
						 dbe_table_end))
			continue;
		ap = (struct archdata *)(mp->archdata_start);

		if (ap->dbe_table_start == NULL ||
		    !(mp->flags & (MOD_RUNNING | MOD_INITIALIZING)))
			continue;
		ret = search_one_table(ap->dbe_table_start,
				       ap->dbe_table_end - 1, addr);
		if (ret)
			break;
	}
	spin_unlock_irqrestore(&modlist_lock, flags);
	return ret;
#endif
}

asmlinkage void do_be(struct pt_regs *regs)
{
	unsigned long new_epc;
	unsigned long fixup = 0;
	int data = regs->cp0_cause & 4;
	int action = MIPS_BE_FATAL;

	if (data && !user_mode(regs))
		fixup = search_dbe_table(regs->cp0_epc);

	if (fixup)
		action = MIPS_BE_FIXUP;

	if (be_board_handler)
		action = be_board_handler(regs, fixup != 0);

	switch (action) {
	case MIPS_BE_DISCARD:
		return;
	case MIPS_BE_FIXUP:
		if (fixup) {
			new_epc = fixup_exception(dpf_reg, fixup,
						  regs->cp0_epc);
			regs->cp0_epc = new_epc;
			return;
		}
		break;
	default:
		break;
	}

	/*
	 * Assume it would be too dangerous to continue ...
	 */
	printk(KERN_ALERT "%s bus error, epc == %08lx, ra == %08lx\n",
	       data ? "Data" : "Instruction",
	       regs->cp0_epc, regs->regs[31]);
	die_if_kernel("Oops", regs);
	force_sig(SIGBUS, current);
}

asmlinkage void do_ov(struct pt_regs *regs)
{
	siginfo_t info;

	if (compute_return_epc(regs))
		return;

	info.si_code = FPE_INTOVF;
	info.si_signo = SIGFPE;
	info.si_errno = 0;
	info.si_addr = (void *)regs->cp0_epc;
	force_sig_info(SIGFPE, &info, current);
}

/*
 * XXX Delayed fp exceptions when doing a lazy ctx switch XXX
 */
asmlinkage void do_fpe(struct pt_regs *regs, unsigned long fcr31)
{
	if (fcr31 & FPU_CSR_UNI_X) {
		int sig;

		/*
	 	 * Unimplemented operation exception.  If we've got the full
		 * software emulator on-board, let's use it...
		 *
		 * Force FPU to dump state into task/thread context.  We're
		 * moving a lot of data here for what is probably a single
		 * instruction, but the alternative is to pre-decode the FP
		 * register operands before invoking the emulator, which seems
		 * a bit extreme for what should be an infrequent event.
		 */
		save_fp(current);

		/* Run the emulator */
		sig = fpu_emulator_cop1Handler (0, regs,
			&current->thread.fpu.soft);

		/*
		 * We can't allow the emulated instruction to leave any of
		 * the cause bit set in $fcr31.
		 */
		current->thread.fpu.soft.sr &= ~FPU_CSR_ALL_X;

		/* Restore the hardware register state */
		restore_fp(current);

		/* If something went wrong, signal */
		if (sig)
		{
			/*
			 * Return EPC is not calculated in the FPU emulator,
			 * if a signal is being send. So we calculate it here.
			 */
			compute_return_epc(regs);
			force_sig(sig, current);
		}

		return;
	}

	if (compute_return_epc(regs))
		return;
	force_sig(SIGFPE, current);
}

static inline int get_insn_opcode(struct pt_regs *regs, unsigned int *opcode)
{
	unsigned long *epc;

	epc = (unsigned long *) regs->cp0_epc +
	      ((regs->cp0_cause & CAUSEF_BD) != 0);
	if (!get_user(*opcode, epc))
		return 0;

	force_sig(SIGSEGV, current);
	return 1;
}

asmlinkage void do_bp(struct pt_regs *regs)
{
	unsigned int opcode, bcode;
	siginfo_t info;

	if (get_insn_opcode(regs, &opcode))
		return;

	/*
	 * There is the ancient bug in the MIPS assemblers that the break
	 * code starts left to bit 16 instead to bit 6 in the opcode.
	 * Gas is bug-compatible ...
	 */
	bcode = ((opcode >> 16) & ((1 << 20) - 1));

	/*
	 * (A short test says that IRIX 5.3 sends SIGTRAP for all break
	 * insns, even for break codes that indicate arithmetic failures.
	 * Weird ...)
	 * But should we continue the brokenness???  --macro
	 */
	switch (bcode) {
	case 6:
	case 7:
		if (bcode == 7)
			info.si_code = FPE_INTDIV;
		else
			info.si_code = FPE_INTOVF;
		info.si_signo = SIGFPE;
		info.si_errno = 0;
		info.si_addr = (void *)regs->cp0_epc;
		force_sig_info(SIGFPE, &info, current);
		break;
	default:
		force_sig(SIGTRAP, current);
	}
}

asmlinkage void do_tr(struct pt_regs *regs)
{
	siginfo_t info;
	unsigned int opcode, tcode = 0;

	if (get_insn_opcode(regs, &opcode))
		return;

	/* Immediate versions don't provide a code.  */
	if (!(opcode & OPCODE))
		tcode = ((opcode >> 6) & ((1 << 20) - 1));

	/*
	 * (A short test says that IRIX 5.3 sends SIGTRAP for all trap
	 * insns, even for trap codes that indicate arithmetic failures.
	 * Weird ...)
	 * But should we continue the brokenness???  --macro
	 */
	switch (tcode) {
	case 6:
	case 7:
		if (tcode == 7)
			info.si_code = FPE_INTDIV;
		else
			info.si_code = FPE_INTOVF;
		info.si_signo = SIGFPE;
		info.si_errno = 0;
		info.si_addr = (void *)regs->cp0_epc;
		force_sig_info(SIGFPE, &info, current);
		break;
	default:
		force_sig(SIGTRAP, current);
	}
}

/*
 * userland emulation for R2300 CPUs
 * needed for the multithreading part of glibc
 *
 * this implementation can handle only sychronization between 2 or more
 * user contexts and is not SMP safe.
 */
asmlinkage void do_ri(struct pt_regs *regs)
{
	die_if_kernel("Reserved instruction in kernel code", regs);

#ifndef CONFIG_CPU_HAS_LLSC

#ifdef CONFIG_SMP
#error "ll/sc emulation is not SMP safe"
#endif

	{
	unsigned int opcode;

	if (!get_insn_opcode(regs, &opcode)) {
		if ((opcode & OPCODE) == LL) {
			simulate_ll(regs, opcode);
			return;
		}
		if ((opcode & OPCODE) == SC) {
			simulate_sc(regs, opcode);
			return;
		}
	}
	}
#endif /* CONFIG_CPU_HAS_LLSC */

	if (compute_return_epc(regs))
		return;
	force_sig(SIGILL, current);
}

asmlinkage void do_cpu(struct pt_regs *regs)
{
	unsigned int cpid;
	void fpu_emulator_init_fpu(void);
	int sig;

	cpid = (regs->cp0_cause >> CAUSEB_CE) & 3;
	if (cpid != 1)
		goto bad_cid;

	if (!(mips_cpu.options & MIPS_CPU_FPU))
		goto fp_emul;

	regs->cp0_status |= ST0_CU1;
	if (last_task_used_math == current)
		return;

	if (current->used_math) {		/* Using the FPU again.  */
		lazy_fpu_switch(last_task_used_math);
	} else {				/* First time FPU user.  */
		if (last_task_used_math != NULL)
			save_fp(last_task_used_math);
		init_fpu();
		current->used_math = 1;
	}
	last_task_used_math = current;

	return;

fp_emul:
	if (last_task_used_math != current) {
		if (!current->used_math) {
			fpu_emulator_init_fpu();
			current->used_math = 1;
		}
	}
	sig = fpu_emulator_cop1Handler(0, regs, &current->thread.fpu.soft);
	last_task_used_math = current;
	if (sig) {
		/*
		 * Return EPC is not calculated in the FPU emulator, if
		 * a signal is being send. So we calculate it here.
		 */
		compute_return_epc(regs);
		force_sig(sig, current);
	}
	return;

bad_cid:
#ifndef CONFIG_CPU_HAS_LLSC
	switch (mips_cpu.cputype) {
	case CPU_TX3927:
	case CPU_TX39XX:
		do_ri(regs);
		return;
	}
#endif
	compute_return_epc(regs);
	force_sig(SIGILL, current);
}

asmlinkage void do_watch(struct pt_regs *regs)
{
	extern void dump_tlb_all(void);

	/*
	 * We use the watch exception where available to detect stack
	 * overflows.
	 */
	dump_tlb_all();
	show_regs(regs);
	panic("Caught WATCH exception - probably caused by stack overflow.");
}

asmlinkage void do_mcheck(struct pt_regs *regs)
{
	show_regs(regs);
	dump_tlb_all();
	/*
	 * Some chips may have other causes of machine check (e.g. SB1
	 * graduation timer)
	 */
	panic("Caught Machine Check exception - %scaused by multiple "
	      "matching entries in the TLB.",
	      (regs->cp0_status & ST0_TS) ? "" : "not ");
}

asmlinkage void do_reserved(struct pt_regs *regs)
{
	/*
	 * Game over - no way to handle this if it ever occurs.  Most probably
	 * caused by a new unknown cpu type or after another deadly
	 * hard/software error.
	 */
	show_regs(regs);
	panic("Caught reserved exception - should not happen.");
}

static inline void watch_init(void)
{
	if (mips_cpu.options & MIPS_CPU_WATCH ) {
		set_except_vector(23, handle_watch);
 		watch_available = 1;
 	}
}

/*
 * Some MIPS CPUs can enable/disable for cache parity detection, but do
 * it different ways.
 */
static inline void parity_protection_init(void)
{
	switch (mips_cpu.cputype) {
	case CPU_5KC:
		/* Set the PE bit (bit 31) in the CP0_ECC register. */
		printk(KERN_INFO "Enable the cache parity protection for "
		       "MIPS 5KC CPUs.\n");
		write_32bit_cp0_register(CP0_ECC,
		                         read_32bit_cp0_register(CP0_ECC)
		                         | 0x80000000);
		break;
	default:
		break;
	}
}

asmlinkage void cache_parity_error(void)
{
	unsigned int reg_val;

	/* For the moment, report the problem and hang. */
	reg_val = read_32bit_cp0_register(CP0_ERROREPC);
	printk("Cache error exception:\n");
	printk("cp0_errorepc == %08x\n", reg_val);
	reg_val = read_32bit_cp0_register(CP0_CACHEERR);
	printk("cp0_cacheerr == %08x\n", reg_val);

	printk("Decoded CP0_CACHEERR: %s cache fault in %s reference.\n",
	       reg_val & (1<<30) ? "secondary" : "primary",
	       reg_val & (1<<31) ? "data" : "insn");
	printk("Error bits: %s%s%s%s%s%s%s\n",
	       reg_val & (1<<29) ? "ED " : "",
	       reg_val & (1<<28) ? "ET " : "",
	       reg_val & (1<<26) ? "EE " : "",
	       reg_val & (1<<25) ? "EB " : "",
	       reg_val & (1<<24) ? "EI " : "",
	       reg_val & (1<<23) ? "E1 " : "",
	       reg_val & (1<<22) ? "E0 " : "");
	printk("IDX: 0x%08x\n", reg_val & ((1<<22)-1));

	if (reg_val&(1<<22))
		printk("DErrAddr0: 0x%08x\n",
		       read_32bit_cp0_set1_register(CP0_S1_DERRADDR0));

	if (reg_val&(1<<23))
		printk("DErrAddr1: 0x%08x\n",
		       read_32bit_cp0_set1_register(CP0_S1_DERRADDR1));

	panic("Can't handle the cache error!");
}

/*
 * SDBBP EJTAG debug exception handler.
 * We skip the instruction and return to the next instruction.
 */
void ejtag_exception_handler(struct pt_regs *regs)
{
        unsigned int depc, old_epc, debug;

        printk("SDBBP EJTAG debug exception - not handled yet, just ignored!\n");
        depc = read_32bit_cp0_register(CP0_DEPC);
        debug = read_32bit_cp0_register(CP0_DEBUG);
        printk("DEPC = %08x, DEBUG = %08x\n", depc, debug);
        if (debug & 0x80000000) {
                /*
                 * In branch delay slot.
                 * We cheat a little bit here and use EPC to calculate the
                 * debug return address (DEPC). EPC is restored after the
                 * calculation.
                 */
                old_epc = regs->cp0_epc;
                regs->cp0_epc = depc;
                __compute_return_epc(regs);
                depc = regs->cp0_epc;
                regs->cp0_epc = old_epc;
        } else
                depc += 4;
        write_32bit_cp0_register(CP0_DEPC, depc);

#if 0
	printk("\n\n----- Enable EJTAG single stepping ----\n\n");
	write_32bit_cp0_register(CP0_DEBUG, debug | 0x100);
#endif
}

/*
 * NMI exception handler.
 */
void nmi_exception_handler(struct pt_regs *regs)
{
        printk("NMI taken!!!!\n");
        die("NMI", regs);
        while(1) ;  /* We die here. */
}

unsigned long exception_handlers[32];

/*
 * As a side effect of the way this is implemented we're limited
 * to interrupt handlers in the address range from
 * KSEG0 <= x < KSEG0 + 256mb on the Nevada.  Oh well ...
 */
void *set_except_vector(int n, void *addr)
{
	unsigned long handler = (unsigned long) addr;
	unsigned long old_handler = exception_handlers[n];

	exception_handlers[n] = handler;
	if (n == 0 && mips_cpu.options & MIPS_CPU_DIVEC) {
		*(volatile u32 *)(KSEG0+0x200) = 0x08000000 |
		                                 (0x03ffffff & (handler >> 2));
		flush_icache_range(KSEG0+0x200, KSEG0 + 0x204);
	}
	return (void *)old_handler;
}

asmlinkage int (*save_fp_context)(struct sigcontext *sc);
asmlinkage int (*restore_fp_context)(struct sigcontext *sc);
extern asmlinkage int _save_fp_context(struct sigcontext *sc);
extern asmlinkage int _restore_fp_context(struct sigcontext *sc);

extern asmlinkage int fpu_emulator_save_context(struct sigcontext *sc);
extern asmlinkage int fpu_emulator_restore_context(struct sigcontext *sc);

void __init per_cpu_trap_init(void)
{
	unsigned int cpu = smp_processor_id();

	/* Some firmware leaves the BEV flag set, clear it.  */
	clear_cp0_status(ST0_BEV);

	/*
	 * Some MIPS CPUs have a dedicated interrupt vector which reduces the
	 * interrupt processing overhead.  Use it where available.
	 */
	if (mips_cpu.options & MIPS_CPU_DIVEC)
		set_cp0_cause(CAUSEF_IV);

	cpu_data[cpu].asid_cache = ASID_FIRST_VERSION;
	set_context(cpu << 23);
}

void __init trap_init(void)
{
	extern char except_vec1_generic, except_vec2_generic;
	extern char except_vec3_generic, except_vec3_r4000;
	extern char except_vec4;
	extern char except_vec_ejtag_debug;
	unsigned long i;

	per_cpu_trap_init();

	/* Copy the generic exception handler code to it's final destination. */
	memcpy((void *)(KSEG0 + 0x80), &except_vec1_generic, 0x80);
	memcpy((void *)(KSEG0 + 0x100), &except_vec2_generic, 0x80);
	memcpy((void *)(KSEG0 + 0x180), &except_vec3_generic, 0x80);

	/*
	 * Setup default vectors
	 */
	for (i = 0; i <= 31; i++)
		set_except_vector(i, handle_reserved);

	/*
	 * Copy the EJTAG debug exception vector handler code to it's final
	 * destination.
	 */
	if (mips_cpu.options & MIPS_CPU_EJTAG)
		memcpy((void *)(KSEG0 + 0x300), &except_vec_ejtag_debug, 0x80);

	/*
	 * Only some CPUs have the watch exceptions or a dedicated
	 * interrupt vector.
	 */
	watch_init();

	/*
	 * Some MIPS CPUs have a dedicated interrupt vector which reduces the
	 * interrupt processing overhead.  Use it where available.
	 */
	if (mips_cpu.options & MIPS_CPU_DIVEC) {
		memcpy((void *)(KSEG0 + 0x200), &except_vec4, 8);
		set_cp0_cause(CAUSEF_IV);
	}

	/*
	 * Some CPUs can enable/disable for cache parity detection, but does
	 * it different ways.
	 */
	parity_protection_init();

	/*
	 * The Data Bus Errors / Instruction Bus Errors are signaled
	 * by external hardware.  Therefore these two exceptions
	 * may have board specific handlers.
	 */
	bus_error_init();

	set_except_vector(1, handle_mod);
	set_except_vector(2, handle_tlbl);
	set_except_vector(3, handle_tlbs);
	set_except_vector(4, handle_adel);
	set_except_vector(5, handle_ades);

	set_except_vector(6, handle_ibe);
	set_except_vector(7, handle_dbe);

	set_except_vector(8, handle_sys);
	set_except_vector(9, handle_bp);
	set_except_vector(10, handle_ri);
	set_except_vector(11, handle_cpu);
	set_except_vector(12, handle_ov);
	set_except_vector(13, handle_tr);

	if ((mips_cpu.options & MIPS_CPU_FPU) &&
	    !(mips_cpu.options & MIPS_CPU_NOFPUEX))
		set_except_vector(15, handle_fpe);
	if (mips_cpu.options & MIPS_CPU_MCHECK)
		set_except_vector(24, handle_mcheck);

	/*
	 * Handling the following exceptions depends mostly of the cpu type
	 */
	if ((mips_cpu.options & MIPS_CPU_4KEX)
	    && (mips_cpu.options & MIPS_CPU_4KTLB)) {
		/* Cache error vector already set above.  */

		if (mips_cpu.options & MIPS_CPU_VCE) {
			memcpy((void *)(KSEG0 + 0x180), &except_vec3_r4000,
			       0x80);
		}
	} else switch (mips_cpu.cputype) {
	case CPU_SB1:
		/*
		 * XXX - This should be folded in to the "cleaner" handling,
		 * above
		 */
		memcpy((void *)(KSEG0 + 0x180), &except_vec3_r4000, 0x80);
#ifdef CONFIG_SB1_CACHE_ERROR
		{
		/* Special cache error handler for SB1 */
		extern char except_vec2_sb1;
		memcpy((void *)(KSEG0 + 0x100), &except_vec2_sb1, 0x80);
		memcpy((void *)(KSEG1 + 0x100), &except_vec2_sb1, 0x80);
		}
#endif

		/* Enable timer interrupt and scd mapped interrupt */
		clear_cp0_status(0xf000);
		set_cp0_status(0xc00);
		break;
	case CPU_R6000:
	case CPU_R6000A:
		/*
		 * The R6000 is the only R-series CPU that features a machine
		 * check exception (similar to the R4000 cache error) and
		 * unaligned ldc1/sdc1 exception.  The handlers have not been
		 * written yet.  Well, anyway there is no R6000 machine on the
		 * current list of targets for Linux/MIPS.
		 * (Duh, crap, there is someone with a tripple R6k machine)
		 */
		//set_except_vector(14, handle_mc);
		//set_except_vector(15, handle_ndc);
	case CPU_R2000:
	case CPU_R3000:
	case CPU_R3000A:
	case CPU_R3041:
	case CPU_R3051:
	case CPU_R3052:
	case CPU_R3081:
	case CPU_R3081E:
	case CPU_TX3912:
	case CPU_TX3922:
	case CPU_TX3927:
	case CPU_TX39XX:
		memcpy((void *)(KSEG0 + 0x80), &except_vec3_generic, 0x80);
		break;

	case CPU_UNKNOWN:
	default:
		panic("Unknown CPU type");
	}

	flush_icache_range(KSEG0, KSEG0 + 0x400);

	if (mips_cpu.options & MIPS_CPU_FPU) {
	        save_fp_context = _save_fp_context;
		restore_fp_context = _restore_fp_context;
	} else {
		save_fp_context = fpu_emulator_save_context;
		restore_fp_context = fpu_emulator_restore_context;
	}

	if (mips_cpu.isa_level == MIPS_CPU_ISA_IV)
		set_cp0_status(ST0_XX);

	atomic_inc(&init_mm.mm_count);	/* XXX  UP?  */
	current->active_mm = &init_mm;

	/* XXX Must be done for all CPUs  */
	current_cpu_data.asid_cache = ASID_FIRST_VERSION;
	TLBMISS_HANDLER_SETUP();
}
