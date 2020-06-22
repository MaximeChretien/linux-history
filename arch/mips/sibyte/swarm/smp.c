/*
 * Copyright (C) 2001 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/sibyte/sb1250.h>
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_int.h>
#include <asm/mipsregs.h>
#include <asm/mmu_context.h>

#include "cfe_xiocb.h"
#include "cfe_api.h"

extern void asmlinkage smp_bootstrap(void);

/* Boot all other cpus in the system, initialize them, and
   bring them into the boot fn */
int prom_boot_secondary(int cpu, unsigned long sp, unsigned long gp)
{
	int retval;
	
	retval = cfe_start_cpu(cpu, &smp_bootstrap, sp, gp, 0);
	if (retval != 0) {
		printk("cfe_start_cpu(%i) returned %i\n" , cpu, retval);
		return 0;
	} else {
		return 1;
	}
}


void prom_init_secondary(void)
{
	/* Set up kseg0 to be cachable coherent */
	clear_cp0_config(CONF_CM_CMASK);
	set_cp0_config(0x5);

	/* Enable interrupts for lines 0-4 */
	clear_cp0_status(0xe000);
	set_cp0_status(0x1f01);
}


/*
 * Set up state, return the total number of cpus in the system, including
 * the master
 */
int prom_setup_smp(void)
{
	int i;
	int num_cpus = 1;

	/* Use CFE to find out how many CPUs are available */
	for (i=1; i<NR_CPUS; i++) {
		if (cfe_stop_cpu(i) == 0) {
			num_cpus++;
		}
	}
	printk("Detected %i available CPU(s)\n", num_cpus);
	return num_cpus;
}

void prom_smp_finish(void)
{
	extern void sb1250_smp_finish(void);
	sb1250_smp_finish();
}

/*
 * XXX This is really halfway portable code and halfway system specific code.
 * XXX Seems like some of this is CPU-specific, too - rather than board/system.
 */
extern atomic_t cpus_booted;

void __init smp_boot_cpus(void)
{
	int i;
	int cur_cpu = 0;

	smp_num_cpus = prom_setup_smp();
	init_new_context(current, &init_mm);
	current->processor = 0;
	cpu_data[0].udelay_val = loops_per_jiffy;
	cpu_data[0].asid_cache = ASID_FIRST_VERSION;
	CPUMASK_CLRALL(cpu_online_map);
	CPUMASK_SETB(cpu_online_map, 0);
	atomic_set(&cpus_booted, 1);  /* Master CPU is already booted... */
	__cpu_number_map[0] = 0;
	__cpu_logical_map[0] = 0;
	init_idle();

	/* 
	 * This loop attempts to compensate for "holes" in the CPU
	 * numbering.  It's overkill, but general.
	 */
	for (i = 1; i < smp_num_cpus; ) {
		struct task_struct *p;
		struct pt_regs regs;
		int retval;
		printk("Starting CPU %d... ", i);

		/* Spawn a new process normally.  Grab a pointer to
		   its task struct so we can mess with it */
		do_fork(CLONE_VM|CLONE_PID, 0, &regs, 0);
		p = init_task.prev_task;

		/* Schedule the first task manually */
		p->processor = i;
		p->cpus_runnable = 1 << i; /* we schedule the first task manually */

		/* Attach to the address space of init_task. */
		atomic_inc(&init_mm.mm_count);
		p->active_mm = &init_mm;
		init_tasks[i] = p;

		del_from_runqueue(p);
		unhash_process(p);

		do {
			/* Iterate until we find a CPU that comes up */
			cur_cpu++;
			retval = prom_boot_secondary(cur_cpu,
					    (unsigned long)p + KERNEL_STACK_SIZE - 32,
					    (unsigned long)p);
			__cpu_number_map[i] = i;
			__cpu_logical_map[i] = i;
		} while (!retval && (cur_cpu < NR_CPUS));
		if (retval) {
			i++;
		} else {
			panic("CPU discovery disaster");
		}

#if 0
		/* This is copied from the ip-27 code in the mips64 tree */

		struct task_struct *p;

		/*
		 * The following code is purely to make sure
		 * Linux can schedule processes on this slave.
		 */
		kernel_thread(0, NULL, CLONE_PID);
		p = init_task.prev_task;
		sprintf(p->comm, "%s%d", "Idle", i);
		init_tasks[i] = p;
		p->processor = i;
		p->cpus_runnable = 1 << i; /* we schedule the first task manually */
		del_from_runqueue(p);
		unhash_process(p);
		/* Attach to the address space of init_task. */
		atomic_inc(&init_mm.mm_count);
		p->active_mm = &init_mm;
		prom_boot_secondary(i, 
				    (unsigned long)p + KERNEL_STACK_SIZE - 32,
				    (unsigned long)p);
#endif
	}

	/* Wait for everyone to come up */
	while (atomic_read(&cpus_booted) != smp_num_cpus);

	smp_threads_ready = 1;
}
