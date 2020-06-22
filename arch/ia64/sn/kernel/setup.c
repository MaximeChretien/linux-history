/*
 * Copyright (C) 1999,2001-2002 Silicon Graphics, Inc. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/timex.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/serial.h>
#include <linux/irq.h>
#include <linux/bootmem.h>
#include <linux/mmzone.h>
#include <linux/interrupt.h>
#include <linux/acpi.h>
#include <linux/compiler.h>
#include <linux/sched.h>

#include <asm/io.h>
#include <asm/sal.h>
#include <asm/machvec.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/sn/sgi.h>
#include <asm/sn/io.h>
#include <asm/sn/arch.h>
#include <asm/sn/addrs.h>
#include <asm/sn/pda.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/simulator.h>
#include <asm/sn/leds.h>
#include <asm/sn/bte.h>
#include <asm/sn/clksupport.h>
#include <asm/sn/sn_sal.h>

#ifdef CONFIG_IA64_SGI_SN2
#include <asm/sn/sn2/shub.h>
#endif

extern void bte_init_node (nodepda_t *, cnodeid_t);
extern void bte_init_cpu (void);

unsigned long sn_rtc_cycles_per_second;   
unsigned long sn_rtc_usec_per_cyc;

partid_t sn_partid = -1;
char sn_system_serial_number_string[128];
u64 sn_partition_serial_number;

/*
 * This is the address of the RRegs in the HSpace of the global
 * master.  It is used by a hack in serial.c (serial_[in|out],
 * printk.c (early_printk), and kdb_io.c to put console output on that
 * node's Bedrock UART.  It is initialized here to 0, so that
 * early_printk won't try to access the UART before
 * master_node_bedrock_address is properly calculated.
 */
u64 master_node_bedrock_address = 0UL;

static void sn_init_pdas(char **);

extern struct irq_desc *_sn_irq_desc[];

#if defined(CONFIG_IA64_SGI_SN1)
extern synergy_da_t	*Synergy_da_indr[];
#endif

static nodepda_t	*nodepdaindr[MAX_COMPACT_NODES];

#ifdef CONFIG_IA64_SGI_SN2
irqpda_t		*irqpdaindr[NR_CPUS];
#endif /* CONFIG_IA64_SGI_SN2 */


/*
 * The format of "screen_info" is strange, and due to early i386-setup
 * code. This is just enough to make the console code think we're on a
 * VGA color display.
 */
struct screen_info sn_screen_info = {
	orig_x:			 0,
	orig_y:			 0,
	orig_video_mode:	 3,
	orig_video_cols:	80,
	orig_video_ega_bx:	 3,
	orig_video_lines:	25,
	orig_video_isVGA:	 1,
	orig_video_points:	16
};

/*
 * This is here so we can use the CMOS detection in ide-probe.c to
 * determine what drives are present.  In theory, we don't need this
 * as the auto-detection could be done via ide-probe.c:do_probe() but
 * in practice that would be much slower, which is painful when
 * running in the simulator.  Note that passing zeroes in DRIVE_INFO
 * is sufficient (the IDE driver will autodetect the drive geometry).
 */
char drive_info[4*16];

/**
 * sn_map_nr - return the mem_map entry for a given kernel address
 * @addr: kernel address to query
 *
 * Finds the mem_map entry for the kernel address given.  Used by
 * virt_to_page() (asm-ia64/page.h), among other things.
 */
unsigned long
sn_map_nr (void *addr)
{
	return MAP_NR_DISCONTIG(addr);
}

/**
 * early_sn_setup - early setup routine for SN platforms
 *
 * Sets up an intial console to aid debugging.  Intended primarily
 * for bringup, it's only called if %BRINGUP and %CONFIG_IA64_EARLY_PRINTK
 * are turned on.  See start_kernel() in init/main.c.
 */
#if defined(CONFIG_IA64_EARLY_PRINTK)

