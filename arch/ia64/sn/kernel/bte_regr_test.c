/*
 *
 *
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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


/***********************************************************************
 * Block Transfer Engine regression tests.
 *
 *	The following set of tests can be used to test for regressions.
 *	It is implemented as a loadable module.
 *
 *	To enable the tests, the kernel must be booted with the
 *	"btetest" command line flag.  If the tests are compiled into the
 *	kernel, additional values may be passed with
 *	"bte_test=t,v,ht,tn,tx,ti,ta,tc"
 *	where:
 *	    t = Bitmask of tests to run.
 *	    v = Level of verbosity.
 *	    ht = Number of seconds to try to force a notification hang.
 *          hu = Number of uSecs to wait until warning of hang.
 *	    tn = Min number of lines to rate test with (2 raised to).
 *	    tx = Max number of lines to rate test with (2 raised to).
 *	    ti = Number of iterations per timing.
 *          ta = Aternate through cpus on each pass.
 *	    tc = Use/Don't Use memcopy
 *
 *	When loaded as a module, each of those values has a seperate
 *	parameter.  Just do a modinfo bte_test.o to get those names
 *	and valid ranges.
 *
 *	Tests are performed in the following order.
 *
 *	Standard Transfer Test - Just transfers a block of initialized
 *	data to a cleared block and ensures that memory before and after
 *	is untouched, but that the body has all the correct values.
 *
 *	Transfer Rate Test - Data is transfered from node to node
 *	to ensure every node is able to BTE data.  Timings are created
 *	for each node.
 *
 *	Notification Hang Test - Attempts to force the Notification
 *	hang problem to arise.  A hang occurs when the BTE fails
 *	to invalidate a processors cache line for the notification
 *	word, resulting in the processor not seeing the updated
 *	value.
 *
 *	Invalid Transfer Test - In this test, we attempt to transfer
 *	data from a valid address to a nasid which does not exist.
 *
 **********************************************************************/


#define BTE_TIME 1		/* Needed to ensure bte_copy records
				 * timings */
// #define BTE_DEBUG
// #define BTE_DEBUG_VERBOSE

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/sn/nodepda.h>
#include <asm/processor.h>

#include <asm/sn/bte_copy.h>


/***********************************************************************
 * Local defines, structs, and typedefs.
 *
 **********************************************************************/


/*
 * The following struct defines standard transfers to use while
 * testing.
 */
typedef struct brt_xfer_entry_s {
	int source_offset;
	int dest_offset;
	int length;
} brt_xfer_entry_t;

/*
 * BRT_TEST_BLOCK_SIZE needs to accomodate the largest transfer that
 * is found in brt_xfer_tests.
 */
#define BRT_TEST_BLOCK_SIZE 1024

/* Flags for selecting tests to run. */
#define TEST_REGRESSION		0x00000001	/* Standard Transfer */
#define TEST_TIME		0x00000002	/* Timed Transfer */
#define TEST_NOTIFY		0x00000004	/* Notification Hang */
#define TEST_NONODE		0x00000008	/* Invalid Nasid Xfer */


/***********************************************************************
 * Global variables.
 * 
 **********************************************************************/


/*
 * bte_setup_time - Time it takes for bte_copy to get locks
 * 		    acquired and values into SHUB registers to start the
 * 		    xfer.
 *
 * bte_transfer_time - Time where hardware is doing the xfer.
 *
 * bte_tear_down_time - Time to unlock and return.
 *
 * bte_execute_time - Time from first call until return.
 */
volatile static u64 bte_setup_time;
volatile static u64 bte_transfer_time;
volatile static u64 bte_tear_down_time;
volatile static u64 bte_execute_time;

