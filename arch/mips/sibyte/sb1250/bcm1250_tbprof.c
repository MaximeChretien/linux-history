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

#define SBPROF_TB_DEBUG 0

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/reboot.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/uaccess.h>
#include <asm/smplock.h>
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_scd.h>
#include <asm/sibyte/sb1250_int.h>
#include <asm/sibyte/64bit.h>
#include "bcm1250_tbprof.h"

#define DEVNAME "sb1250_tbprof"

static struct sbprof_tb *sbp;

/************************************************************************
 * Support for ZBbus sampling using the trace buffer
 *
 * We use the SCD performance counter interrupt, caused by a Zclk counter
 * overflow, to trigger the start of tracing.
 *
 * We set the trace buffer to sample everything and freeze on
 * overflow.
 *
 * We map the interrupt for trace_buffer_freeze to handle it on CPU 0.
 * 
 ************************************************************************/

/* Once per second on a 500 Mhz 1250 */
#define TB_PERIOD 250000000ULL

static void arm_tb(void)
{
	unsigned long long next = (1ULL << 40) - TB_PERIOD;
	/* Generate an SCD_PERFCNT interrupt in TB_PERIOD Zclks to
	   trigger start of trace.  XXX vary sampling period */
	out64(0, KSEG1 + A_SCD_PERF_CNT_1);
	out64(in64(KSEG1 + A_SCD_PERF_CNT_CFG) | // keep counters 0,2,3 as is
		   M_SPC_CFG_ENABLE |		 // enable counting
		   V_SPC_CFG_SRC1(1),		 // counter 1 counts cycles
	      KSEG1 + A_SCD_PERF_CNT_CFG);
	out64(next, KSEG1 + A_SCD_PERF_CNT_1);
	/* Reset the trace buffer */
	out64(M_SCD_TRACE_CFG_RESET, KSEG1 + A_SCD_TRACE_CFG);
	out64(M_SCD_TRACE_CFG_FREEZE_FULL, KSEG1 + A_SCD_TRACE_CFG);
	sbp->tb_armed = 1;
}

static void sbprof_tb_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	int i;
	DBG(printk(DEVNAME ": tb_intr\n"));
	if (sbp->next_tb_sample < MAX_TB_SAMPLES) {
		/* XXX should use XKPHYS to make writes bypass L2 */
		unsigned long long *p = sbp->sbprof_tbbuf[sbp->next_tb_sample++];
		/* Read out trace */
		out64(M_SCD_TRACE_CFG_START_READ, KSEG1 + A_SCD_TRACE_CFG);
		__asm__ __volatile__ ("sync" : : : "memory");
		/* Loop runs backwards because bundles are read out in reverse order */
		for (i = 256 * 6; i > 0; i -= 6) {
			// Subscripts decrease to put bundle in the order
			//   t0 lo, t0 hi, t1 lo, t1 hi, t2 lo, t2 hi
			p[i-1] = in64(KSEG1 + A_SCD_TRACE_READ); // read t2 hi
			p[i-2] = in64(KSEG1 + A_SCD_TRACE_READ); // read t2 lo
			p[i-3] = in64(KSEG1 + A_SCD_TRACE_READ); // read t1 hi
			p[i-4] = in64(KSEG1 + A_SCD_TRACE_READ); // read t1 lo
			p[i-5] = in64(KSEG1 + A_SCD_TRACE_READ); // read t0 hi
			p[i-6] = in64(KSEG1 + A_SCD_TRACE_READ); // read t0 lo
		}
		if (!sbp->tb_enable) {
			DBG(printk(DEVNAME ": tb_intr shutdown\n"));
			out64(M_SCD_TRACE_CFG_RESET, KSEG1 + A_SCD_TRACE_CFG);
			sbp->tb_armed = 0;
			wake_up(&sbp->tb_sync);
		} else {
			arm_tb();	// knock down current interrupt and get another one later
		}
	} else {
		/* No more trace buffer samples */
		DBG(printk(DEVNAME ": tb_intr full\n"));
		out64(M_SCD_TRACE_CFG_RESET, KSEG1 + A_SCD_TRACE_CFG);
		sbp->tb_armed = 0;
		if (!sbp->tb_enable) {
			wake_up(&sbp->tb_sync);
		}
	}
}

static void sbprof_pc_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	panic(DEVNAME ": pc_intr");
}