void __init
early_sn_setup(void)
{
	void ia64_sal_handler_init (void *entry_point, void *gpval);
	efi_system_table_t			*efi_systab;
	efi_config_table_t 			*config_tables;
	struct ia64_sal_systab			*sal_systab;
	struct ia64_sal_desc_entry_point	*ep;
	char					*p;
	int					i;

	/*
	 * Parse enough of the SAL tables to locate the SAL entry point. Since, console
	 * IO on SN2 is done via SAL calls, early_printk wont work without this.
	 *
	 * This code duplicates some of the ACPI table parsing that is in efi.c & sal.c.
	 * Any changes to those file may have to be made hereas well.
	 */
	efi_systab = (efi_system_table_t*)__va(ia64_boot_param->efi_systab);
	config_tables = __va(efi_systab->tables);
	for (i = 0; i < efi_systab->nr_tables; i++) {
		if (efi_guidcmp(config_tables[i].guid, SAL_SYSTEM_TABLE_GUID) == 0) {
			sal_systab = __va(config_tables[i].table);
			p = (char*)(sal_systab+1);
			for (i = 0; i < sal_systab->entry_count; i++) {
				if (*p == SAL_DESC_ENTRY_POINT) {
					ep = (struct ia64_sal_desc_entry_point *) p;
					ia64_sal_handler_init(__va(ep->sal_proc), __va(ep->gp));
					break;
				}
				p += SAL_DESC_SIZE(*p);
			}
		}
	}

	if ( IS_RUNNING_ON_SIMULATOR() ) {
#if defined(CONFIG_IA64_SGI_SN1)
		master_node_bedrock_address = (u64)REMOTE_HSPEC_ADDR(get_nasid(), 0);
#else
		master_node_bedrock_address = (u64)REMOTE_HUB(get_nasid(), SH_JUNK_BUS_UART0);
#endif
		printk(KERN_DEBUG "early_sn_setup: setting master_node_bedrock_address to 0x%lx\n", master_node_bedrock_address);
	}
}
#endif /* CONFIG_IA64_SGI_SN1 */

#ifdef CONFIG_IA64_MCA
extern int platform_intr_list[];
#endif

extern nasid_t master_nasid;

/**
 * sn_setup - SN platform setup routine
 * @cmdline_p: kernel command line
 *
 * Handles platform setup for SN machines.  This includes determining
 * the RTC frequency (via a SAL call), initializing secondary CPUs, and
 * setting up per-node data areas.  The console is also initialized here.
 */
void __init
sn_setup(char **cmdline_p)
{
	long status, ticks_per_sec, drift;
	int i;
	int major = sn_sal_rev_major(), minor = sn_sal_rev_minor();

	printk("SGI SAL version %x.%02x\n", major, minor);

	/*
	 * Confirm the SAL we're running on is recent enough...
	 */
	if ((major < SN_SAL_MIN_MAJOR) || (major == SN_SAL_MIN_MAJOR &&
					   minor < SN_SAL_MIN_MINOR)) {
		printk(KERN_ERR "This kernel needs SGI SAL version >= "
		       "%x.%02x\n", SN_SAL_MIN_MAJOR, SN_SAL_MIN_MINOR);
		panic("PROM version too old\n");
	}

#ifdef CONFIG_IA64_SGI_SN2
	{
		extern void io_sh_swapper(int, int);
		io_sh_swapper(get_nasid(), 0);
	}
#endif

	master_nasid = get_nasid();
	(void)get_console_nasid();
#ifndef CONFIG_IA64_SGI_SN1
	{
		extern nasid_t get_master_baseio_nasid(void);
		(void)get_master_baseio_nasid();
	}
#endif

	status = ia64_sal_freq_base(SAL_FREQ_BASE_REALTIME_CLOCK, &ticks_per_sec, &drift);
	if (status != 0 || ticks_per_sec < 100000) {
		printk(KERN_WARNING "unable to determine platform RTC clock frequency, guessing.\n");
		/* PROM gives wrong value for clock freq. so guess */
		sn_rtc_cycles_per_second = 1000000000000UL/30000UL;
	}
	else
		sn_rtc_cycles_per_second = ticks_per_sec;

#ifdef CONFIG_IA64_SGI_SN1
	/* PROM has wrong value on SN1 */
	sn_rtc_cycles_per_second = 990177;
#endif
	sn_rtc_usec_per_cyc = ((1000000UL<<IA64_USEC_PER_CYC_SHIFT)
			       + sn_rtc_cycles_per_second/2) / sn_rtc_cycles_per_second;
		
	for (i=0;i<NR_CPUS;i++)
		_sn_irq_desc[i] = _irq_desc;

	platform_intr_list[ACPI_INTERRUPT_CPEI] = IA64_PCE_VECTOR;


	if ( IS_RUNNING_ON_SIMULATOR() )
	{
#ifdef CONFIG_IA64_SGI_SN2
		master_node_bedrock_address = (u64)REMOTE_HUB(get_nasid(), SH_JUNK_BUS_UART0);
#else
		master_node_bedrock_address = (u64)REMOTE_HSPEC_ADDR(get_nasid(), 0);
#endif
		printk(KERN_DEBUG "sn_setup: setting master_node_bedrock_address to 0x%lx\n",
		       master_node_bedrock_address);
	}

	/*
	 * we set the default root device to /dev/hda
	 * to make simulation easy
	 */
	ROOT_DEV = to_kdev_t(0x0301);

	/*
	 * Create the PDAs and NODEPDAs for all the cpus.
	 */
	sn_init_pdas(cmdline_p);


	/* 
	 * For the bootcpu, we do this here. All other cpus will make the
	 * call as part of cpu_init in slave cpu initialization.
	 */
	sn_cpu_init();


#ifdef CONFIG_SMP
	init_smp_config();
#endif
	screen_info = sn_screen_info;

	/*
	 * Turn off "floating-point assist fault" warnings by default.
	 */
	current->thread.flags |= IA64_THREAD_FPEMU_NOPRINT;
}