/* Tests to run during standard transfer tests. */
static brt_xfer_entry_t brt_xfer_tests[] = {
	{0, 0, 2 * L1_CACHE_BYTES},	/* The ideal case. */

	{0, 0, 35},		/* Symetrical aligned test. */
	{L1_CACHE_BYTES, L1_CACHE_BYTES, 35},
	{0, 0, L1_CACHE_BYTES + 35},
	{L1_CACHE_BYTES, L1_CACHE_BYTES, L1_CACHE_BYTES + 35},
	{0, 0, (2 * L1_CACHE_BYTES) + 35},
	{L1_CACHE_BYTES, L1_CACHE_BYTES, (2 * L1_CACHE_BYTES) + 35},
	{0, 0, (4 * L1_CACHE_BYTES) + 35},
	{L1_CACHE_BYTES, L1_CACHE_BYTES, (4 * L1_CACHE_BYTES) + 35},

	{(0 + 25), (0 + 25), 35},	/* Symetrical unaligned test. */
	{(L1_CACHE_BYTES + 25), (L1_CACHE_BYTES + 25), 35},
	{(0 + 25), (0 + 25), L1_CACHE_BYTES + 35},
	{(L1_CACHE_BYTES + 25), (L1_CACHE_BYTES + 25),
	 (L1_CACHE_BYTES + 25) + 35},
	{(0 + 25), (0 + 25), (2 * L1_CACHE_BYTES) + 35},
	{(L1_CACHE_BYTES + 25), (L1_CACHE_BYTES + 25),
	 (2 * L1_CACHE_BYTES) + 35},
	{(0 + 25), (0 + 25), (4 * L1_CACHE_BYTES) + 35},
	{(L1_CACHE_BYTES + 25), (L1_CACHE_BYTES + 25),
	 (4 * L1_CACHE_BYTES) + 35},

	{(0 + 25), (0 + 26), 35},	/* Asymetrical unaligned test. */
	{(L1_CACHE_BYTES + 25), (L1_CACHE_BYTES + 26), 35},
	{(0 + 25), (0 + 26), L1_CACHE_BYTES + 35},
	{(L1_CACHE_BYTES + 25), (L1_CACHE_BYTES + 26),
	 (L1_CACHE_BYTES + 25) + 35},
	{(0 + 25), (0 + 26), (2 * L1_CACHE_BYTES) + 35},
	{(L1_CACHE_BYTES + 25), (L1_CACHE_BYTES + 26),
	 (2 * L1_CACHE_BYTES) + 35},
	{(0 + 25), (0 + 26), (4 * L1_CACHE_BYTES) + 35},
	{(L1_CACHE_BYTES + 25), (L1_CACHE_BYTES + 26),
	 (4 * L1_CACHE_BYTES) + 35},

	{0, 0, 0}		/* Terminator */
};

static atomic_t brt_thread_cnt;	/* Threads in test. */
volatile static int brt_exit_flag;	/* Flag termination of hang test */

/* command line/module parameters */
static int selected_tests = 0;
static int verbose = 0;
static int hang_timeout = 10;
static int hang_usec = 12;
static int tm_min_lines = 1;
static int tm_max_lines = 3;
static int tm_iterations = 1;
static int tm_alternate = 0;
static int tm_memcpy = 1;
static int ix_iterations = 1000;
static int ix_srcnasid = -1;


/***********************************************************************
 * Local Function Prototypes.
 * 
 **********************************************************************/


static int __init brt_test_init(void);
static void __exit brt_test_exit(void);

/* Standard Transfer Test related functions. */
static int brt_tst_std_xfer(void);
static int brt_std_xfer(char *, char *, int, int, int, int);
static void brt_hex_dump(char *, int);

/* Timed Transfer Test related functions. */
static int brt_tst_time_xfers(void);
static void brt_time_xfer(int, int, int);

/* Notification Hang Test related functions. */
static int brt_tst_notify_hang(void);
static int brt_notify_thrd(void *);

/* Transfers to Invalid Nasid Test related functions. */
static int brt_tst_invalid_xfers(void);

#if !defined(MODULE)
/* Kernel command line handler. */
static int __init brt_setup(char *);
#endif				/* !defined(MODULE) */


