/*
 * 
 * Copyright (C) 2001 MontaVista Software, ppopov@mvista.com
 * Copied and modified Carsten Langgaard's time.c
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
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

#include <linux/types.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <asm/mipsregs.h>
#include <asm/ptrace.h>
#include <asm/time.h>
#include <asm/hardirq.h>
#include <asm/div64.h>
#include <asm/au1000.h>

#include <linux/mc146818rtc.h>
#include <linux/timex.h>

extern void startup_match20_interrupt(void);

extern volatile unsigned long wall_jiffies;
unsigned long missed_heart_beats = 0;

static unsigned long r4k_offset; /* Amount to increment compare reg each time */
static unsigned long r4k_cur;    /* What counter should be at next timer irq */
extern rwlock_t xtime_lock;
unsigned int mips_counter_frequency = 0;

/* Cycle counter value at the previous timer interrupt.. */
static unsigned int timerhi = 0, timerlo = 0;

#ifdef CONFIG_PM
#define MATCH20_INC 328
extern void startup_match20_interrupt(void);
static unsigned long last_pc0, last_match20;
#endif

static inline void ack_r4ktimer(unsigned long newval)
{
	write_32bit_cp0_register(CP0_COMPARE, newval);
}

/*
 * There are a lot of conceptually broken versions of the MIPS timer interrupt
 * handler floating around.  This one is rather different, but the algorithm
 * is provably more robust.
 */
unsigned long wtimer;
void mips_timer_interrupt(struct pt_regs *regs)
{
	int irq = 63;
	unsigned long count;
	int cpu = smp_processor_id();

	irq_enter(cpu, irq);
	kstat.irqs[cpu][irq]++;

#ifdef CONFIG_PM
	printk(KERN_ERR "Unexpected CP0 interrupt\n");
	regs->cp0_status &= ~IE_IRQ5; /* disable CP0 interrupt */
	return;
#endif

	if (r4k_offset == 0)
		goto null;

	do {
		count = read_32bit_cp0_register(CP0_COUNT);
		timerhi += (count < timerlo);   /* Wrap around */
		timerlo = count;

		kstat.irqs[0][irq]++;
		do_timer(regs);
		r4k_cur += r4k_offset;
		ack_r4ktimer(r4k_cur);

	} while (((unsigned long)read_32bit_cp0_register(CP0_COUNT)
	         - r4k_cur) < 0x7fffffff);

	irq_exit(cpu, irq);

	if (softirq_pending(cpu))
		do_softirq();
	return;

null:
	ack_r4ktimer(0);
}

#ifdef CONFIG_PM
void counter0_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long pc0;
	int time_elapsed;
	static int jiffie_drift = 0;

	kstat.irqs[0][irq]++;
	if (readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_M20) {
		/* should never happen! */
		printk(KERN_WARNING "counter 0 w status eror\n");
		return;
	}

	pc0 = inl(SYS_TOYREAD);
	if (pc0 < last_match20) {
		/* counter overflowed */
		time_elapsed = (0xffffffff - last_match20) + pc0;
	}
	else {
		time_elapsed = pc0 - last_match20;
	}

	while (time_elapsed > 0) {
		do_timer(regs);
		time_elapsed -= MATCH20_INC;
		last_match20 += MATCH20_INC;
		jiffie_drift++;
	}

	last_pc0 = pc0;
	outl(last_match20 + MATCH20_INC, SYS_TOYMATCH2);
	au_sync();

	/* our counter ticks at 10.009765625 ms/tick, we we're running
	 * almost 10uS too slow per tick.
	 */
	 
	if (jiffie_drift >= 999) {
		jiffie_drift -= 999;
		do_timer(regs); /* increment jiffies by one */
	}
}
#endif

/* 
 * Figure out the r4k offset, the amount to increment the compare
 * register for each time tick. 
 * Use the Programmable Counter 1 to do this.
 */
unsigned long cal_r4koff(void)
{
	unsigned long count;
	unsigned long cpu_speed;
	unsigned long start, end;
	unsigned long counter;
	int trim_divide = 16;
	unsigned long flags;

	save_and_cli(flags);

	counter = inl(SYS_COUNTER_CNTRL);
	outl(counter | SYS_CNTRL_EN1, SYS_COUNTER_CNTRL);

	while (inl(SYS_COUNTER_CNTRL) & SYS_CNTRL_T1S);
	outl(trim_divide-1, SYS_RTCTRIM); /* RTC now ticks at 32.768/16 kHz */
	while (inl(SYS_COUNTER_CNTRL) & SYS_CNTRL_T1S);

	while (inl(SYS_COUNTER_CNTRL) & SYS_CNTRL_C1S);
	outl (0, SYS_TOYWRITE);
	while (inl(SYS_COUNTER_CNTRL) & SYS_CNTRL_C1S);

	start = inl(SYS_RTCREAD);
	start += 2;
	/* wait for the beginning of a new tick */
	while (inl(SYS_RTCREAD) < start);

	/* Start r4k counter. */
	write_32bit_cp0_register(CP0_COUNT, 0);
	end = start + (32768 / trim_divide)/2; /* wait 0.5 seconds */

	while (end > inl(SYS_RTCREAD));

	count = read_32bit_cp0_register(CP0_COUNT);
	cpu_speed = count * 2;
	mips_counter_frequency = count;
	set_au1000_uart_baud_base(((cpu_speed) / 4) / 16);
	restore_flags(flags);
	return (cpu_speed / HZ);
}


