/*
 * ip22-int.c: Routines for generic manipulation of the INT[23] ASIC
 *             found on INDY and Indigo2 workstations.
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 * Copyright (C) 1997, 1998 Ralf Baechle (ralf@gnu.org)
 * Copyright (C) 1999 Andrew R. Baker (andrewb@uab.edu)
 *                    - Indigo2 changes
 *                    - Interrupt handling fixes
 * Copyright (C) 2001 Ladislav Michl (ladis@psi.cz)
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/mipsregs.h>
#include <asm/addrspace.h>

#include <asm/sgi/sgint23.h>
#include <asm/sgi/sgihpc.h>

/* #define DEBUG_SGINT */
#undef I_REALLY_NEED_THIS_IRQ

struct sgi_int2_regs *sgi_i2regs;
struct sgi_int3_regs *sgi_i3regs;
struct sgi_ioc_ints *ioc_icontrol;
struct sgi_ioc_timers *ioc_timers;
volatile unsigned char *ioc_tclear;

static char lc0msk_to_irqnr[256];
static char lc1msk_to_irqnr[256];
static char lc2msk_to_irqnr[256];
static char lc3msk_to_irqnr[256];

extern asmlinkage void indyIRQ(void);
extern void do_IRQ(int irq, struct pt_regs *regs);
extern int ip22_eisa_init (void);

static void enable_local0_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	/* don't allow mappable interrupt to be enabled from setup_irq,
	 * we have our own way to do so */
	if (irq != SGI_MAP_0_IRQ)
		ioc_icontrol->imask0 |= (1 << (irq - SGINT_LOCAL0));
	restore_flags(flags);
}

static unsigned int startup_local0_irq(unsigned int irq)
{
	enable_local0_irq(irq);
	return 0;		/* Never anything pending  */
}

static void disable_local0_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	ioc_icontrol->imask0 &= ~(1 << (irq - SGINT_LOCAL0));
	restore_flags(flags);
}

#define shutdown_local0_irq	disable_local0_irq
#define mask_and_ack_local0_irq	disable_local0_irq

static void end_local0_irq (unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_local0_irq(irq);
}

static struct hw_interrupt_type ip22_local0_irq_type = {
	"IP22 local 0",
	startup_local0_irq,
	shutdown_local0_irq,
	enable_local0_irq,
	disable_local0_irq,
	mask_and_ack_local0_irq,
	end_local0_irq,
	NULL
};

static void enable_local1_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	/* don't allow mappable interrupt to be enabled from setup_irq,
	 * we have our own way to do so */
	if (irq != SGI_MAP_1_IRQ)
		ioc_icontrol->imask1 |= (1 << (irq - SGINT_LOCAL1));
	restore_flags(flags);
}

static unsigned int startup_local1_irq(unsigned int irq)
{
	enable_local1_irq(irq);
	return 0;		/* Never anything pending  */
}

void disable_local1_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	ioc_icontrol->imask1 &= ~(1 << (irq - SGINT_LOCAL1));
	restore_flags(flags);
}

#define shutdown_local1_irq	disable_local1_irq
#define mask_and_ack_local1_irq	disable_local1_irq

static void end_local1_irq (unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_local1_irq(irq);
}

static struct hw_interrupt_type ip22_local1_irq_type = {
	"IP22 local 1",
	startup_local1_irq,
	shutdown_local1_irq,
	enable_local1_irq,
	disable_local1_irq,
	mask_and_ack_local1_irq,
	end_local1_irq,
	NULL
};

static void enable_local2_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	ioc_icontrol->imask0 |= (1 << (SGI_MAP_0_IRQ - SGINT_LOCAL0));
	ioc_icontrol->cmeimask0 |= (1 << (irq - SGINT_LOCAL2));
	restore_flags(flags);
}

static unsigned int startup_local2_irq(unsigned int irq)
{
	enable_local2_irq(irq);
	return 0;		/* Never anything pending  */
}

void disable_local2_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	ioc_icontrol->cmeimask0 &= ~(1 << (irq - SGINT_LOCAL2));
	if (!ioc_icontrol->cmeimask0)
		ioc_icontrol->imask0 &= ~(1 << (SGI_MAP_0_IRQ - SGINT_LOCAL0));
	restore_flags(flags);
}

#define shutdown_local2_irq disable_local2_irq
#define mask_and_ack_local2_irq	disable_local2_irq

static void end_local2_irq (unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_local2_irq(irq);
}

static struct hw_interrupt_type ip22_local2_irq_type = {
	"IP22 local 2",
	startup_local2_irq,
	shutdown_local2_irq,
	enable_local2_irq,
	disable_local2_irq,
	mask_and_ack_local2_irq,
	end_local2_irq,
	NULL
};