/***********************************************************************
 * Module Load/Unload.
 * 
 **********************************************************************/


#define brt_marker() \
	printk("**************************************************" \
	       "**********************.\n"); \
	printk("\n"); \
	printk("**************************************************" \
	       "**********************.\n"); \
	printk("\n"); \
	printk("**************************************************" \
	       "**********************.\n"); \
	printk("\n");


static int __init
brt_test_init(void)
{
	int some_tests_removed;

	if (numnodes < 2) {
		printk("These tests are best run on multinode "
		       "systems.\n");
	}

	if (!pda.cpu_bte_if[0]->bte_test_buf) {
		some_tests_removed = 0;

		/* Timed Transfers go node-to-node. */
		if (selected_tests & TEST_TIME) {
			some_tests_removed = 1;
			selected_tests &= ~(TEST_TIME);
		}

		/* Notification Hang runs on all cpus simultaneously */
		if (selected_tests & TEST_NOTIFY) {
			some_tests_removed = 1;
			selected_tests &= ~(TEST_NOTIFY);
		}

		/* Invalid Tests */
		if (selected_tests & TEST_NONODE) {
			some_tests_removed = 1;
			selected_tests &= ~(TEST_NONODE);
		}

		if (some_tests_removed) {
			printk("Test Buffers were not allocated.\n");
			printk("Please reboot the system and supply "
			       "the \"btetest\" kernel flag\n");
			printk("Some tests were removed.\n");
		}
	}

	brt_marker();

	printk("brt_test(): Starting.\n");

	if (selected_tests & TEST_REGRESSION) {
		if (brt_tst_std_xfer()) {
			printk("Standard Transfers had errors.\n");
		}

	}

	if (selected_tests & TEST_TIME) {
		if (tm_min_lines < 0) {
			tm_min_lines = 0;
		}
		if (tm_max_lines < 0) {
			tm_max_lines = 0;
		}

		if (tm_max_lines > BTE_LEN_MASK) {
			tm_max_lines = BTE_LEN_MASK;
		}
		if (tm_min_lines > tm_max_lines) {
			tm_min_lines = tm_max_lines;
		}

		if (brt_tst_time_xfers()) {
			printk("Timed transfers had errors.\n");
		}

	}

	if (selected_tests & TEST_NOTIFY) {
		if (hang_usec < 8) {
			hang_usec = 8;
		}
		if (hang_usec > 256) {
			hang_usec = 256;
		}

		if (brt_tst_notify_hang()) {
			printk("Notification Hang test had errors.\n");
		}
	}

	if (selected_tests & TEST_NONODE) {
		if (brt_tst_invalid_xfers()) {
			printk("Invalid Nasid test had errors.\n");
		}
	}

	return (1);		/* Prevent module load. */
}


static void __exit
brt_test_exit(void)
{
}


/***********************************************************************
 * Standard Transfer Test.
 *	
 *	This test has a table of transfers defined above.  For each
 *	transfer, it calls bte_unaligned_copy.  It compares the actual
 *	result with the expected result.  If they differ, it prints out
 *	information about the transfer and hex dumps the actual block
 *
 **********************************************************************/


/*
 * Allocate the needed buffers and then initiate each xfer specified
 * by brt_xfer_tests.
 */
static int
brt_tst_std_xfer(void)
{
	char *block_1;
	char *block_2;
	int iteration = 0;
	brt_xfer_entry_t *cur_test;
	int cpu;
	int err_cnt;

	block_1 = kmalloc(BRT_TEST_BLOCK_SIZE, GFP_KERNEL);
	ASSERT(!((u64) block_1 & L1_CACHE_MASK));
	block_2 = kmalloc(BRT_TEST_BLOCK_SIZE, GFP_KERNEL);
	ASSERT(!((u64) block_2 & L1_CACHE_MASK));

	cur_test = brt_xfer_tests;

	err_cnt = 0;
	while (cur_test->length) {
		for (cpu = 0; cpu < smp_num_cpus; cpu++) {
			set_cpus_allowed(current, (1UL << cpu));

			if (verbose > 1) {
				printk("Cpu %d Transfering %d from "
				       "%d to %d.\n",
				       smp_processor_id(),
				       cur_test->length,
				       cur_test->source_offset,
				       cur_test->dest_offset);
			}

			err_cnt += brt_std_xfer(block_1, block_2,
						cur_test->source_offset,
						cur_test->dest_offset,
						cur_test->length,
						++iteration);

		}
		cur_test++;
	}

	kfree(block_2);
	kfree(block_1);

	return ((err_cnt ? 1 : 0));
}