/**
 * sn_init_pdas - setup node data areas
 *
 * One time setup for Node Data Area.  Called by sn_setup().
 */
void
sn_init_pdas(char **cmdline_p)
{
	cnodeid_t	cnode;

	/*
	 * Make sure that the PDA fits entirely in the same page as the 
	 * cpu_data area.
	 */
	if ((PDAADDR&~PAGE_MASK)+sizeof(pda_t) > PAGE_SIZE)
		panic("overflow of cpu_data page");

        /*
         * Allocate & initalize the nodepda for each node.
         */
        for (cnode=0; cnode < numnodes; cnode++) {
		nodepdaindr[cnode] = alloc_bootmem_node(NODE_DATA(cnode), sizeof(nodepda_t));
		memset(nodepdaindr[cnode], 0, sizeof(nodepda_t));

#if defined(CONFIG_IA64_SGI_SN1)
		Synergy_da_indr[cnode * 2] = (synergy_da_t *) alloc_bootmem_node(NODE_DATA(cnode), sizeof(synergy_da_t));
		Synergy_da_indr[cnode * 2 + 1] = (synergy_da_t *) alloc_bootmem_node(NODE_DATA(cnode), sizeof(synergy_da_t));
		memset(Synergy_da_indr[cnode * 2], 0, sizeof(synergy_da_t));
		memset(Synergy_da_indr[cnode * 2 + 1], 0, sizeof(synergy_da_t));
#endif
        }

	/*
	 * Now copy the array of nodepda pointers to each nodepda.
	 */
        for (cnode=0; cnode < numnodes; cnode++)
		memcpy(nodepdaindr[cnode]->pernode_pdaindr, nodepdaindr, sizeof(nodepdaindr));


	/*
	 * Set up IO related platform-dependent nodepda fields.
	 * The following routine actually sets up the hubinfo struct
	 * in nodepda.
	 */
	for (cnode = 0; cnode < numnodes; cnode++) {
		init_platform_nodepda(nodepdaindr[cnode], cnode);
		bte_init_node (nodepdaindr[cnode], cnode);
	}
}

/**
 * sn_cpu_init - initialize per-cpu data areas
 * @cpuid: cpuid of the caller
 *
 * Called during cpu initialization on each cpu as it starts.
 * Currently, initializes the per-cpu data area for SNIA.
 * Also sets up a few fields in the nodepda.  Also known as
 * platform_cpu_init() by the ia64 machvec code.
 */
