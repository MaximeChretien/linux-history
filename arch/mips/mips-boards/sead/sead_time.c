/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2002 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * Setting up the clock on the MIPS boards.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <asm/mipsregs.h>
#include <asm/ptrace.h>
#include <asm/hardirq.h>
#include <asm/cpu.h>

#include <linux/interrupt.h>
#include <linux/timex.h>

#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/prom.h>

extern volatile unsigned long wall_jiffies;

static unsigned long r4k_offset; /* Amount to increment compare reg each time */
static unsigned long r4k_cur;    /* What counter should be at next timer irq */
extern rwlock_t xtime_lock;

#define ALLINTS (IE_IRQ0 | IE_IRQ1 | IE_IRQ5)

static char display_string[] = "        LINUX ON SEAD       ";

static unsigned int display_count = 0;
#define MAX_DISPLAY_COUNT (sizeof(display_string) - 8)

#define MIPS_CPU_TIMER_IRQ 7

static unsigned int timer_tick_count=0;

static inline void ack_r4ktimer(unsigned long newval)
{
	write_32bit_cp0_register(CP0_COMPARE, newval);
}

/*
 * There are a lot of conceptually broken versions of the MIPS timer interrupt
 * handler floating around.  This one is rather different, but the algorithm
 * is provably more robust.
 */
void mips_timer_interrupt(struct pt_regs *regs)
{
	int cpu = smp_processor_id();
	int irq = MIPS_CPU_TIMER_IRQ;

	irq_enter(cpu, irq);

	do {
		kstat.irqs[cpu][irq]++;
		do_timer(regs);

		if ((timer_tick_count++ % HZ) == 0) {
		    mips_display_message(&display_string[display_count++]);
		    if (display_count == MAX_DISPLAY_COUNT)
		        display_count = 0;
		}

		r4k_cur += r4k_offset;
		ack_r4ktimer(r4k_cur);

	} while (((unsigned long)read_32bit_cp0_register(CP0_COUNT)
	         - r4k_cur) < 0x7fffffff);

	irq_exit(cpu, irq);
	if (softirq_pending(cpu))
		do_softirq();
}

/*
 * Figure out the r4k offset, the amount to increment the compare
 * register for each time tick.
 */
static unsigned long __init cal_r4koff(void)
{
	/*
	 * The SEAD board doesn't have a real time clock, so we can't
	 * really calculate the timer offset.
	 * For now we hardwire the SEAD board frequency to 12MHz.
	 */
	return(6000000/HZ);
}

void __init mips_time_init(void)
{
        unsigned int est_freq, flags;

	__save_and_cli(flags);

        /* Start r4k counter. */
        write_32bit_cp0_register(CP0_COUNT, 0);

	printk("calculating r4koff... ");
	r4k_offset = cal_r4koff();
	printk("%08lx(%d)\n", r4k_offset, (int) r4k_offset);

        if ((read_32bit_cp0_register(CP0_PRID) & 0xffff00) ==
	    (PRID_COMP_MIPS | PRID_IMP_20KC))
		est_freq = r4k_offset*HZ;
	else
		est_freq = 2*r4k_offset*HZ;

	est_freq += 5000;    /* round */
	est_freq -= est_freq%10000;
	printk("CPU frequency %d.%02d MHz\n", est_freq/1000000,
	       (est_freq%1000000)*100/1000000);

	__restore_flags(flags);
}

void __init mips_timer_setup(struct irqaction *irq)
{
	/* we are using the cpu counter for timer interrupts */
	irq->handler = no_action;     /* we use our own handler */
	setup_irq(MIPS_CPU_TIMER_IRQ, irq);

        /* to generate the first timer interrupt */
	r4k_cur = (read_32bit_cp0_register(CP0_COUNT) + r4k_offset);
	write_32bit_cp0_register(CP0_COMPARE, r4k_cur);
	set_cp0_status(ALLINTS);
}