/*
 * Perform a single transfer and ensure the result matches
 * the expected.  Returns the number of differences found.
 *
 * Testing is performed by setting the source buffer to a
 * known value, and zeroing out the destination.
 *
 * After the copy, if the destination has only the known
 * source values at the correct place, we know we had a
 * good transfer.
 *
 */
static int
brt_std_xfer(char *src, char *dst,
	     int src_offset, int dst_offset, int len,
	     int magic)
{
	int i, ret;
	int err_cnt = 0;

	if (verbose > 3) {
		printk("brt_test(src=0x%lx, dst=0x%lx, src_offset=%d, "
		       "dst_offset=%d, len=%d, magic=%d\n",
		       (u64) src, (u64) dst, src_offset,
		       dst_offset, len, magic);
	}

	memset(src, ((magic + 1) & 0xff), BRT_TEST_BLOCK_SIZE);
	memset((src + src_offset), magic, len);
	if (verbose > 8) {
		printk("Before transfer: Source is\n");
		brt_hex_dump(src, BRT_TEST_BLOCK_SIZE);
	}
	memset(dst, 0, BRT_TEST_BLOCK_SIZE);
	if (verbose > 8) {
		printk("Before transfer: dest is\n");
		brt_hex_dump(dst, BRT_TEST_BLOCK_SIZE);
	}

	ret = BTE_UNALIGNED_COPY(__pa(src + src_offset),
				 __pa(dst + dst_offset),
				 len, BTE_NOTIFY);
	if (ret != BTE_SUCCESS) {
		printk("brt_test: BTE_UNALIGNED_COPY() error: %d\n", ret);
		return (1);
	}

	/* Check head */
	for (i = 0; i < dst_offset; i++) {
		if ((dst[i] & 0xff) != 0) {
			err_cnt++;
		}
	}
	/* Check body */
	for (i = 0; i < len; i++) {
		if ((dst[dst_offset + i] & 0xff) != (magic & 0xff)) {
			err_cnt++;
		}
	}
	/* Check foot */
	for (i = (dst_offset + len); i < BRT_TEST_BLOCK_SIZE; i++) {
		if ((dst[i] & 0xff) != 0) {
			err_cnt++;
		}
	}

	if ((verbose > 3) || err_cnt) {
		printk("brt_test: %d errors during basic "
		       "transfer test.\n", err_cnt);
		brt_hex_dump(dst, BRT_TEST_BLOCK_SIZE);
	}

	return (err_cnt);
}


/*
 * Dump a block of data to console as hex.
 */
static void
brt_hex_dump(char *block_to_dump, int block_len)
{
	int i;
	char fmt_line[128];
	char *fmt_eol = fmt_line;

	for (i = 0; i < block_len; i++) {
		if (!(i % 16)) {
			if (i > 0) {
				printk("%s\n", fmt_line);
			}
			sprintf(fmt_line, "0x%lx %05d ",
				__pa(&block_to_dump[i]), i);
			fmt_eol = fmt_line;
		}
		while (*fmt_eol++) {	/* empty */
		};
		fmt_eol--;
		sprintf(fmt_eol, "%s%02x",
			(!(i % 4) ? " " : ""), (block_to_dump[i] & 0xff));
	}
	printk("%s\n", fmt_line);
}