static int sbprof_zbprof_start(struct file *filp)
{
	if (sbp->tb_enable)
		return -EBUSY;

	DBG(printk(DEVNAME ": starting\n"));

	sbp->tb_enable = 1;
	sbp->next_tb_sample = 0;
	filp->f_pos = 0;

	if (request_irq
	    (K_INT_TRACE_FREEZE, sbprof_tb_intr, 0, "sbprof_tb trace freeze", sbp)) {
		return -EBUSY;
	}
	if (request_irq
	    (K_INT_PERF_CNT, sbprof_pc_intr, 0, "sbprof_tb scd perfcnt", sbp)) {
		free_irq(K_INT_TRACE_FREEZE, sbp);
		return -EBUSY;
	}

	/* I need the core to mask these, but the interrupt mapper to
	   pass them through.  I am exploiting my knowledge that
	   cp0_status masks out IP[5]. krw */
	out64(K_INT_MAP_I3,
	      KSEG1 + A_IMR_REGISTER(0, R_IMR_INTERRUPT_MAP_BASE) + (K_INT_PERF_CNT<<3));

	/* Initialize address traps */
	out64(0, KSEG1 + A_ADDR_TRAP_UP_0);
	out64(0, KSEG1 + A_ADDR_TRAP_UP_1);
	out64(0, KSEG1 + A_ADDR_TRAP_UP_2);
	out64(0, KSEG1 + A_ADDR_TRAP_UP_3);
	
	out64(0, KSEG1 + A_ADDR_TRAP_DOWN_0);
	out64(0, KSEG1 + A_ADDR_TRAP_DOWN_1);
	out64(0, KSEG1 + A_ADDR_TRAP_DOWN_2);
	out64(0, KSEG1 + A_ADDR_TRAP_DOWN_3);
	
	out64(0, KSEG1 + A_ADDR_TRAP_CFG_0);
	out64(0, KSEG1 + A_ADDR_TRAP_CFG_1);
	out64(0, KSEG1 + A_ADDR_TRAP_CFG_2);
	out64(0, KSEG1 + A_ADDR_TRAP_CFG_3);
	
	/* Initialize Trace Event 0-7 */
	//				when interrupt
	out64(M_SCD_TREVT_INTERRUPT, KSEG1 + A_SCD_TRACE_EVENT_0);
	out64(0, KSEG1 + A_SCD_TRACE_EVENT_1);
	out64(0, KSEG1 + A_SCD_TRACE_EVENT_2);
	out64(0, KSEG1 + A_SCD_TRACE_EVENT_3);
	out64(0, KSEG1 + A_SCD_TRACE_EVENT_4);
	out64(0, KSEG1 + A_SCD_TRACE_EVENT_5);
	out64(0, KSEG1 + A_SCD_TRACE_EVENT_6);
	out64(0, KSEG1 + A_SCD_TRACE_EVENT_7);
	
	/* Initialize Trace Sequence 0-7 */
	//				     Start on event 0 (interrupt)
	out64(V_SCD_TRSEQ_FUNC_START|0x0fff,
	      KSEG1 + A_SCD_TRACE_SEQUENCE_0);
	//			  dsamp when d used | asamp when a used
	out64(M_SCD_TRSEQ_ASAMPLE|M_SCD_TRSEQ_DSAMPLE|K_SCD_TRSEQ_TRIGGER_ALL,
	      KSEG1 + A_SCD_TRACE_SEQUENCE_1);
	out64(0, KSEG1 + A_SCD_TRACE_SEQUENCE_2);
	out64(0, KSEG1 + A_SCD_TRACE_SEQUENCE_3);
	out64(0, KSEG1 + A_SCD_TRACE_SEQUENCE_4);
	out64(0, KSEG1 + A_SCD_TRACE_SEQUENCE_5);
	out64(0, KSEG1 + A_SCD_TRACE_SEQUENCE_6);
	out64(0, KSEG1 + A_SCD_TRACE_SEQUENCE_7);

	/* Now indicate the PERF_CNT interrupt as a trace-relevant interrupt */
	out64((1ULL << K_INT_PERF_CNT), KSEG1 + A_IMR_REGISTER(0, R_IMR_INTERRUPT_TRACE));

	arm_tb();

	DBG(printk(DEVNAME ": done starting\n"));

	return 0;
}

static int sbprof_zbprof_stop(void)
{
	DBG(printk(DEVNAME ": stopping\n"));

	if (sbp->tb_enable) {
		sbp->tb_enable = 0;
		/* XXXKW there is a window here where the intr handler
		   may run, see the disable, and do the wake_up before
		   this sleep happens. */
		if (sbp->tb_armed) {
			DBG(printk(DEVNAME ": wait for disarm\n"));
			interruptible_sleep_on(&sbp->tb_sync);
			DBG(printk(DEVNAME ": disarm complete\n"));
		}
		free_irq(K_INT_TRACE_FREEZE, sbp);
		free_irq(K_INT_PERF_CNT, sbp);
	}

	DBG(printk(DEVNAME ": done stopping\n"));

	return 0;
}

