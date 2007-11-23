/*  $Id: irq.c,v 1.66 1997/04/14 05:38:21 davem Exp $
 *  arch/sparc/kernel/irq.c:  Interrupt request handling routines. On the
 *                            Sparc the IRQ's are basically 'cast in stone'
 *                            and you are supposed to probe the prom's device
 *                            node trees to find out who's got which IRQ.
 *
 *  Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 *  Copyright (C) 1995 Miguel de Icaza (miguel@nuclecu.unam.mx)
 *  Copyright (C) 1995 Pete A. Zaitcev (zaitcev@ipmce.su)
 *  Copyright (C) 1996 Dave Redman (djhr@tadpole.co.uk)
 */

#include <linux/config.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/malloc.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>

#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/psr.h>
#include <asm/smp.h>
#include <asm/vaddrs.h>
#include <asm/timer.h>
#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/traps.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/spinlock.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>

/*
 * Dave Redman (djhr@tadpole.co.uk)
 *
 * IRQ numbers.. These are no longer restricted to 15..
 *
 * this is done to enable SBUS cards and onboard IO to be masked
 * correctly. using the interrupt level isn't good enough.
 *
 * For example:
 *   A device interrupting at sbus level6 and the Floppy both come in
 *   at IRQ11, but enabling and disabling them requires writing to
 *   different bits in the SLAVIO/SEC.
 *
 * As a result of these changes sun4m machines could now support
 * directed CPU interrupts using the existing enable/disable irq code
 * with tweaks.
 *
 */

static void irq_panic(void)
{
    extern char *cputypval;
    prom_printf("machine: %s doesn't have irq handlers defined!\n",cputypval);
    prom_halt();
}

void (*enable_irq)(unsigned int) = (void (*)(unsigned int)) irq_panic;
void (*disable_irq)(unsigned int) = (void (*)(unsigned int)) irq_panic;
void (*enable_pil_irq)(unsigned int) = (void (*)(unsigned int)) irq_panic;
void (*disable_pil_irq)(unsigned int) = (void (*)(unsigned int)) irq_panic;
void (*clear_clock_irq)( void ) = irq_panic;
void (*clear_profile_irq)( void ) = irq_panic;
void (*load_profile_irq)( unsigned int ) =  (void (*)(unsigned int)) irq_panic;
void (*init_timers)( void (*)(int, void *,struct pt_regs *)) =
    (void (*)( void (*)(int, void *,struct pt_regs *))) irq_panic;

#ifdef __SMP__
void (*set_cpu_int)(int, int);
void (*clear_cpu_int)(int, int);
void (*set_irq_udt)(int);
#endif

/*
 * Dave Redman (djhr@tadpole.co.uk)
 *
 * There used to be extern calls and hard coded values here.. very sucky!
 * instead, because some of the devices attach very early, I do something
 * equally sucky but at least we'll never try to free statically allocated
 * space or call kmalloc before kmalloc_init :(.
 * 
 * In fact it's the timer10 that attaches first.. then timer14
 * then kmalloc_init is called.. then the tty interrupts attach.
 * hmmm....
 *
 */
#define MAX_STATIC_ALLOC	4
static struct irqaction static_irqaction[MAX_STATIC_ALLOC];
static int static_irq_count = 0;

static struct irqaction *irq_action[NR_IRQS+1] = {
	  NULL, NULL, NULL, NULL, NULL, NULL , NULL, NULL,
	  NULL, NULL, NULL, NULL, NULL, NULL , NULL, NULL
};

int get_irq_list(char *buf)
{
	int i, len = 0;
	struct irqaction * action;

	for (i = 0 ; i < (NR_IRQS+1) ; i++) {
	        action = *(i + irq_action);
		if (!action) 
		        continue;
		len += sprintf(buf+len, "%2d: %8d %c %s",
			i, kstat.interrupts[i],
			(action->flags & SA_INTERRUPT) ? '+' : ' ',
			action->name);
		for (action=action->next; action; action = action->next) {
			len += sprintf(buf+len, ",%s %s",
				(action->flags & SA_INTERRUPT) ? " +" : "",
				action->name);
		}
		len += sprintf(buf+len, "\n");
	}
	return len;
}