static void enable_local3_irq(unsigned int irq)
{
#ifdef I_REALLY_NEED_THIS_IRQ
	unsigned long flags;

	save_and_cli(flags);
	ioc_icontrol->imask1 |= (1 << (SGI_MAP_1_IRQ - SGINT_LOCAL1));
	ioc_icontrol->cmeimask1 |= (1 << (irq - SGINT_LOCAL3));
	restore_flags(flags);
#else
	panic("Who need local 3 irq? see ip22-int.c");
#endif
}

static unsigned int startup_local3_irq(unsigned int irq)
{
	enable_local3_irq(irq);
	return 0;		/* Never anything pending  */
}

void disable_local3_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	ioc_icontrol->cmeimask1 &= ~(1 << (irq - SGINT_LOCAL3));
	if (!ioc_icontrol->cmeimask1)
		ioc_icontrol->imask1 &= ~(1 << (SGI_MAP_1_IRQ - SGINT_LOCAL1));
	restore_flags(flags);
}

#define shutdown_local3_irq disable_local3_irq
#define mask_and_ack_local3_irq	disable_local3_irq

static void end_local3_irq (unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_local3_irq(irq);
}

static struct hw_interrupt_type ip22_local3_irq_type = {
	"IP22 local 3",
	startup_local3_irq,
	shutdown_local3_irq,
	enable_local3_irq,
	disable_local3_irq,
	mask_and_ack_local3_irq,
	end_local3_irq,
	NULL
};

void indy_local0_irqdispatch(struct pt_regs *regs)
{
	unsigned char mask = ioc_icontrol->istat0;
	unsigned char mask2 = 0;
	int irq;

	mask &= ioc_icontrol->imask0;
	if (mask & ISTAT0_LIO2) {
		mask2 = ioc_icontrol->vmeistat;
		mask2 &= ioc_icontrol->cmeimask0;
		irq = lc2msk_to_irqnr[mask2];
	} else {
		irq = lc0msk_to_irqnr[mask];
	}

	/* if irq == 0, then the interrupt has already been cleared */
	if (irq)
		do_IRQ(irq, regs);
	return;
}

void indy_local1_irqdispatch(struct pt_regs *regs)
{
	unsigned char mask = ioc_icontrol->istat1;
	unsigned char mask2 = 0;
	int irq;

	mask &= ioc_icontrol->imask1;
	if (mask & ISTAT1_LIO3) {
#ifndef I_REALLY_NEED_THIS_IRQ
		printk("Whee: Got an LIO3 irq, winging it...\n");
#endif
		mask2 = ioc_icontrol->vmeistat;
		mask2 &= ioc_icontrol->cmeimask1;
		irq = lc3msk_to_irqnr[mask2];
	} else {
		irq = lc1msk_to_irqnr[mask];
	}

	/* if irq == 0, then the interrupt has already been cleared */
	if (irq)
		do_IRQ(irq, regs);
	return;
}

extern void be_ip22_interrupt(int irq, struct pt_regs *regs);

void indy_buserror_irq(struct pt_regs *regs)
{
	int cpu = smp_processor_id();
	int irq = SGI_BUSERR_IRQ;

	irq_enter(cpu, irq);
	kstat.irqs[cpu][irq]++;
	be_ip22_interrupt(irq, regs);
	irq_exit(cpu, irq);
}

static struct irqaction local0_cascade =
	{ no_action, SA_INTERRUPT, 0, "local0 cascade", NULL, NULL };
static struct irqaction local1_cascade =
	{ no_action, SA_INTERRUPT, 0, "local1 cascade", NULL, NULL };
static struct irqaction buserr =
	{ no_action, SA_INTERRUPT, 0, "Bus Error", NULL, NULL };
static struct irqaction map0_cascade =
	{ no_action, SA_INTERRUPT, 0, "mappable0 cascade", NULL, NULL };
#ifdef I_REALLY_NEED_THIS_IRQ
static struct irqaction map1_cascade =
	{ no_action, SA_INTERRUPT, 0, "mappable1 cascade", NULL, NULL };
#endif

extern void mips_cpu_irq_init(unsigned int irq_base);

