/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/traps.c
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003  Paul Mundt
 *
 */

/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'entry.S'.
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/processor.h>

#undef DEBUG_EXCEPTION
#ifdef DEBUG_EXCEPTION
/* implemented in ../lib/dbg.c */
extern void show_excp_regs(char *fname, int trapnr, int signr,
			   struct pt_regs *regs);
#else
#define show_excp_regs(a, b, c, d)
#endif

#define DO_ERROR(trapnr, signr, str, name, tsk) \
asmlinkage void do_##name(unsigned long error_code, struct pt_regs *regs) \
{ \
	show_excp_regs(__FUNCTION__, trapnr, signr, regs); \
	tsk->thread.error_code = error_code; \
	tsk->thread.trap_no = trapnr; \
	if (user_mode(regs)) force_sig(signr, tsk); \
	die_if_no_fixup(str,regs,error_code); \
}

spinlock_t die_lock;

void die(const char * str, struct pt_regs * regs, long err)
{
	console_verbose();
	spin_lock_irq(&die_lock);
	printk("%s: %lx\n", str, (err & 0xffffff));
	show_regs(regs);
	spin_unlock_irq(&die_lock);
	do_exit(SIGSEGV);
}

static inline void die_if_kernel(const char * str, struct pt_regs * regs, long err)
{
	if (!user_mode(regs))
		die(str, regs, err);
}

static void die_if_no_fixup(const char * str, struct pt_regs * regs, long err)
{
	if (!user_mode(regs))
	{
		unsigned long fixup;
		fixup = search_exception_table(regs->pc);
		if (fixup) {
			regs->pc = fixup;
			return;
		}
		die(str, regs, err);
	}
}

DO_ERROR( 7, SIGSEGV, "address error (load)", address_error_load, current)
DO_ERROR( 8, SIGSEGV, "address error (store)", address_error_store, current)
DO_ERROR(13, SIGILL,  "illegal slot instruction", illegal_slot_inst, current)
DO_ERROR(87, SIGSEGV, "address error (exec)", address_error_exec, current)

#if defined(CONFIG_SH64_ID2815_WORKAROUND)

#define OPCODE_INVALID      0
#define OPCODE_USER_VALID   1
#define OPCODE_PRIV_VALID   2

/* getcon/putcon - requires checking which control register is referenced. */
#define OPCODE_CTRL_REG     3

/* Table of valid opcodes for SHmedia mode.
   Form a 10-bit value by concatenating the major/minor opcodes i.e.
   opcode[31:26,20:16].  The 6 MSBs of this value index into the following
   array.  The 4 LSBs select the bit-pair in the entry (bits 1:0 correspond to
   LSBs==4'b0000 etc). */
static unsigned long shmedia_opcode_table[64] = {
	0x55554044,0x54445055,0x15141514,0x14541414,0x00000000,0x10001000,0x01110055,0x04050015,
	0x00000444,0xc0000000,0x44545515,0x40405555,0x55550015,0x10005555,0x55555505,0x04050000,
	0x00000555,0x00000404,0x00040445,0x15151414,0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000055,0x40404444,0x00000404,0xc0009495,0x00000000,0x00000000,0x00000000,0x00000000,
	0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,
	0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,
	0x80005050,0x04005055,0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,0x55555555,
	0x81055554,0x00000404,0x55555555,0x55555555,0x00000000,0x00000000,0x00000000,0x00000000
};