void free_irq(unsigned int irq, void *dev_id)
{
	struct irqaction * action;
	struct irqaction * tmp = NULL;
        unsigned long flags;
	unsigned int cpu_irq;
	
	cpu_irq = irq & NR_IRQS;
	action = *(cpu_irq + irq_action);
        if (cpu_irq > 14) {  /* 14 irq levels on the sparc */
                printk("Trying to free bogus IRQ %d\n", irq);
                return;
        }
	if (!action->handler) {
		printk("Trying to free free IRQ%d\n",irq);
		return;
	}
	if (dev_id) {
		for (; action; action = action->next) {
			if (action->dev_id == dev_id)
				break;
			tmp = action;
		}
		if (!action) {
			printk("Trying to free free shared IRQ%d\n",irq);
			return;
		}
	} else if (action->flags & SA_SHIRQ) {
		printk("Trying to free shared IRQ%d with NULL device ID\n", irq);
		return;
	}
	if (action->flags & SA_STATIC_ALLOC)
	{
	    /* This interrupt is marked as specially allocated
	     * so it is a bad idea to free it.
	     */
	    printk("Attempt to free statically allocated IRQ%d (%s)\n",
		   irq, action->name);
	    return;
	}
	
        save_and_cli(flags);
	if (action && tmp)
		tmp->next = action->next;
	else
		*(cpu_irq + irq_action) = action->next;

	kfree_s(action, sizeof(struct irqaction));

	if (!(*(cpu_irq + irq_action)))
		disable_irq(irq);

        restore_flags(flags);
}

/* Per-processor IRQ locking depth, both SMP and non-SMP code use this. */
unsigned int local_irq_count[NR_CPUS];
atomic_t __sparc_bh_counter;

#ifdef __SMP__
/* SMP interrupt locking on Sparc. */

/* Who has global_irq_lock. */
unsigned char global_irq_holder = NO_PROC_ID;

/* This protects IRQ's. */
spinlock_t global_irq_lock = SPIN_LOCK_UNLOCKED;

/* This protects BH software state (masks, things like that). */
spinlock_t global_bh_lock = SPIN_LOCK_UNLOCKED;

/* Global IRQ locking depth. */
atomic_t global_irq_count = ATOMIC_INIT;

#define irq_active(cpu) \
	(atomic_read(&global_irq_count) != local_irq_count[cpu])

static unsigned long previous_irqholder;

#define INIT_STUCK 10000000

#define STUCK \
if (!--stuck) { \
	printk("wait_on_irq CPU#%d stuck at %08lx, waiting for [%08lx:%x] " \
	       "(local=[%d(%x:%x:%x:%x)], global=[%d:%x]) ", \
	       cpu, where, previous_irqholder, global_irq_holder, \
	       local_count, local_irq_count[0], local_irq_count[1], \
	       local_irq_count[2], local_irq_count[3], \
	       atomic_read(&global_irq_count), global_irq_lock); \
	printk("g[%d:%x]\n", atomic_read(&global_irq_count), global_irq_lock); \
	stuck = INIT_STUCK; \
}

static inline void wait_on_irq(int cpu, unsigned long where)
{
	int stuck = INIT_STUCK;
	int local_count = local_irq_count[cpu];

	while(local_count != atomic_read(&global_irq_count)) {
		atomic_sub(local_count, &global_irq_count);
		spin_unlock(&global_irq_lock);
		while(1) {
			STUCK;
			if(atomic_read(&global_irq_count))
				continue;
			if(global_irq_lock)
				continue;
			if(spin_trylock(&global_irq_lock))
				break;
		}
		atomic_add(local_count, &global_irq_count);
	}
}

