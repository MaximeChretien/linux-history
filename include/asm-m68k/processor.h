/*
 * include/asm-m68k/processor.h
 *
 * Copyright (C) 1995 Hamish Macdonald
 */

#ifndef __ASM_M68K_PROCESSOR_H
#define __ASM_M68K_PROCESSOR_H

#include <asm/segment.h>
#include <asm/fpu.h>

/*
 * User space process size: 3.75GB. This is hardcoded into a few places,
 * so don't change it unless you know what you are doing.
 */
#define TASK_SIZE	(0xF0000000UL)

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	0xC0000000UL

/*
 * Bus types
 */
#define EISA_bus 0
#define MCA_bus 0

/* 
 * if you change this structure, you must change the code and offsets
 * in m68k/machasm.S
 */
   
struct thread_struct {
	unsigned long  ksp;		/* kernel stack pointer */
	unsigned long  usp;		/* user stack pointer */
	unsigned short sr;		/* saved status register */
	unsigned short fs;		/* saved fs (sfc, dfc) */
	unsigned long  crp[2];		/* cpu root pointer */
	unsigned long  esp0;		/* points to SR of stack frame */
	unsigned long  fp[8*3];
	unsigned long  fpcntl[3];	/* fp control regs */
	unsigned char  fpstate[FPSTATESIZE];  /* floating point state */
};

#define INIT_MMAP { &init_mm, 0, 0x40000000, __pgprot(_PAGE_PRESENT|_PAGE_ACCESSED), VM_READ | VM_WRITE | VM_EXEC, NULL, &init_mm.mmap }

#define INIT_TSS  { \
	sizeof(init_stack) + (unsigned long) init_stack, 0, \
	PS_S, KERNEL_DS, \
	{0, 0}, 0, {0,}, {0, 0, 0}, {0,}, \
}

/*
 * Do necessary setup to start up a newly executed thread.
 */
static inline void start_thread(struct pt_regs * regs, unsigned long pc,
				unsigned long usp)
{
	unsigned long nilstate = 0;

	/* clear floating point state */
	__asm__ __volatile__ (".chip 68k/68881\n\t"
			      "frestore %0@\n\t"
			      ".chip 68k" : : "a" (&nilstate));

	/* reads from user space */
	set_fs(USER_DS);

	regs->pc = pc;
	regs->sr &= ~0x2000;
	wrusp(usp);
}

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);

/*
 * Return saved PC of a blocked thread.
 */
extern inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	extern int sys_pause(void);
	extern void schedule(void);
	struct switch_stack *sw = (struct switch_stack *)t->ksp;
	/* Check whether the thread is blocked in resume() */
	if (sw->retpc >= (unsigned long)schedule &&
	    sw->retpc < (unsigned long)sys_pause)
		return ((unsigned long *)sw->a6)[1];
	else
		return sw->retpc;
}

/* Allocation and freeing of basic task resources. */
#define alloc_task_struct() \
	((struct task_struct *) __get_free_pages(GFP_KERNEL,1,0))
#define free_task_struct(p)	free_pages((unsigned long)(p),1)

#define init_task	(init_task_union.task)
#define init_stack	(init_task_union.stack)

#endif