static int sbprof_tb_open(struct inode *inode, struct file *filp)
{
	int minor;

	minor = MINOR(inode->i_rdev);
	if (minor != 0) {
		return -ENODEV;
	}
	if (sbp != NULL) {
		return -EBUSY;
	}

	/* XXXKW spinlock? */
	sbp = kmalloc(sizeof(struct sbprof_tb), GFP_KERNEL);
	if (!sbp) {
		return -ENOMEM;
	}
	memset(sbp, 0, sizeof(struct sbprof_tb));
	sbp->sbprof_tbbuf = vmalloc(MAX_TBSAMPLE_BYTES);
	if (!sbp->sbprof_tbbuf) {
		kfree(sbp);
		sbp = NULL;
		return -ENOMEM;
	}
	memset(sbp->sbprof_tbbuf, 0, MAX_TBSAMPLE_BYTES);
	init_waitqueue_head(&sbp->tb_sync);

	return 0;
}

static int sbprof_tb_release(struct inode *inode, struct file *filp)
{
	int minor;
	
	minor = MINOR(inode->i_rdev);
	if (minor != 0 || sbp == NULL) {
		return -ENODEV;
	}

	if (sbp->tb_armed || sbp->tb_enable) {
		sbprof_zbprof_stop();
	}

	vfree(sbp->sbprof_tbbuf);
	kfree(sbp);
	sbp = NULL;

	return 0;
}

static ssize_t sbprof_tb_read(struct file *filp, char *buf, 
			   size_t size, loff_t *offp)
{
	int cur_sample, sample_off, cur_count, sample_left;
	char *src;
	int   count   =	 0;
	char *dest    =	 buf;
	long  cur_off = *offp;

	count = 0;
	cur_sample = cur_off / TB_SAMPLE_SIZE;
	sample_off = cur_off % TB_SAMPLE_SIZE;
	sample_left = TB_SAMPLE_SIZE - sample_off;
	while (size && (cur_sample < sbp->next_tb_sample)) {
		cur_count = size < sample_left ? size : sample_left;
		src = (char *)(((long)sbp->sbprof_tbbuf[cur_sample])+sample_off);
		copy_to_user(dest, src, cur_count);
		DBG(printk(DEVNAME ": read from sample %d, %d bytes\n", cur_sample, cur_count));
		size -= cur_count;
		sample_left -= cur_count;
		if (!sample_left) {
			cur_sample++;
			sample_off = 0;
			sample_left = TB_SAMPLE_SIZE;
		} else {
			sample_off += cur_count;
		}
		cur_off += cur_count;
		dest += cur_count;
		count += cur_count;
	}
	*offp = cur_off;

	return count;
}

#define SBPROF_ZBSTART	_IOW('s', 0, int)
#define SBPROF_ZBSTOP	_IOW('s', 1, int)
#define SBPROF_ZBFULL	_IOW('s', 2, int)

static int sbprof_tb_ioctl(struct inode *inode, 
			struct file *filp,
			unsigned int command, 
			unsigned long arg)
{
	int error = 0;
	int full;

	switch (command) {
	case SBPROF_ZBSTART:
		error = sbprof_zbprof_start(filp);
		break;
	case SBPROF_ZBSTOP:
		error = sbprof_zbprof_stop();
		break;
	case SBPROF_ZBFULL:
		full = (sbp->next_tb_sample == MAX_TB_SAMPLES);
		return put_user(full, (int *) arg);
	default:
		error = -EINVAL;
		break;
	}

	return error;
}

static struct file_operations sbprof_tb_fops = {
	owner:		THIS_MODULE,
	open:		sbprof_tb_open,
	release:	sbprof_tb_release,
	read:		sbprof_tb_read,
	ioctl:		sbprof_tb_ioctl,
	mmap:		NULL,
};

static devfs_handle_t devfs_handle;

static int __init sbprof_tb_init(void) 
{
	if (devfs_register_chrdev(SBPROF_TB_MAJOR, DEVNAME, &sbprof_tb_fops)) {
		printk(KERN_WARNING DEVNAME ": initialization failed (dev %d)\n",
		       SBPROF_TB_MAJOR);
		return -EIO;
	}
	devfs_handle = devfs_register(NULL, DEVNAME,
				      DEVFS_FL_DEFAULT, SBPROF_TB_MAJOR, 0,
				      S_IFCHR | S_IRUGO | S_IWUGO,
				      &sbprof_tb_fops, NULL);
	sbp = NULL;
	printk(KERN_INFO DEVNAME ": initialized\n");
	return 0;
}

static void __exit sbprof_tb_cleanup(void)
{
	devfs_unregister_chrdev(SBPROF_TB_MAJOR, DEVNAME);
	devfs_unregister(devfs_handle);
}

module_init(sbprof_tb_init);
module_exit(sbprof_tb_cleanup);