/* There has to be a better way. */
void synchronize_irq(void)
{
	int cpu = smp_processor_id();
	int local_count = local_irq_count[cpu];

	if(local_count != atomic_read(&global_irq_count)) {
		unsigned long flags;

		/* See comment below at __global_save_flags to understand
		 * why we must do it this way on Sparc.
		 */
		save_and_cli(flags);
		restore_flags(flags);
	}
}

#undef INIT_STUCK
#define INIT_STUCK 10000000

#undef STUCK
#define STUCK \
if (!--stuck) {printk("get_irqlock stuck at %08lx, waiting for %08lx\n", where, previous_irqholder); stuck = INIT_STUCK;}

static inline void get_irqlock(int cpu, unsigned long where)
{
	int stuck = INIT_STUCK;

	if(!spin_trylock(&global_irq_lock)) {
		if((unsigned char) cpu == global_irq_holder)
			return;
		do {
			do {
				STUCK;
				barrier();
			} while(global_irq_lock);
		} while(!spin_trylock(&global_irq_lock));
	}
	wait_on_irq(cpu, where);
	global_irq_holder = cpu;
	previous_irqholder = where;
}

void __global_cli(void)
{
	int cpu = smp_processor_id();
	unsigned long where;

	__asm__ __volatile__("mov %%i7, %0\n\t" : "=r" (where));
	__cli();
	get_irqlock(cpu, where);
}

void __global_sti(void)
{
	release_irqlock(smp_processor_id());
	__sti();
}

/* Yes, I know this is broken, but for the time being...
 *
 * On Sparc we must differentiate between real local processor
 * interrupts being disabled and global interrupt locking, this
 * is so that interrupt handlers which call this stuff don't get
 * interrupts turned back on when restore_flags() runs because
 * our current drivers will be very surprised about this, yes I
 * know they need to be fixed... -DaveM
 */
unsigned long __global_save_flags(void)
{
	unsigned long flags, retval = 0;

	__save_flags(flags);
	if(global_irq_holder == (unsigned char) smp_processor_id())
		retval |= 1;
	if(flags & PSR_PIL)
		retval |= 2;
	return retval;
}

void __global_restore_flags(unsigned long flags)
{
	if(flags & 1) {
		__global_cli();
	} else {
		release_irqlock(smp_processor_id());

		if(flags & 2)
			__cli();
		else
			__sti();
	}
}

#undef INIT_STUCK
#define INIT_STUCK 200000000

#undef STUCK
#define STUCK \
if (!--stuck) { \
	printk("irq_enter stuck (irq=%d, cpu=%d, global=%d)\n", \
	       irq, cpu, global_irq_holder); \
	stuck = INIT_STUCK; \
}

static void irq_enter(int cpu, int irq)
{
	extern void smp_irq_rotate(int cpu);
	int stuck = INIT_STUCK;

	smp_irq_rotate(cpu);
	hardirq_enter(cpu);
	while(global_irq_lock) {
		if((unsigned char) cpu == global_irq_holder) {
			printk("YEEEE Local interrupts enabled, global disabled\n");
			break;
		}
		STUCK;
		barrier();
	}
}

static void irq_exit(int cpu, int irq)
{
	__cli();
	hardirq_exit(cpu);
	release_irqlock(cpu);
}

#else /* !__SMP__ */

#define irq_enter(cpu, irq)	(local_irq_count[cpu]++)
#define irq_exit(cpu, irq)	(local_irq_count[cpu]--)

#endif /* __SMP__ */