/***********************************************************************
 * Transfer Rate Test.
 * 
 *	This test migrates to each cpu one at a time.  This is done to
 *	get a complete view of how each of the bte engines is performing
 *	and help ensure that each virtual interface is being used.
 *	NOTE: All virtual interfaces will not necessarily be used.
 *
 *	Now that we have migrated to the desired cpu, we transfer from
 *	our node to all the other nodes (including ourself).  We
 *	accomplish this with both a memcpy and a bte_copy.  Timings
 *	for both are printed.
 *
 **********************************************************************/


#define NSEC(x) ((x) * (1000000000UL / local_cpu_data->itc_freq))


/*
 * Migrate to each cpu. When on the desired cpu, time transfers to
 * each node by calling brt_time_xfer.
 */
static int
brt_tst_time_xfers(void)
{
	int tst_cpu;
	int dest_node;
	int xfer_lines;
	int i;

	if (tm_memcpy) {
		printk("Cpu,Src,Dst,Lines,Stup,Transfr,Fin,Execute,"
		       "Overall,Memcpy\n");
	} else {
		printk("Cpu,Src,Dst,Lines,Stup,Transfr,Fin,Execute,"
		       "Overall\n");
	}

	if (tm_alternate) {
		/* Now transfer from this node to all the others. */
		for (dest_node = 0; dest_node < numnodes; dest_node++) {
			for (xfer_lines = tm_min_lines;
			     xfer_lines <= tm_max_lines;) {

				for (i = 0; i < tm_iterations; i++) {
					for (tst_cpu = 0;
					     tst_cpu < smp_num_cpus;
					     tst_cpu++) {
						/* Move to the desired CPU. */
						set_cpus_allowed(current,
						    (1UL << tst_cpu));

						brt_time_xfer(dest_node,
							      1,
							      xfer_lines);

					}
				}
				/* Handle a min of 0 */
				if (xfer_lines < 1) {
					xfer_lines = 1;
				} else {
					xfer_lines *= 2;
				}
			}
		}
	} else {
		for (tst_cpu = 0; tst_cpu < smp_num_cpus; tst_cpu++) {
			/* Move to the desired CPU. */
			set_cpus_allowed(current, (1UL << tst_cpu));

			/* Now transfer from this node to all the others. */
			for (dest_node = 0; dest_node < numnodes;
			     dest_node++) {
				for (xfer_lines = tm_min_lines;
				     xfer_lines <= tm_max_lines;) {

					brt_time_xfer(dest_node,
						      tm_iterations,
						      xfer_lines);

					/* Handle a min of 0 */
					if (xfer_lines < 1) {
						xfer_lines = 1;
					} else {
						xfer_lines *= 2;
					}
				}
			}
		}
	}
	return (0);
}


/*
 * Transfer the bte_test_buffer from our node to the specified
 * destination and print out timing results.
 */
static void
brt_time_xfer(int dest_node, int iterations, int xfer_lines)
{
	int iteration;
	char *src, *dst;
	u64 xfer_len, src_phys, dst_phys;
	u64 itc_before, itc_after, mem_intvl, bte_intvl;


	xfer_len = xfer_lines * L1_CACHE_BYTES;

	src = nodepda->bte_if[0].bte_test_buf;
	src_phys = __pa(src);
	dst = NODEPDA(dest_node)->bte_if[1].bte_test_buf;
	dst_phys = __pa(dst);
	mem_intvl = 0;

	for (iteration = 0; iteration < iterations; iteration++) {
		if (tm_memcpy) {
			itc_before = ia64_get_itc();
			memcpy(dst, src, xfer_len);
			itc_after = ia64_get_itc();
			mem_intvl = itc_after - itc_before;
		}

		itc_before = ia64_get_itc();
		bte_copy(src_phys, dst_phys, xfer_len, BTE_NOTIFY, NULL);
		itc_after = ia64_get_itc();
		bte_intvl = itc_after - itc_before;

		if (tm_memcpy) {
			printk("%3d,%3d,%3d,%5d,%4ld,%7ld,%3ld,"
			       "%7ld,%7ld,%7ld\n",
			       smp_processor_id(), NASID_GET(src),
			       NASID_GET(dst), xfer_lines,
			       NSEC(bte_setup_time),
			       NSEC(bte_transfer_time),
			       NSEC(bte_tear_down_time),
			       NSEC(bte_execute_time), NSEC(bte_intvl),
			       NSEC(mem_intvl));
		} else {
			printk("%3d,%3d,%3d,%5d,%4ld,%7ld,%3ld,"
			       "%7ld,%7ld\n",
			       smp_processor_id(), NASID_GET(src),
			       NASID_GET(dst), xfer_lines,
			       NSEC(bte_setup_time),
			       NSEC(bte_transfer_time),
			       NSEC(bte_tear_down_time),
			       NSEC(bte_execute_time), NSEC(bte_intvl));
		}
	}

}