void __init
sn_cpu_init(void)
{
	int	cpuid;
	int	cpuphyid;
	int	nasid;
	int	slice;
	int	cnode;

	/*
	 * The boot cpu makes this call again after platform initialization is
	 * complete.
	 */
	if (nodepdaindr[0] == NULL)
		return;

	cpuid = smp_processor_id();
	cpuphyid = ((ia64_get_lid() >> 16) & 0xffff);
	nasid = cpu_physical_id_to_nasid(cpuphyid);
	cnode = nasid_to_cnodeid(nasid);
	slice = cpu_physical_id_to_slice(cpuphyid);

	printk("CPU %d: nasid %d, slice %d, cnode %d\n",
			smp_processor_id(), nasid, slice, cnode);

	memset(&pda, 0, sizeof(pda));
	pda.p_nodepda = nodepdaindr[cnode];
	pda.led_address = (typeof(pda.led_address)) (LED0 + (slice<<LED_CPU_SHIFT));
#ifdef LED_WAR
	pda.led_address = (typeof(pda.led_address)) (LED0 + (slice<<(LED_CPU_SHIFT-1))); /* temp til L1 firmware fixed */
#endif
	pda.led_state = LED_ALWAYS_SET;
	pda.hb_count = HZ/2;
	pda.hb_state = 0;
	pda.idle_flag = 0;
	
	if (local_node_data->active_cpu_count == 1)
		nodepda->node_first_cpu = cpuid;

#ifdef CONFIG_IA64_SGI_SN1
	{
		int	synergy;
		synergy = cpu_physical_id_to_synergy(cpuphyid);
		pda.p_subnodepda = &nodepdaindr[cnode]->snpda[synergy];
	}
#endif

#ifdef CONFIG_IA64_SGI_SN2

	/*
	 * We must use different memory allocators for first cpu (bootmem 
	 * allocator) than for the other cpus (regular allocator).
	 */
	if (cpuid == 0)
		irqpdaindr[cpuid] = alloc_bootmem_node(NODE_DATA(cpuid_to_cnodeid(cpuid)),sizeof(irqpda_t));
	else
		irqpdaindr[cpuid] = page_address(alloc_pages_node(local_cnodeid(), GFP_KERNEL, get_order(sizeof(irqpda_t))));

	memset(irqpdaindr[cpuid], 0, sizeof(irqpda_t));
	pda.p_irqpda = irqpdaindr[cpuid];

	pda.pio_write_status_addr = (volatile unsigned long *)
			LOCAL_MMR_ADDR((slice < 2 ? SH_PIO_WRITE_STATUS_0 : SH_PIO_WRITE_STATUS_1 ) );
	pda.mem_write_status_addr = (volatile u64 *)
			LOCAL_MMR_ADDR((slice < 2 ? SH_MEMORY_WRITE_STATUS_0 : SH_MEMORY_WRITE_STATUS_1 ) );

	if (nodepda->node_first_cpu == cpuid) {
		int	buddy_nasid;
		buddy_nasid = cnodeid_to_nasid(local_cnodeid() == numnodes-1 ? 0 : local_cnodeid()+ 1);
		pda.pio_shub_war_cam_addr = (volatile unsigned long*)GLOBAL_MMR_ADDR(nasid, SH_PI_CAM_CONTROL);
	}

#ifdef BRINGUP2
	/*
	 * Zero out the counters used for counting MCAs
	 *   ZZZZZ temp
	 */
	{
		long	*p;
		p = (long*)LOCAL_MMR_ADDR(SH_XN_IILB_LB_CMP_ENABLE0);
		*p = 0;
		p = (long*)LOCAL_MMR_ADDR(SH_XN_IILB_LB_CMP_ENABLE1);
		*p = 0;
	}

#endif
#endif

#ifdef CONFIG_IA64_SGI_SN1
	pda.bedrock_rev_id = (volatile unsigned long *) LOCAL_HUB(LB_REV_ID);
	if (cpuid_to_synergy(cpuid))
		/* CPU B */
		pda.pio_write_status_addr = (volatile unsigned long *) GBL_PERF_B_ADDR;
	else
		/* CPU A */
		pda.pio_write_status_addr = (volatile unsigned long *) GBL_PERF_A_ADDR;
#endif


	bte_init_cpu();
}


/**
 * cnodeid_to_cpuid - convert a cnode to a cpuid of a cpu on the node.
 * @cnode: node to get a cpuid from
 *	
 * Returns -1 if no cpus exist on the node.
 * NOTE:BRINGUP ZZZ This is NOT a good way to find cpus on the node.
 * Need a better way!!
 */
int
cnodeid_to_cpuid(int cnode) {
	int cpu;

	for (cpu = 0; cpu < smp_num_cpus; cpu++)
		if (cpuid_to_cnodeid(cpu) == cnode)
			break;

	if (cpu == smp_num_cpus) 
		cpu = -1;

	return cpu;
}

/**
 * get_cycles - return a non-decreasing timestamp
 *
 * On SN, we use an RTC read for this function
 */
cycles_t
get_cycles (void)
{
	return GET_RTC_COUNTER();
}

/**
 * gettimeoffset - number of usecs elapsed since &xtime was last updated
 *
 * This function is used by do_gettimeofday() to determine the number
 * of usecs that have elapsed since the last update to &xtime.  On SN
 * this is accomplished using the RTC built in to each Hub chip; each
 * is guaranteed to be synchronized by the PROM, so a local read will
 * suffice (get_cycles() does this for us).  A snapshot of the RTC value
 * is taken on every timer interrupt and this function more or less
 * subtracts that snapshot value from the current value.
 *
 * Note that if a lot of processing was done during the last timer
 * interrupt then &xtime may be some number of jiffies out of date.
 * This function must account for that.
 */