void unexpected_irq(int irq, void *dev_id, struct pt_regs * regs)
{
        int i;
	struct irqaction * action;
	unsigned int cpu_irq;
	
	cpu_irq = irq & NR_IRQS;
	action = *(cpu_irq + irq_action);

        printk("IO device interrupt, irq = %d\n", irq);
        printk("PC = %08lx NPC = %08lx FP=%08lx\n", regs->pc, 
		    regs->npc, regs->u_regs[14]);
	if (action) {
		printk("Expecting: ");
        	for (i = 0; i < 16; i++)
                	if (action->handler)
                        	prom_printf("[%s:%d:0x%x] ", action->name,
				    (int) i, (unsigned int) action->handler);
	}
        printk("AIEEE\n");
	panic("bogus interrupt received");
}

void handler_irq(int irq, struct pt_regs * regs)
{
	struct irqaction * action;
	unsigned int cpu_irq = irq & NR_IRQS;
	int cpu = smp_processor_id();
	
	disable_pil_irq(cpu_irq);
	irq_enter(cpu, irq);
	action = *(cpu_irq + irq_action);
	kstat.interrupts[cpu_irq]++;
	do {
		if (!action || !action->handler)
			unexpected_irq(irq, 0, regs);
		action->handler(irq, action->dev_id, regs);
		action = action->next;
	} while (action);
	irq_exit(cpu, irq);
	enable_pil_irq(cpu_irq);
}

#ifdef CONFIG_BLK_DEV_FD
extern void floppy_interrupt(int irq, void *dev_id, struct pt_regs *regs);

void sparc_floppy_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	disable_pil_irq(irq);
	irq_enter(cpu, irq);
	floppy_interrupt(irq, dev_id, regs);
	irq_exit(cpu, irq);
	disable_pil_irq(irq);
}
#endif

/* Fast IRQ's on the Sparc can only have one routine attached to them,
 * thus no sharing possible.
 */
int request_fast_irq(unsigned int irq,
		     void (*handler)(int, void *, struct pt_regs *),
		     unsigned long irqflags, const char *devname)
{
	struct irqaction *action;
	unsigned long flags;
	unsigned int cpu_irq;
#ifdef __SMP__
	struct tt_entry *trap_table;
	extern struct tt_entry trapbase_cpu1, trapbase_cpu2, trapbase_cpu3;
#endif
	
	cpu_irq = irq & NR_IRQS;
	if(cpu_irq > 14)
		return -EINVAL;
	if(!handler)
		return -EINVAL;
	action = *(cpu_irq + irq_action);
	if(action) {
		if(action->flags & SA_SHIRQ)
			panic("Trying to register fast irq when already shared.\n");
		if(irqflags & SA_SHIRQ)
			panic("Trying to register fast irq as shared.\n");

		/* Anyway, someone already owns it so cannot be made fast. */
		printk("request_fast_irq: Trying to register yet already owned.\n");
		return -EBUSY;
	}

	save_and_cli(flags);

	/* If this is flagged as statically allocated then we use our
	 * private struct which is never freed.
	 */
	if (irqflags & SA_STATIC_ALLOC)
	    if (static_irq_count < MAX_STATIC_ALLOC)
		action = &static_irqaction[static_irq_count++];
	    else
		printk("Fast IRQ%d (%s) SA_STATIC_ALLOC failed using kmalloc\n",
		       irq, devname);
	
	if (action == NULL)
	    action = (struct irqaction *)kmalloc(sizeof(struct irqaction),
						 GFP_KERNEL);
	
	if (!action) { 
		restore_flags(flags);
		return -ENOMEM;
	}

	/* Dork with trap table if we get this far. */
#define INSTANTIATE(table) \
	table[SP_TRAP_IRQ1+(cpu_irq-1)].inst_one = SPARC_RD_PSR_L0; \
	table[SP_TRAP_IRQ1+(cpu_irq-1)].inst_two = \
		SPARC_BRANCH((unsigned long) handler, \
			     (unsigned long) &table[SP_TRAP_IRQ1+(cpu_irq-1)].inst_two);\
	table[SP_TRAP_IRQ1+(cpu_irq-1)].inst_three = SPARC_RD_WIM_L3; \
	table[SP_TRAP_IRQ1+(cpu_irq-1)].inst_four = SPARC_NOP;

	INSTANTIATE(sparc_ttable)
#ifdef __SMP__
	trap_table = &trapbase_cpu1; INSTANTIATE(trap_table)
	trap_table = &trapbase_cpu2; INSTANTIATE(trap_table)
	trap_table = &trapbase_cpu3; INSTANTIATE(trap_table)
#endif
#undef INSTANTIATE
	/*
	 * XXX Correct thing whould be to flush only I- and D-cache lines
	 * which contain the handler in question. But as of time of the
	 * writing we have no CPU-neutral interface to fine-grained flushes.
	 */
	flush_cache_all();

	action->handler = handler;
	action->flags = irqflags;
	action->mask = 0;
	action->name = devname;
	action->dev_id = NULL;
	action->next = NULL;

	*(cpu_irq + irq_action) = action;

	enable_irq(irq);
	restore_flags(flags);
	return 0;
}