/***********************************************************************
 * Notification Hang Test. -- NOTE: Has never actually caused a hang.
 *
 *	The next set of code checks to see if the Notification Hang
 *	occurs.  It does this by starting one thread per cpu, pinning
 *	the thread to its assigned cpu.  After it is pinned, we lock
 *	the associated bte.  Source, Dest, and Notification are
 *	assigned.
 *
 *	Inside of a loop, we set the length and trigger the
 *	transfer. We use the ITC to determine when the transfer should
 *	complete. Whenever the IBLS_BUSY bit is cleared, the transfer
 *	has completed. We occasionally call schedule (Since all CPUs
 *	have a pinned process The machine will be doing nothing but
 *	out tranfers) and loop.
 *
 *	If twice max normal time has passed without seeing the
 *	notification, we check the Length/Status register to see if
 *	IBLS_BUSY is still asserted and length is zero.  This is an
 *	indication of the hang.
 *
 **********************************************************************/


/*
 * Launch one thread per cpu.  When all threads are started, sleep
 * the specified timeout and then notify the other threads that it
 * is time to exit.
 */
static int
brt_tst_notify_hang(void)
{
	int tst_cpu;

	printk("Waiting %d seconds to complete test.\n", hang_timeout);

	atomic_set(&brt_thread_cnt, 0);
	brt_exit_flag = 0;

	for (tst_cpu = 0; tst_cpu < smp_num_cpus; tst_cpu++) {
		if ((kernel_thread(brt_notify_thrd,
				   (void *)(long)tst_cpu, 0)) < 0) {
			printk("Failed to start thread.\n");
		}
	}

	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(hang_timeout * HZ);
	set_current_state(TASK_RUNNING);

	printk("Flagging an exit.\n");
	brt_exit_flag = 1;

	while (atomic_read(&brt_thread_cnt)) {
		/* Wait until everyone else is done. */
		schedule();
	}

	printk("All threads have exited.\n");
	return (0);
}


/*
 * One of these threads is started per cpu.  Each thread is responsible
 * for loading that cpu's bte interface and then writing to the
 * test buffer.  The transfers are set in a round-robin fashion.
 * The end result is that each test buffer is being written into
 * by the previous node and both cpu's at the same time as the
 * local bte is transferring it to the next node.
 */