void do_reserved_inst(unsigned long error_code, struct pt_regs *regs)
{
	/* Workaround SH5-101 cut2 silicon defect #2815 :
	   in some situations, inter-mode branches from SHcompact -> SHmedia
	   which should take ITLBMISS or EXECPROT exceptions at the target
	   falsely take RESINST at the target instead. */

	unsigned long opcode;
	unsigned long pc, aligned_pc;
	int get_user_error;
	int trapnr = 12;
	int signr = SIGILL;
	char *exception_name = "reserved_instruction";

	pc = regs->pc;
	if ((pc & 3) == 1) {
		/* SHmedia : check for defect.  This requires executable vmas
		   to be readable too. */
		aligned_pc = pc & ~3;
		if (!access_ok(VERIFY_READ, aligned_pc, sizeof(unsigned long))) {
			get_user_error = -EFAULT;
		} else {
			get_user_error = __get_user(opcode, (unsigned long *)aligned_pc);
		}
		if (get_user_error >= 0) {
			unsigned long index, shift;
			unsigned long major, minor, combined;
			unsigned long reserved_field;
			reserved_field = opcode & 0xf; /* These bits are currently reserved as zero in all valid opcodes */
			major = (opcode >> 26) & 0x3f;
			minor = (opcode >> 16) & 0xf;
			combined = (major << 4) | minor;
			index = major;
			shift = minor << 1;
			if (reserved_field == 0) {
				int opcode_state = (shmedia_opcode_table[index] >> shift) & 0x3;
				switch (opcode_state) {
					case OPCODE_INVALID:
						/* Trap. */
						break;
					case OPCODE_USER_VALID:
						/* Restart the instruction : the branch to the instruction will now be from an RTE
						   not from SHcompact so the silicon defect won't be triggered. */
						return;
					case OPCODE_PRIV_VALID:
						if (!user_mode(regs)) {
							/* Should only ever get here if a module has
							   SHcompact code inside it.  If so, the same fix up is needed. */
							return; /* same reason */
						}
						/* Otherwise, user mode trying to execute a privileged instruction - 
						   fall through to trap. */
						break;
					case OPCODE_CTRL_REG:
						/* If in privileged mode, return as above. */
						if (!user_mode(regs)) return; 
						/* In user mode ... */
						if (combined == 0x9f) { /* GETCON */
							unsigned long regno = (opcode >> 20) & 0x3f;
							if (regno >= 62) {
								return;
							}
							/* Otherwise, reserved or privileged control register, => trap */
						} else if (combined == 0x1bf) { /* PUTCON */
							unsigned long regno = (opcode >> 4) & 0x3f;
							if (regno >= 62) {
								return;
							}
							/* Otherwise, reserved or privileged control register, => trap */
						} else {
							/* Trap */
						}
						break;
					default:
						/* Fall through to trap. */
						break;
				}
			}
			/* fall through to normal resinst processing */
		} else {
			/* Error trying to read opcode.  This typically means a
			   real fault, not a RESINST any more.  So change the
			   codes. */
			trapnr = 87;
			exception_name = "address error (exec)";
			signr = SIGSEGV;
		}
	}

	show_excp_regs("do_reserved_inst", trapnr, signr, regs);
	current->thread.error_code = error_code;
	current->thread.trap_no = trapnr;
	if (user_mode(regs)) force_sig(signr, current);
	die_if_no_fixup(exception_name, regs, error_code);
}

#else /* CONFIG_SH64_ID2815_WORKAROUND */

/* If the workaround isn't needed, this is just a straightforward reserved
   instruction */
DO_ERROR(12, SIGILL,  "reserved instruction", reserved_inst, current)

#endif /* CONFIG_SH64_ID2815_WORKAROUND */


#include <asm/system.h>

/* Called with interrupts disabled */
asmlinkage void do_exception_error(unsigned long ex, struct pt_regs *regs)
{
	PLS();
	show_excp_regs(__FUNCTION__, -1, -1, regs);
	die_if_kernel("exception", regs, ex);
}

int do_unknown_trapa(unsigned long scId, struct pt_regs *regs)
{	
	/* Syscall debug */
        printk("System call ID error: [0x1#args:8 #syscall:16  0x%lx]\n", scId);

	die_if_kernel("unknown trapa", regs, scId);

	return -ENOSYS;
}

void show_task(unsigned long *sp)
{
	unsigned long module_start, module_end;
	unsigned long kernel_start, kernel_end;
	unsigned long *stack;
	extern int _text, _etext;
	int i = 1;

	kernel_start = (unsigned long)&_text;
	kernel_end   = (unsigned long)&_etext;

	module_start = VMALLOC_START;
	module_end   = VMALLOC_END;

	if (!sp) {
		/*
		 * If we haven't specified a sane sp, fetch it..
		 */
		__asm__ __volatile__ (
			"or	r15, r63, %0\n\t"
			"getcon " __c17 ", %1\n\t"
			: "=r" (module_start),
			  "=r" (module_end)
		);

		sp = (unsigned long *)module_start;
	}

	stack = sp;

	printk("\nCall Trace: ");

	while ((unsigned long)stack & (PAGE_SIZE - 1)) {
		unsigned long addr = *stack++;

		if ((addr >= kernel_start && addr < kernel_end) ||
		    (addr >= module_start && addr < module_end)) {
			/* 
			 * Do a bit of formatting here.. on an 80 column
			 * display, 6 entries is the most we can deal with
			 * per-line, since each address will take up 13 spaces.
			 */
			if (i && ((i % 6) == 0))
				printk("\n       ");

			printk("[<%08lx>] ", addr);
			i++;
		}
	}

	printk("\n");
}

void show_trace_task(struct task_struct *tsk)
{
	show_task((unsigned long *)tsk->thread.sp);
}

void dump_stack(void)
{
	show_task(NULL);
}