unsigned long
gettimeoffset(void)
{
	unsigned long current_rtc_val, local_last_rtc_val;
	unsigned long usec;

	local_last_rtc_val = last_rtc_val;
	current_rtc_val = get_cycles();
	usec = last_itc_lost_usec;

	/* If the RTC has wrapped around, compensate */
	if (unlikely(current_rtc_val < local_last_rtc_val)) {
		printk(KERN_NOTICE "RTC wrapped cpu:%d current:0x%lx last:0x%lx\n",
				smp_processor_id(), current_rtc_val,
				local_last_rtc_val);
		current_rtc_val += RTC_MASK;
	}

	usec += ((current_rtc_val - local_last_rtc_val)*sn_rtc_usec_per_cyc) >> 
		IA64_USEC_PER_CYC_SHIFT;

	/*
	 * usec is the number of microseconds into the current clock interval. Every
	 * clock tick, xtime is advanced by "tick" microseconds. If "usec"
	 * is allowed to get larger than "tick", the time value returned by gettimeofday
	 * will go backward.
	 */
	if (usec >= tick)
		usec = tick-1;

	return usec;
}

#ifdef II_PRTE_TLB_WAR
long iiprt_lock[16*64] __cacheline_aligned; /* allow for NASIDs up to 64 */
#endif

#ifdef BUS_INT_WAR

#include <asm/hw_irq.h>
#include <asm/sn/pda.h>

void ia64_handle_irq (ia64_vector vector, struct pt_regs *regs);

static spinlock_t irq_lock = SPIN_LOCK_UNLOCKED;

#define IRQCPU(irq)	((irq)>>8)

void
sn_add_polled_interrupt(int irq, int interval)
{
	unsigned long flags, irq_cnt;
	sn_poll_entry_t	*irq_list;

	irq_list = pdacpu(IRQCPU(irq)).pda_poll_entries;;

	spin_lock_irqsave(&irq_lock, flags);
	irq_cnt = pdacpu(IRQCPU(irq)).pda_poll_entry_count;
	irq_list[irq_cnt].irq = irq;
	irq_list[irq_cnt].interval = interval;
	irq_list[irq_cnt].tick = interval;
	pdacpu(IRQCPU(irq)).pda_poll_entry_count++;
	spin_unlock_irqrestore(&irq_lock, flags);


}

void
sn_delete_polled_interrupt(int irq)
{
	unsigned long flags, i, irq_cnt;
	sn_poll_entry_t	*irq_list;

	irq_list = pdacpu(IRQCPU(irq)).pda_poll_entries;

	spin_lock_irqsave(&irq_lock, flags);
	irq_cnt = pdacpu(IRQCPU(irq)).pda_poll_entry_count;
	for (i=0; i<irq_cnt; i++) {
		if (irq_list[i].irq == irq) {
			irq_list[i] = irq_list[irq_cnt-1];
			pdacpu(IRQCPU(irq)).pda_poll_entry_count--;
			break;
		}
	}
	spin_unlock_irqrestore(&irq_lock, flags);
}

long sn_int_poll_ticks=50;

void
sn_irq_poll(int cpu, int reason) 
{
	unsigned long flags, i;
	sn_poll_entry_t	*irq_list;


#ifdef CONFIG_SHUB_1_0_SPECIFIC
	ia64_handle_irq(IA64_IPI_VECTOR, 0);
#else
	if (sn_int_poll_ticks == 0)
		return;
#endif
	irq_list = pda.pda_poll_entries;

	for (i=0; i<pda.pda_poll_entry_count; i++, irq_list++) {
		if (--irq_list->tick <= 0) {
#ifdef CONFIG_SHUB_1_0_SPECIFIC
			irq_list->tick = irq_list->interval;
#else
			irq_list->tick = sn_int_poll_ticks;
#endif
			local_irq_save(flags);
			ia64_handle_irq(irq_to_vector(irq_list->irq), 0);
			local_irq_restore(flags);
		}
	}
}	

#define PROCFILENAME	"sn_int_poll_ticks"

static struct proc_dir_entry *proc_op;


static int
read_proc(char *buffer, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;

	len += sprintf(buffer + len, "Poll hack: interval %ld ticks\n", sn_int_poll_ticks);

	if (len <= off+count) *eof = 1;
	*start = buffer + off;
	len   -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int
write_proc (struct file *file, const char *userbuf, unsigned long count, void *data)
{
       extern long atoi(char *);
       char        buf[80];

       if (copy_from_user(buf, userbuf, count < sizeof(buf) ? count : sizeof(buf)))
           return -EFAULT;

       sn_int_poll_ticks = atoi(buf);

       return count;
}


int __init
sn_poll_init(void)
{
	if ((proc_op = create_proc_entry(PROCFILENAME, 0644, NULL)) == NULL) {
		printk("%s: unable to create proc entry", PROCFILENAME);
		return -1;
	}
	proc_op->read_proc = read_proc;
	proc_op->write_proc = write_proc;
	return 0;
}

module_init(sn_poll_init);
#endif