static int
brt_notify_thrd(void *__bind_cpu)
{
	int bind_cpu = (long int)__bind_cpu;
	int cpu = cpu_logical_map(bind_cpu);
	nodepda_t *nxt_node;
	long tmout_itc_intvls;
	long tmout;
	long passes;
	long good_xfer_cnt;
	u64 src_phys, dst_phys;
	int i;
	volatile char *src_buf;
	u64 *notify;

	atomic_inc(&brt_thread_cnt);
	daemonize();
	set_user_nice(current, 19);
	sigfillset(&current->blocked);

	/* Migrate to the right CPU */
	set_cpus_allowed(current, 1UL << cpu);

	/* Calculate the uSec timeout itc offset. */
	tmout_itc_intvls = local_cpu_data->cyc_per_usec * hang_usec;

	if (local_cnodeid() == (numnodes - 1)) {
		nxt_node = NODEPDA(0);
	} else {
		nxt_node = NODEPDA(local_cnodeid() + 1);
	}

	src_buf = nodepda->bte_if[0].bte_test_buf;
	src_phys = __pa(src_buf);
	dst_phys = __pa(nxt_node->bte_if[0].bte_test_buf);

	notify = kmalloc(L1_CACHE_BYTES, GFP_KERNEL);
	ASSERT(!((u64) notify & L1_CACHE_MASK));

	printk("BTE Hang %d xfer 0x%lx -> 0x%lx, Notify=0x%lx\n",
	       smp_processor_id(), src_phys, dst_phys, (u64) notify);

	passes = 0;
	good_xfer_cnt = 0;

	/* Loop until signalled to exit. */
	while (!brt_exit_flag) {
		/*
		 * A hang will prevent further transfers.
		 * NOTE: Sometimes, it appears like a hang occurred and
		 * then transfers begin again.  This just means that
		 * there is NUMA congestion and the hang_usec param
		 * should be increased.
		 */
		if (!(*notify & IBLS_BUSY)) {
			if ((bte_copy(src_phys,
				      dst_phys,
				      4UL * L1_CACHE_BYTES,
				      BTE_NOTIFY,
				      (void *)notify)) != BTE_SUCCESS) {
				printk("<0>Cpu %d Could not "
				       "allocate a bte.\n",
				       smp_processor_id());
				continue;
			}

			tmout = ia64_get_itc() + tmout_itc_intvls;

			while ((*notify & IBLS_BUSY) &&
			       (ia64_get_itc() < tmout)) {


				/* Push data out with the processor. */
				for (i = 0; i < (4 * L1_CACHE_BYTES);
				     i += L1_CACHE_BYTES) {
					src_buf[i] = (passes % 128);
				}
			};

			if (*notify & IBLS_BUSY) {
				printk("<0>Cpu %d BTE appears to have "
				       "hung.\n", smp_processor_id());
			} else {
				good_xfer_cnt++;
			}
		}

		/* Every x passes, take a little break. */
		if (!(++passes % 40)) {
			passes = 0;
			schedule_timeout(0.01 * HZ);
		}
	}

	kfree(notify);

	printk("Cpu %d had %ld good passes\n",
	       smp_processor_id(), good_xfer_cnt);

	atomic_dec(&brt_thread_cnt);
	return (0);
}


/***********************************************************************
 * Invalid Transfer Test.
 * 
 *	Just transfer from the local node to a nasid which does not
 *	exist.
 *
 *	>>> Potential Problem: on SN1, HUB interrupt doesn't always
 *	occurr.
 *
 **********************************************************************/


/*
 * Locate a nasid which doesn't exist.  Perform a bte_copy from that
 * node to our local node.
 */
static int
brt_tst_invalid_xfers(void)
{
	int i;
	int free_nasid = -1;
	int cpu;
	int error_cnt;

	u64 ret_code;

	if (ix_srcnasid != -1) {
		free_nasid = ix_srcnasid;
	} else {
		/* Only looking for nasids from C-Nodes. */
		for (i = 0; i < PLAT_MAX_NODE_NUMBER; i += 2) {
			if (local_node_data->physical_node_map[i] == -1) {
				free_nasid = i;
				break;
			}
		}
	}

	if (free_nasid == -1) {
		printk("tst_invalid_xfers: No free nodes found. "
		       "Exiting.\n");
		return (0);
	}

	printk("tst_invalid_xfers: Using source nasid of %d\n",
	       free_nasid);

	error_cnt = 0;

	for (i = 0; i < ix_iterations; i++) {
		if (verbose >= 1) {
			printk("-------------------------------"
			       "-------------------------------"
			       "--------------\n");
		}
		if ((verbose >= 1) || !(i % 10)) {
			printk("  Loop %d\n", i);
		}

		for (cpu = 0; cpu < smp_num_cpus; cpu++) {
			set_cpus_allowed(current, (1UL << cpu));

			if (verbose > 1) {
				printk("Testing with CPU %d\n",
				       smp_processor_id());
			}

			/* >>> Need a better means of calculating a
			 * remote addr. */
			ret_code = bte_copy(TO_NODE(free_nasid, 0),
					    __pa(nodepda->bte_if[0].
						 bte_test_buf),
					    4 * L1_CACHE_BYTES,
					    BTE_NOTIFY,
					    NULL);
			error_cnt += (ret_code ? 1 : 0);
		}
	}

	ret_code = ((error_cnt != (ix_iterations * smp_num_cpus)) ?
		    1 : 0);
	return (ret_code);
}