void __init init_IRQ(void)
{
	int i;

	sgi_i2regs = (struct sgi_int2_regs *) (KSEG1 + SGI_INT2_BASE);
	sgi_i3regs = (struct sgi_int3_regs *) (KSEG1 + SGI_INT3_BASE);

	/* Init local mask --> irq tables. */
	for (i = 0; i < 256; i++) {
		if (i & 0x80) {
			lc0msk_to_irqnr[i] = SGINT_LOCAL0 + 7;
			lc1msk_to_irqnr[i] = SGINT_LOCAL1 + 7;
			lc2msk_to_irqnr[i] = SGINT_LOCAL2 + 7;
			lc3msk_to_irqnr[i] = SGINT_LOCAL3 + 7;
		} else if (i & 0x40) {
			lc0msk_to_irqnr[i] = SGINT_LOCAL0 + 6;
			lc1msk_to_irqnr[i] = SGINT_LOCAL1 + 6;
			lc2msk_to_irqnr[i] = SGINT_LOCAL2 + 6;
			lc3msk_to_irqnr[i] = SGINT_LOCAL3 + 6;
		} else if (i & 0x20) {
			lc0msk_to_irqnr[i] = SGINT_LOCAL0 + 5;
			lc1msk_to_irqnr[i] = SGINT_LOCAL1 + 5;
			lc2msk_to_irqnr[i] = SGINT_LOCAL2 + 5;
			lc3msk_to_irqnr[i] = SGINT_LOCAL3 + 5;
		} else if (i & 0x10) {
			lc0msk_to_irqnr[i] = SGINT_LOCAL0 + 4;
			lc1msk_to_irqnr[i] = SGINT_LOCAL1 + 4;
			lc2msk_to_irqnr[i] = SGINT_LOCAL2 + 4;
			lc3msk_to_irqnr[i] = SGINT_LOCAL3 + 4;
		} else if (i & 0x08) {
			lc0msk_to_irqnr[i] = SGINT_LOCAL0 + 3;
			lc1msk_to_irqnr[i] = SGINT_LOCAL1 + 3;
			lc2msk_to_irqnr[i] = SGINT_LOCAL2 + 3;
			lc3msk_to_irqnr[i] = SGINT_LOCAL3 + 3;
		} else if (i & 0x04) {
			lc0msk_to_irqnr[i] = SGINT_LOCAL0 + 2;
			lc1msk_to_irqnr[i] = SGINT_LOCAL1 + 2;
			lc2msk_to_irqnr[i] = SGINT_LOCAL2 + 2;
			lc3msk_to_irqnr[i] = SGINT_LOCAL3 + 2;
		} else if (i & 0x02) {
			lc0msk_to_irqnr[i] = SGINT_LOCAL0 + 1;
			lc1msk_to_irqnr[i] = SGINT_LOCAL1 + 1;
			lc2msk_to_irqnr[i] = SGINT_LOCAL2 + 1;
			lc3msk_to_irqnr[i] = SGINT_LOCAL3 + 1;
		} else if (i & 0x01) {
			lc0msk_to_irqnr[i] = SGINT_LOCAL0 + 0;
			lc1msk_to_irqnr[i] = SGINT_LOCAL1 + 0;
			lc2msk_to_irqnr[i] = SGINT_LOCAL2 + 0;
			lc3msk_to_irqnr[i] = SGINT_LOCAL3 + 0;
		} else {
			lc0msk_to_irqnr[i] = 0;
			lc1msk_to_irqnr[i] = 0;
			lc2msk_to_irqnr[i] = 0;
			lc3msk_to_irqnr[i] = 0;
		}
	}

	/* Indy uses an INT3, Indigo2 uses an INT2 */
	if (sgi_guiness) {
		ioc_icontrol = &sgi_i3regs->ints;
		ioc_timers = &sgi_i3regs->timers;
		ioc_tclear = &sgi_i3regs->tclear;
	} else {
		ioc_icontrol = &sgi_i2regs->ints;
		ioc_timers = &sgi_i2regs->timers;
		ioc_tclear = &sgi_i2regs->tclear;
	}

	/* Mask out all interrupts. */
	ioc_icontrol->imask0 = 0;
	ioc_icontrol->imask1 = 0;
	ioc_icontrol->cmeimask0 = 0;
	ioc_icontrol->cmeimask1 = 0;

	set_except_vector(0, indyIRQ);

	init_generic_irq();
	/* init CPU irqs */
	mips_cpu_irq_init(SGINT_CPU);

	for (i = SGINT_LOCAL0; i < SGINT_END; i++) {
		hw_irq_controller *handler;

		if (i < SGINT_LOCAL1)
			handler		= &ip22_local0_irq_type;
		else if (i < SGINT_LOCAL2)
			handler		= &ip22_local1_irq_type;
		else if (i < SGINT_LOCAL3)
			handler		= &ip22_local2_irq_type;
		else
			handler		= &ip22_local3_irq_type;

		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= 0;
		irq_desc[i].depth	= 1;
		irq_desc[i].handler	= handler;
	}

	/* vector handler. this register the IRQ as non-sharable */
	setup_irq(SGI_LOCAL_0_IRQ, &local0_cascade);
	setup_irq(SGI_LOCAL_1_IRQ, &local1_cascade);
	setup_irq(SGI_BUSERR_IRQ, &buserr);

	/* cascade in cascade. i love Indy ;-) */
	setup_irq(SGI_MAP_0_IRQ, &map0_cascade);
#ifdef I_REALLY_NEED_THIS_IRQ
	setup_irq(SGI_MAP_1_IRQ, &map1_cascade);
#endif
#ifdef CONFIG_IP22_EISA
	if (!sgi_guiness)	/* Only Indigo-2 have EISA stuff */
	        ip22_eisa_init ();
#endif
}