void __init time_init(void)
{
        unsigned int est_freq;

	printk("calculating r4koff... ");
	r4k_offset = cal_r4koff();
	printk("%08lx(%d)\n", r4k_offset, (int) r4k_offset);

	//est_freq = 2*r4k_offset*HZ;	
	est_freq = r4k_offset*HZ;	
	est_freq += 5000;    /* round */
	est_freq -= est_freq%10000;
	printk("CPU frequency %d.%02d MHz\n", est_freq/1000000, 
	       (est_freq%1000000)*100/1000000);
	set_au1000_speed(est_freq);
	set_au1000_lcd_clock(); // program the LCD clock
	r4k_cur = (read_32bit_cp0_register(CP0_COUNT) + r4k_offset);

	write_32bit_cp0_register(CP0_COMPARE, r4k_cur);

	/* no RTC on the pb1000 */
	xtime.tv_sec = 0;
	xtime.tv_usec = 0;

#ifdef CONFIG_PM
	/*
	 * setup counter 0, since it keeps ticking after a
	 * 'wait' instruction has been executed. The CP0 timer and
	 * counter 1 do NOT continue running after 'wait'
	 *
	 * It's too early to call request_irq() here, so we handle
	 * counter 0 interrupt as a special irq and it doesn't show
	 * up under /proc/interrupts.
	 */
	while (readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_C0S);
	writel(0, SYS_TOYWRITE);
	while (readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_C0S);

	writel(readl(SYS_WAKEMSK) | (1<<8), SYS_WAKEMSK);
	writel(~0, SYS_WAKESRC);
	au_sync();
	while (readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_M20);

	/* setup match20 to interrupt once every 10ms */
	last_pc0 = last_match20 = readl(SYS_TOYREAD);
	writel(last_match20 + MATCH20_INC, SYS_TOYMATCH2);
	au_sync();
	while (readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_M20);
	startup_match20_interrupt();
#endif

	//set_cp0_status(ALLINTS);
	au_sync();
}

/* This is for machines which generate the exact clock. */
#define USECS_PER_JIFFY (1000000/HZ)
#define USECS_PER_JIFFY_FRAC (0x100000000*1000000/HZ&0xffffffff)


static unsigned long
div64_32(unsigned long v1, unsigned long v2, unsigned long v3)
{
	unsigned long r0;
	do_div64_32(r0, v1, v2, v3);
	return r0;
}


static unsigned long do_fast_gettimeoffset(void)
{
#ifdef CONFIG_PM
	unsigned long pc0;
	unsigned long offset;

	pc0 = readl(SYS_TOYREAD);
	if (pc0 < last_pc0) {
		offset = 0xffffffff - last_pc0 + pc0;
		printk("offset over: %x\n", (unsigned)offset);
	}
	else {
		offset = (unsigned long)(((pc0 - last_pc0) * 305) / 10);
	}
	if ((pc0-last_pc0) > 2*MATCH20_INC) {
		printk("huge offset %x, last_pc0 %x last_match20 %x pc0 %x\n", 
				(unsigned)offset, (unsigned)last_pc0, 
				(unsigned)last_match20, (unsigned)pc0);
	}
	au_sync();
	return offset;
#else
	u32 count;
	unsigned long res, tmp;
	unsigned long r0;

	/* Last jiffy when do_fast_gettimeoffset() was called. */
	static unsigned long last_jiffies=0;
	unsigned long quotient;

	/*
	 * Cached "1/(clocks per usec)*2^32" value.
	 * It has to be recalculated once each jiffy.
	 */
	static unsigned long cached_quotient=0;

	tmp = jiffies;

	quotient = cached_quotient;

	if (tmp && last_jiffies != tmp) {
		last_jiffies = tmp;
		if (last_jiffies != 0) {
			r0 = div64_32(timerhi, timerlo, tmp);
			quotient = div64_32(USECS_PER_JIFFY, USECS_PER_JIFFY_FRAC, r0);
			cached_quotient = quotient;
		}
	}

	/* Get last timer tick in absolute kernel time */
	count = read_32bit_cp0_register(CP0_COUNT);

	/* .. relative to previous jiffy (32 bits is enough) */
	count -= timerlo;

	__asm__("multu\t%1,%2\n\t"
		"mfhi\t%0"
		:"=r" (res)
		:"r" (count),
		 "r" (quotient));

	/*
 	 * Due to possible jiffies inconsistencies, we need to check 
	 * the result so that we'll get a timer that is monotonic.
	 */
	if (res >= USECS_PER_JIFFY)
		res = USECS_PER_JIFFY-1;

	return res;
#endif
}

void do_gettimeofday(struct timeval *tv)
{
	unsigned int flags;

	read_lock_irqsave (&xtime_lock, flags);
	*tv = xtime;
	tv->tv_usec += do_fast_gettimeoffset();

	/*
	 * xtime is atomically updated in timer_bh. jiffies - wall_jiffies
	 * is nonzero if the timer bottom half hasnt executed yet.
	 */
	if (jiffies - wall_jiffies)
		tv->tv_usec += USECS_PER_JIFFY;

	read_unlock_irqrestore (&xtime_lock, flags);

	if (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq (&xtime_lock);

	/* This is revolting. We need to set the xtime.tv_usec correctly.
	 * However, the value in this location is is value at the last tick.
	 * Discover what correction gettimeofday would have done, and then
	 * undo it!
	 */
	tv->tv_usec -= do_fast_gettimeoffset();

	if (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;

	write_unlock_irq (&xtime_lock);
}