/***********************************************************************
 * Kernel command line handler.
 * 
 **********************************************************************/


#if !defined(MODULE)
static int __init
brt_setup(char *str)
{
	int cur_val;

	if (get_option(&str, &cur_val)) {
		selected_tests = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		verbose = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		hang_timeout = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		hang_usec = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		tm_min_lines = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		tm_max_lines = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		tm_iterations = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		tm_alternate = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		tm_memcpy = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		ix_iterations = cur_val;
	}
	if (get_option(&str, &cur_val)) {
		ix_srcnasid = cur_val;
	}

	return (1);
}
#endif				/* !defined(MODULE) */


/***********************************************************************
 * Module parameters.
 *	
 *	The two supported cases are loadable module parms and kernel
 *	command line support.
 *	
 *	The loadable module options are specified below in the
 *	MODULE_PARM macros and have associated descriptions.
 *	
 *	The kernel command line option is btetest=x[,y[,z]] etc.  The
 *	individual setting order is constant.  NOTE: The btetest flag
 *	is checked for in the bte_init_node function.
 *	
 **********************************************************************/


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Silicon Graphics, Inc.");
MODULE_DESCRIPTION("Test the Block Transfer Engine(BTE) "
		   "present on SGI machines.");

MODULE_PARM(selected_tests, "1i");
MODULE_PARM_DESC(selected_tests, "Bitmask of tests to run.");

MODULE_PARM(verbose, "1i");
MODULE_PARM_DESC(verbose,
		 "How much information should be "
		 "printed during the tests.");

MODULE_PARM(hang_timeout, "1i");
MODULE_PARM_DESC(hang_timeout,
		 "Number of seconds to wait for the Bte "
		 "Notification Failure.");

MODULE_PARM(hang_usec, "1i");
MODULE_PARM_DESC(hang_usec,
		 "Number of micro-seconds to wait for the 4-line Bte "
		 "transfer to complete.");

MODULE_PARM(tm_min_lines, "1i");
MODULE_PARM_DESC(tm_min_lines, "Minimum number of cache lines"
		 " to time with.");

MODULE_PARM(tm_max_lines, "1i");
MODULE_PARM_DESC(tm_max_lines, "Maximum number of cache lines"
		 " to time with.");

MODULE_PARM(tm_iterations, "1i");
MODULE_PARM_DESC(tm_iterations, "Rerun each timed transfer this "
		 "many times.");

MODULE_PARM(tm_alternate, "1i");
MODULE_PARM_DESC(tm_alternate, "Cycle across cpus between each "
		 "iteration");

MODULE_PARM(tm_memcpy, "1i");
MODULE_PARM_DESC(tm_memcpy, "Use memcpy as a comparison to BTE");

MODULE_PARM(ix_iterations, "1i");
MODULE_PARM_DESC(ix_iterations, "Rerun each transfer from an "
		 "invalid nasid this many times.");

MODULE_PARM(ix_srcnasid, "1i");
MODULE_PARM_DESC(ix_srcnasid, "Nasid to attempt xfer from.");


#if !defined(MODULE)
__setup("btetest=", brt_setup);
#endif				/* !defined(MODULE) */


module_init(brt_test_init);
module_exit(brt_test_exit);