int request_irq(unsigned int irq,
		void (*handler)(int, void *, struct pt_regs *),
		unsigned long irqflags, const char * devname, void *dev_id)
{
	struct irqaction * action, *tmp = NULL;
	unsigned long flags;
	unsigned int cpu_irq;
	
	cpu_irq = irq & NR_IRQS;
	if(cpu_irq > 14)
		return -EINVAL;

	if (!handler)
	    return -EINVAL;
	action = *(cpu_irq + irq_action);
	if (action) {
		if ((action->flags & SA_SHIRQ) && (irqflags & SA_SHIRQ)) {
			for (tmp = action; tmp->next; tmp = tmp->next);
		} else {
			return -EBUSY;
		}
		if ((action->flags & SA_INTERRUPT) ^ (irqflags & SA_INTERRUPT)) {
			printk("Attempt to mix fast and slow interrupts on IRQ%d denied\n", irq);
			return -EBUSY;
		}   
		action = NULL;		/* Or else! */
	}

	save_and_cli(flags);

	/* If this is flagged as statically allocated then we use our
	 * private struct which is never freed.
	 */
	if (irqflags & SA_STATIC_ALLOC)
	    if (static_irq_count < MAX_STATIC_ALLOC)
		action = &static_irqaction[static_irq_count++];
	    else
		printk("Request for IRQ%d (%s) SA_STATIC_ALLOC failed using kmalloc\n",irq, devname);
	
	if (action == NULL)
	    action = (struct irqaction *)kmalloc(sizeof(struct irqaction),
						 GFP_KERNEL);
	
	if (!action) { 
		restore_flags(flags);
		return -ENOMEM;
	}

	action->handler = handler;
	action->flags = irqflags;
	action->mask = 0;
	action->name = devname;
	action->next = NULL;
	action->dev_id = dev_id;

	if (tmp)
		tmp->next = action;
	else
		*(cpu_irq + irq_action) = action;

	enable_irq(irq);
	restore_flags(flags);
	return 0;
}

/* We really don't need these at all on the Sparc.  We only have
 * stubs here because they are exported to modules.
 */
unsigned long probe_irq_on(void)
{
  return 0;
}

int probe_irq_off(unsigned long mask)
{
  return 0;
}

/* djhr
 * This could probably be made indirect too and assigned in the CPU
 * bits of the code. That would be much nicer I think and would also
 * fit in with the idea of being able to tune your kernel for your machine
 * by removing unrequired machine and device support.
 *
 */

__initfunc(void init_IRQ(void))
{
	extern void sun4c_init_IRQ( void );
	extern void sun4m_init_IRQ( void );
    
	switch(sparc_cpu_model) {
	case sun4c:
		sun4c_init_IRQ();
		break;

	case sun4m:
		sun4m_init_IRQ();
		break;

	case ap1000:
#if CONFIG_AP1000
		ap_init_IRQ();;
		break;
#endif

	default:
		prom_printf("Cannot initialize IRQ's on this Sun machine...");
		break;
	}
}
