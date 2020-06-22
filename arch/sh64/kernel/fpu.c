/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/fpu.c
 *
 * Copyright (C) 2001  Manuela Cirronis, Paolo Alberelli
 *
 * Started from SH4 version:
 *   Copyright (C) 1999, 2000  Kaz Kojima & Niibe Yutaka
 *
 */

#include <linux/sched.h>
#include <linux/signal.h>
#include <asm/processor.h>
#include <asm/user.h>
#include <asm/io.h>

/*
 * Initially load the FPU with signalling NANS.  This bit pattern
 * has the property that no matter whether considered as single or as
 * double precision, it still represents a signalling NAN.  
 */
#define sNAN64		0xFFFFFFFFFFFFFFFF

static struct sh_fpu_hard_struct init_fpuregs = {
	{ [0 ... 31] = sNAN64 },
	FPSCR_INIT
};

inline void fpsave(struct sh_fpu_hard_struct *fpregs)
{
	asm volatile("fst.d     %0, (0*8),"  __d(0)  "\n\t"
		     "fst.d     %0, (1*8),"  __d(2)  "\n\t"
		     "fst.d     %0, (2*8),"  __d(4)  "\n\t"
		     "fst.d     %0, (3*8),"  __d(6)  "\n\t"
		     "fst.d     %0, (4*8),"  __d(8)  "\n\t"
		     "fst.d     %0, (5*8),"  __d(10) "\n\t"
		     "fst.d     %0, (6*8),"  __d(12) "\n\t"
		     "fst.d     %0, (7*8),"  __d(14) "\n\t"
		     "fst.d     %0, (8*8),"  __d(16) "\n\t"
		     "fst.d     %0, (9*8),"  __d(18) "\n\t"
		     "fst.d     %0, (10*8)," __d(20) "\n\t"
		     "fst.d     %0, (11*8)," __d(22) "\n\t"
		     "fst.d     %0, (12*8)," __d(24) "\n\t"
		     "fst.d     %0, (13*8)," __d(26) "\n\t"
		     "fst.d     %0, (14*8)," __d(28) "\n\t"
		     "fst.d     %0, (15*8)," __d(30) "\n\t"
		     "fst.d     %0, (16*8)," __d(32) "\n\t"
		     "fst.d     %0, (17*8)," __d(34) "\n\t"
		     "fst.d     %0, (18*8)," __d(36) "\n\t"
		     "fst.d     %0, (19*8)," __d(38) "\n\t"
		     "fst.d     %0, (20*8)," __d(40) "\n\t"
		     "fst.d     %0, (21*8)," __d(42) "\n\t"
		     "fst.d     %0, (22*8)," __d(44) "\n\t"
		     "fst.d     %0, (23*8)," __d(46) "\n\t"
		     "fst.d     %0, (24*8)," __d(48) "\n\t"
		     "fst.d     %0, (25*8)," __d(50) "\n\t"
		     "fst.d     %0, (26*8)," __d(52) "\n\t"
		     "fst.d     %0, (27*8)," __d(54) "\n\t"
		     "fst.d     %0, (28*8)," __d(56) "\n\t"
		     "fst.d     %0, (29*8)," __d(58) "\n\t"
		     "fst.d     %0, (30*8)," __d(60) "\n\t"
		     "fst.d     %0, (31*8)," __d(62) "\n\t"

		     "_fgetscr  " __f(63) 	     "\n\t"
		     "fst.s     %0, (32*8)," __f(63) "\n\t"
		: /* no output */
		: "r" (fpregs)
		: "memory");
}


static inline void
fpload(struct sh_fpu_hard_struct *fpregs)
{
	asm volatile("fld.d     %0, (0*8),"  __d(0)  "\n\t"
		     "fld.d     %0, (1*8),"  __d(2)  "\n\t"
		     "fld.d     %0, (2*8),"  __d(4)  "\n\t"
		     "fld.d     %0, (3*8),"  __d(6)  "\n\t"
		     "fld.d     %0, (4*8),"  __d(8)  "\n\t"
		     "fld.d     %0, (5*8),"  __d(10) "\n\t"
		     "fld.d     %0, (6*8),"  __d(12) "\n\t"
		     "fld.d     %0, (7*8),"  __d(14) "\n\t"
		     "fld.d     %0, (8*8),"  __d(16) "\n\t"
		     "fld.d     %0, (9*8),"  __d(18) "\n\t"
		     "fld.d     %0, (10*8)," __d(20) "\n\t"
		     "fld.d     %0, (11*8)," __d(22) "\n\t"
		     "fld.d     %0, (12*8)," __d(24) "\n\t"
		     "fld.d     %0, (13*8)," __d(26) "\n\t"
		     "fld.d     %0, (14*8)," __d(28) "\n\t"
		     "fld.d     %0, (15*8)," __d(30) "\n\t"
		     "fld.d     %0, (16*8)," __d(32) "\n\t"
		     "fld.d     %0, (17*8)," __d(34) "\n\t"
		     "fld.d     %0, (18*8)," __d(36) "\n\t"
		     "fld.d     %0, (19*8)," __d(38) "\n\t"
		     "fld.d     %0, (20*8)," __d(40) "\n\t"
		     "fld.d     %0, (21*8)," __d(42) "\n\t"
		     "fld.d     %0, (22*8)," __d(44) "\n\t"
		     "fld.d     %0, (23*8)," __d(46) "\n\t"
		     "fld.d     %0, (24*8)," __d(48) "\n\t"
		     "fld.d     %0, (25*8)," __d(50) "\n\t"
		     "fld.d     %0, (26*8)," __d(52) "\n\t"
		     "fld.d     %0, (27*8)," __d(54) "\n\t"
		     "fld.d     %0, (28*8)," __d(56) "\n\t"
		     "fld.d     %0, (29*8)," __d(58) "\n\t"
		     "fld.d     %0, (30*8)," __d(60) "\n\t"

		     "fld.s     %0, (32*8)," __f(63) "\n\t"
		     "_fputscr  " __f(63) 	     "\n\t"

	     	     "fld.d     %0, (31*8)," __d(62) "\n\t"
		: /* no output */
		: "r" (fpregs) );
}

void fpinit(struct sh_fpu_hard_struct *fpregs)
{
	*fpregs = init_fpuregs;
}

asmlinkage void
do_fpu_error(unsigned long ex, struct pt_regs *regs)
{
	struct task_struct *tsk = current;

	regs->pc += 4;

	tsk->thread.trap_no = 11;
	tsk->thread.error_code = 0;
	force_sig(SIGFPE, tsk);
}


asmlinkage void
do_fpu_state_restore(unsigned long ex, struct pt_regs *regs)
{
	void die(const char * str, struct pt_regs * regs, long err);

#if 0
printk("do_fpu_state_restore (pid %d, used_math %d, last_used_math pid %d)\n",
       current->pid, current->used_math,
       last_task_used_math ? last_task_used_math->pid : -1);
#endif

	if (! user_mode(regs))
		die("FPU used in kernel", regs, ex);

	regs->sr &= ~SR_FD;

	if (last_task_used_math == current)
		return;

	grab_fpu();
	if (last_task_used_math != NULL) {
		/* Other processes fpu state, save away */
		fpsave(&last_task_used_math->thread.fpu.hard);
        }
        last_task_used_math = current;
        if (current->used_math) {
                fpload(&current->thread.fpu.hard);
        } else {
		/* First time FPU user.  */
		fpload(&init_fpuregs);
                current->used_math = 1;
        }
	release_fpu();
}

