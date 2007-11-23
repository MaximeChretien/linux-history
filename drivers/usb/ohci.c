/*
 * Open Host Controller Interface driver for USB.
 *
 * (C) Copyright 1999 Gregory P. Smith <greg@electricrain.com>
 *
 * This is the "other" host controller interface for USB.  You will
 * find this on many non-Intel based motherboards, and of course the
 * Mac.  As Linus hacked his UHCI driver together first, I modeled
 * this after his.. (it should be obvious)
 *
 * From the programming standpoint the OHCI interface seems a little
 * prettier and potentially less CPU intensive.  This remains to be
 * proven.  In reality, I don't believe it'll make one darn bit of
 * difference.  USB v1.1 is a slow bus by today's standards.
 *
 * OHCI hardware takes care of most of the scheduling of different
 * transfer types with the correct prioritization for us.
 *
 * To get started in USB, I used the "Universal Serial Bus System
 * Architecture" book by Mindshare, Inc.  It was a reasonable introduction
 * and overview of USB and the two dominant host controller interfaces
 * however you're better off just reading the real specs available
 * from www.usb.org as you'll need them to get enough detailt to
 * actually implement a HCD.  The book has many typos and omissions
 * Beware, the specs are the victim of a committee.
 *
 * This code was written with Guinness on the brain, xsnow on the desktop
 * and Orbital, Orb, Enya & Massive Attack on the CD player.  What a life!  ;) 
 *
 * No filesystems were harmed in the development of this code.
 *
 * $Id: ohci.c,v 1.43 1999/05/16 22:35:24 greg Exp $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/malloc.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>

#include <asm/spinlock.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include "ohci.h"

#ifdef CONFIG_APM
#include <linux/apm_bios.h>
static int handle_apm_event(apm_event_t event);
static int apm_resume = 0;
#endif

static DECLARE_WAIT_QUEUE_HEAD(ohci_configure);

#ifdef CONFIG_USB_OHCI_DEBUG
#define OHCI_DEBUG    /* to make typing it easier.. */
#endif

int MegaDebug = 0;	/* SIGUSR2 to the control thread toggles this */


#ifdef OHCI_TIMER
static struct timer_list ohci_timer;	/* timer for root hub polling */
#endif

static spinlock_t ohci_edtd_lock = SPIN_LOCK_UNLOCKED;

/*
 * Add a TD to the end of the TD list on a given ED.  This function
 * does NOT advance the ED's tail_td pointer beyond the given TD.  To
 * add multiple TDs, call this function once for each TD.  Do not
 * "simply" update tail_td yourself... This function does more than
 * that.
 * 
 * If this ED is on the controller, you MUST set its SKIP flag before
 * calling this function.
 *
 * Important!  This function needs locking and atomicity as it works
 * in parallel with the HC's DMA.  Locking ohci_edtd_lock while using
 * the function is a must.
 *
 * This function can be called by the interrupt handler.
 */
static void ohci_add_td_to_ed(struct ohci_td *td, struct ohci_ed *ed)
{
	struct ohci_td *dummy_td, *prev_td;

	if (ed->tail_td == 0) {
		printk("eek! an ED without a dummy_td\n");
	}

	/* The ED's tail_td is constant, always pointing to the
	 * dummy_td.  The reason the ED never processes the dummy is
	 * that it stops processing TDs as soon as head_td == tail_td.
	 * When it advances to this last dummy TD it conveniently stops. */
	dummy_td = bus_to_virt(ed->tail_td);

	/* Dummy's data pointer is used to point to the previous TD */
	if (ed_head_td(ed) != ed->tail_td) {
		prev_td = (struct ohci_td *) dummy_td->data;
	} else {
		/* if the ED is empty, previous is meaningless */
		/* We'll be inserting into the head of the list */
		prev_td = NULL;
	}

	/* Store the new back pointer and set up this TD's next */
	dummy_td->data = td;
	td->next_td = ed->tail_td;

	/* Store the TD pointer back to the ED */
	td->ed = ed;

	if (!prev_td) { /* No previous TD? then insert us at the head */
		if (ed_head_td(ed) != ed->tail_td)
			printk(KERN_DEBUG "Suspicious ED...\n");
		set_ed_head_td(ed, virt_to_bus(td));	/* put it on the ED */
	} else {
		/* add the TD to the end */
		prev_td->next_td = virt_to_bus(td);
	}
} /* ohci_add_td_to_ed() */


inline void ohci_start_control(struct ohci *ohci)
{
	/* tell the HC to start processing the control list */
	writel_set(OHCI_USB_CLE, &ohci->regs->control);
	writel_set(OHCI_CMDSTAT_CLF, &ohci->regs->cmdstatus);
}

inline void ohci_start_bulk(struct ohci *ohci)
{
	/* tell the HC to start processing the bulk list */
	writel_set(OHCI_USB_BLE, &ohci->regs->control);
	writel_set(OHCI_CMDSTAT_BLF, &ohci->regs->cmdstatus);
}

inline void ohci_start_periodic(struct ohci *ohci)
{
	/* enable processing periodic (intr) transfers starting next frame */
	writel_set(OHCI_USB_PLE, &ohci->regs->control);
}

inline void ohci_start_isoc(struct ohci *ohci)
{
	/* enable processing isoc. transfers starting next frame */
	writel_set(OHCI_USB_IE, &ohci->regs->control);
}

/*
 * Add an ED to the hardware register ED list pointed to by hw_listhead_p
 * This function only makes sense for Control and Bulk EDs.
 */
static void ohci_add_ed_to_hw(struct ohci_ed *ed, void* hw_listhead_p)
{
	__u32 listhead;
	unsigned long flags;

	spin_lock_irqsave(&ohci_edtd_lock, flags);
	
	listhead = readl(hw_listhead_p);

	/* if the list is not empty, insert this ED at the front */
	/* XXX should they go on the end? */
	if (listhead) {
		ed->next_ed = listhead;
	}

	/* update the hardware listhead pointer */
	writel(virt_to_bus(ed), hw_listhead_p);

	spin_unlock_irqrestore(&ohci_edtd_lock, flags);
} /* ohci_add_ed_to_hw() */


/*
 *  Put a control ED on the controller's list
 */
void ohci_add_control_ed(struct ohci *ohci, struct ohci_ed *ed)
{
	ohci_add_ed_to_hw(ed, &ohci->regs->ed_controlhead);
	ohci_start_control(ohci);
} /* ohci_add_control_ed() */

/*
 *  Put a bulk ED on the controller's list
 */
void ohci_add_bulk_ed(struct ohci *ohci, struct ohci_ed *ed)
{
	ohci_add_ed_to_hw(ed, &ohci->regs->ed_bulkhead);
	ohci_start_bulk(ohci);
} /* ohci_add_bulk_ed() */

/*
 *  Put a periodic ED on the appropriate list given the period.
 */
void ohci_add_periodic_ed(struct ohci *ohci, struct ohci_ed *ed, int period)
{
	struct ohci_ed *int_ed;
	struct ohci_device *root_hub=usb_to_ohci(ohci->bus->root_hub);
	unsigned long flags;

	/*
	 * Pick a good frequency endpoint based on the requested period
	 */
	int_ed = &root_hub->ed[ms_to_ed_int(period)];
#ifdef OHCI_DEBUG
	printk("usb-ohci: Using INT ED queue %d for %dms period\n",
			ms_to_ed_int(period), period);
#endif

	spin_lock_irqsave(&ohci_edtd_lock, flags);
	/*
	 * Insert this ED at the front of the list.
	 */
	ed->next_ed = int_ed->next_ed;
	int_ed->next_ed = virt_to_bus(ed);

	spin_unlock_irqrestore(&ohci_edtd_lock, flags);

	ohci_start_periodic(ohci);
} /* ohci_add_periodic_ed() */

/*
 *  Put an isochronous ED on the controller's list
 */
inline void ohci_add_isoc_ed(struct ohci *ohci, struct ohci_ed *ed)
{
	ohci_add_periodic_ed(ohci, ed, 1);
}


/*
 * This will be used for the interrupt to wake us up on the next SOF
 */
DECLARE_WAIT_QUEUE_HEAD(start_of_frame_wakeup);

/*
 * Guarantee that an ED is safe to be modified by the HCD (us).
 *
 * This function can NOT be called from an interrupt.
 */
void ohci_wait_for_ed_safe(struct ohci_regs *regs, struct ohci_ed *ed, int ed_type)
{
	__u32 hw_listcurrent;

	/* tell the controller to skip this ED */
	ed->status |= OHCI_ED_SKIP;

	switch (ed_type) {
	case HCD_ED_CONTROL:
		hw_listcurrent = readl(regs->ed_controlcurrent);
		break;
	case HCD_ED_BULK:
		hw_listcurrent = readl(regs->ed_bulkcurrent);
		break;
	case HCD_ED_ISOC:
	case HCD_ED_INT:
		hw_listcurrent = readl(regs->ed_periodcurrent);
		break;
	default:
		return;
	}

	/* 
	 * If the HC is processing this ED we need to wait until the
	 * at least the next frame.
	 */
	if (virt_to_bus(ed) == hw_listcurrent) {
		DECLARE_WAITQUEUE(wait, current);

#ifdef OHCI_DEBUG
		printk("Waiting a frame for OHC to finish with ED %p\n", ed);
#endif

		add_wait_queue(&start_of_frame_wakeup, &wait);

		/* clear the SOF interrupt status and enable it */
		writel(OHCI_INTR_SF, &regs->intrstatus);
		writel(OHCI_INTR_SF, &regs->intrenable);

		schedule_timeout(HZ/10);

		remove_wait_queue(&start_of_frame_wakeup, &wait);
	}

	return; /* The ED is now safe */
} /* ohci_wait_for_ed_safe() */


/*
 *  Remove an ED from the HC's list.
 *  This function can ONLY be used for Control or Bulk EDs.
 *  
 *  Note that the SKIP bit is left on in the removed ED.
 */
void ohci_remove_norm_ed_from_hw(struct ohci *ohci, struct ohci_ed *ed, int ed_type)
{
	unsigned long flags;
	struct ohci_regs *regs = ohci->regs;
	struct ohci_ed *cur;
	__u32 bus_ed = virt_to_bus(ed);
	__u32 bus_cur;
	__u32 *hw_listhead_p;

	if (ed == NULL || !bus_ed)
		return;

	switch (ed_type) {
	case HCD_ED_CONTROL:
		hw_listhead_p = &regs->ed_controlhead;
		break;
	case HCD_ED_BULK:
		hw_listhead_p = &regs->ed_bulkhead;
		break;
	default:
		printk("Unknown HCD ED type %d.\n", ed_type);
		return;
	}

	/*
	 * Tell the controller to this skip ED and make sure it is not the
	 * being accessed by the HC as we speak.
	 */
	ohci_wait_for_ed_safe(regs, ed, ed_type);

	bus_cur = readl(hw_listhead_p);

	if (bus_cur == 0)
		return;   /* the list is already empty */

	cur = bus_to_virt(bus_cur);

	spin_lock_irqsave(&ohci_edtd_lock, flags);

	/* if its the head ED, move the head */
	if (bus_cur == bus_ed) {
		writel(cur->next_ed, hw_listhead_p);
	} else if (cur->next_ed != 0) {
		struct ohci_ed *prev;

		/* walk the list and unlink the ED if found */
		do {
			prev = cur;
			cur = bus_to_virt(cur->next_ed);

			if (virt_to_bus(cur) == bus_ed) {
				/* unlink from the list */
				prev->next_ed = cur->next_ed;
				break;
			}
		} while (cur->next_ed != 0);
	}

	/* clear any links from the ED for safety */
	ed->next_ed = 0;

	spin_unlock_irqrestore(&ohci_edtd_lock, flags);
} /* ohci_remove_norm_ed_from_hw() */

/*
 *  Remove an ED from the controller's control list.  Note that the SKIP bit
 *  is left on in the removed ED.
 */
inline void ohci_remove_control_ed(struct ohci *ohci, struct ohci_ed *ed)
{
	ohci_remove_norm_ed_from_hw(ohci, ed, HCD_ED_CONTROL);
}

/*
 *  Remove an ED from the controller's bulk list.  Note that the SKIP bit
 *  is left on in the removed ED.
 */
inline void ohci_remove_bulk_ed(struct ohci *ohci, struct ohci_ed *ed)
{
	ohci_remove_norm_ed_from_hw(ohci, ed, HCD_ED_BULK);
}


/*
 *  Remove a TD from the given EDs TD list.
 */
static void ohci_remove_td_from_ed(struct ohci_td *td, struct ohci_ed *ed)
{
	unsigned long flags;
	struct ohci_td *head_td;

	if ((td == NULL) || (ed == NULL))
		return;

	spin_lock_irqsave(&ohci_edtd_lock, flags);

	if (ed_head_td(ed) == 0)
		return;

	/* set the "skip me bit" in this ED */
	ed->status |= OHCI_ED_SKIP;

	/* XXX Assuming this list will never be circular */

	head_td = bus_to_virt(ed_head_td(ed));
	if (virt_to_bus(td) == ed_head_td(ed)) {
		/* It's the first TD, remove it. */
		set_ed_head_td(ed, head_td->next_td);
	} else {
		struct ohci_td *prev_td, *cur_td;

		/* FIXME: collapse this into a nice simple loop :) */
		if (head_td->next_td != 0) {
			prev_td = head_td;
			cur_td = bus_to_virt(head_td->next_td);
			for (;;) {
				if (td == cur_td) {
					/* remove it */
					prev_td->next_td = cur_td->next_td;
					break;
				}
				if (cur_td->next_td == 0)
					break;
				prev_td = cur_td;
				cur_td = bus_to_virt(cur_td->next_td);
			}
		}
	}

	td->next_td = 0;  /* remove the TDs links */
	td->ed = NULL;

	/* return this TD to the pool of free TDs */
	ohci_free_td(td);

	/* unset the "skip me bit" in this ED */
	ed->status &= ~OHCI_ED_SKIP;

	spin_unlock_irqrestore(&ohci_edtd_lock, flags);
} /* ohci_remove_td_from_ed() */


/*
 * Get a pointer (virtual) to an available TD from the given device's
 * pool.  Return NULL if none are left.
 */
static struct ohci_td *ohci_get_free_td(struct ohci_device *dev)
{
	int idx;

#if 0
	printk(KERN_DEBUG "in ohci_get_free_td()\n");
#endif

	/* FIXME: this is horribly inefficient */
	for (idx=0; idx < NUM_TDS; idx++) {
#if 0
		show_ohci_td(&dev->td[idx]);
#endif
		if (!td_allocated(dev->td[idx])) {
			struct ohci_td *new_td = &dev->td[idx];
			/* zero out the TD */
			memset(new_td, 0, sizeof(*new_td));
			/* mark the new TDs as unaccessed */
			new_td->info = OHCI_TD_CC_NEW;
			/* mark it as allocated */
			allocate_td(new_td);
			return new_td;
		}
	}

	printk("usb-ohci: unable to allocate a TD\n");
	return NULL;
} /* ohci_get_free_td() */


/*
 * Get a pointer (virtual) to an available TD from the given device's
 * pool.  Return NULL if none are left.
 */
static struct ohci_ed *ohci_get_free_ed(struct ohci_device *dev)
{
	int idx;

	/* FIXME: this is horribly inefficient */
	for (idx=0; idx < NUM_EDS; idx++) {
		if (!ed_allocated(dev->ed[idx])) {
			struct ohci_ed *new_ed = &dev->ed[idx];
			/* zero out the ED */
			memset(new_ed, 0, sizeof(*new_ed));
			/* all new EDs start with the SKIP bit set */
			new_ed->status |= OHCI_ED_SKIP;
			/* mark it as allocated */
			allocate_ed(new_ed);
			return new_ed;
		}
	}

	printk("usb-ohci: unable to allocate an ED\n");
	return NULL;
} /* ohci_get_free_ed() */


void ohci_free_ed(struct ohci_ed *ed)
{
	if (!ed)
		return;

	if (ed->tail_td == 0) {
		printk("yikes! an ED without a dummy_td\n");
	} else
		ohci_free_td((struct ohci_td *)bus_to_virt(ed->tail_td));

	ed->status &= ~(__u32)ED_ALLOCATED;
} /* ohci_free_ed() */


/*
 *  Initialize a TD
 *
 * 	dir = OHCI_TD_D_IN, OHCI_TD_D_OUT, or OHCI_TD_D_SETUP
 * 	toggle = TOGGLE_AUTO, TOGGLE_DATA0, TOGGLE_DATA1
 */
inline struct ohci_td *ohci_fill_new_td(struct ohci_td *td, int dir, int toggle, __u32 flags, void *data, __u32 len, void *dev_id, usb_device_irq completed)
{
	/* hardware fields */
	td->info = OHCI_TD_CC_NEW |
		(dir & OHCI_TD_D) |
		(toggle & OHCI_TD_DT) |
		flags;
	td->cur_buf = (data == NULL) ? 0 : virt_to_bus(data);
	td->buf_end = (len == 0) ? 0 : td->cur_buf + len - 1;

	/* driver fields */
	td->data = data;
	td->dev_id = dev_id;
	td->completed = completed;

#if 0
	printk(KERN_DEBUG "ohci_fill_new_td created:\n");
	show_ohci_td(td);
#endif

	return td;
} /* ohci_fill_new_td() */


/*
 *  Initialize a new ED on device dev, including allocating and putting the
 *  dummy tail_td on its queue if it doesn't already have one.  Any
 *  TDs on this ED other than the dummy will be lost (so there better
 *  not be any!).  This assumes that the ED is Allocated and will
 *  force the Allocated bit on.
 */
struct ohci_ed *ohci_fill_ed(struct ohci_device *dev, struct ohci_ed *ed, int maxpacketsize, int lowspeed, int endp_id, int isoc_tds)
{
	struct ohci_td *dummy_td;

	if (ed_head_td(ed) != ed->tail_td)
		printk("Reusing a non-empty ED %p!\n", ed);

	if (!ed->tail_td) {
		dummy_td = ohci_get_free_td(dev);
		if (dummy_td == NULL) {
			printk("Error allocating dummy TD for ED %p\n", ed);
			return NULL;	/* no dummy available! */
		}
		make_dumb_td(dummy_td);	/* flag it as a dummy */
		ed->tail_td = virt_to_bus(dummy_td);
	} else {
		dummy_td = bus_to_virt(ed->tail_td);
		if (!td_dummy(*dummy_td))
			printk("ED %p's dummy %p is screwy\n", ed, dummy_td);
	}

	/* set the head TD to the dummy and clear the Carry & Halted bits */
	ed->_head_td = ed->tail_td;

	ed->status = \
		ed_set_maxpacket(maxpacketsize) |
		ed_set_speed(lowspeed) |
		(endp_id & 0x7ff) |
		((isoc_tds == 0) ? OHCI_ED_F_NORM : OHCI_ED_F_ISOC);
	allocate_ed(ed);
	ed->next_ed = 0;

	return ed;
} /* ohci_fill_ed() */


/**********************************
 * OHCI interrupt list operations *
 **********************************/

/*
 * Request an interrupt handler for one "pipe" of a USB device.
 * (this function is pretty minimal right now)
 *
 * At the moment this is only good for input interrupts. (ie: for a
 * mouse or keyboard)
 *
 * Period is desired polling interval in ms.  The closest, shorter
 * match will be used.  Powers of two from 1-32 are supported by OHCI.
 */
static int ohci_request_irq(struct usb_device *usb, unsigned int pipe,
	usb_device_irq handler, int period, void *dev_id)
{
	struct ohci_device *dev = usb_to_ohci(usb);
	struct ohci_td *td;
	struct ohci_ed *interrupt_ed;	/* endpoint descriptor for this irq */

	/* Get an ED and TD */
	interrupt_ed = ohci_get_free_ed(dev);
	if (!interrupt_ed) {
		printk("Out of EDs on device %p in ohci_request_irq\n", dev);
		return -1;
	}

	td = ohci_get_free_td(dev);
	if (!td) {
		printk("Out of TDs in ohci_request_irq\n");
		ohci_free_ed(interrupt_ed);
		return -1;
	}

	/*
	 * Set the max packet size, device speed, endpoint number, usb
	 * device number (function address), and type of TD.
	 */
	ohci_fill_ed(dev, interrupt_ed, usb_maxpacket(usb,pipe), usb_pipeslow(pipe),
		usb_pipe_endpdev(pipe), 0 /* normal TDs */);

	/* Fill in the TD */
	ohci_fill_new_td(td, td_set_dir_out(usb_pipeout(pipe)),
			TOGGLE_AUTO,
			OHCI_TD_ROUND,
			&dev->data, DATA_BUF_LEN,
			dev_id, handler);
	/*
	 * TODO: be aware of how the OHCI controller deals with DMA
	 * spanning more than one page.
	 */

	/*
	 *  Put the TD onto our ED and make sure its ready to run
	 */
	ohci_add_td_to_ed(td, interrupt_ed);
	interrupt_ed->status &= ~OHCI_ED_SKIP;
	ohci_unhalt_ed(interrupt_ed);

	/* Linus did this. see asm/system.h; scary concept... I don't
	 * know if its needed here or not but it won't hurt. */
	wmb();

	/* Assimilate the new ED into the collective */
	ohci_add_periodic_ed(dev->ohci, interrupt_ed, period);

	return 0;
} /* ohci_request_irq() */


/*
 * Control thread operations:
 */
static DECLARE_WAIT_QUEUE_HEAD(control_wakeup);

/*
 *  This is the handler that gets called when a control transaction
 *  completes.
 *
 *  This function is called from the interrupt handler.
 */
static int ohci_control_completed(int stats, void *buffer, void *dev_id)
{
	/* pass the TDs completion status back to control_msg */
	if (dev_id) {
		int *completion_status = (int *)dev_id;
		*completion_status = stats;
	}

	wake_up(&control_wakeup);
	return 0;
} /* ohci_control_completed() */


/*
 * Send or receive a control message on a "pipe"
 *
 * The cmd parameter is a pointer to the 8 byte setup command to be
 * sent.  FIXME:  This is a devrequest in usb.h.  The function
 * should be updated to accept a devrequest* instead of void*..
 *
 * A control message contains:
 *   - The command itself
 *   - An optional data phase (if len > 0)
 *   - Status complete phase
 *
 * This function can NOT be called from an interrupt.
 */
static int ohci_control_msg(struct usb_device *usb, unsigned int pipe, void *cmd, void *data, int len)
{
	struct ohci_device *dev = usb_to_ohci(usb);
	struct ohci_ed *control_ed = ohci_get_free_ed(dev);
	struct ohci_td *setup_td, *data_td, *status_td;
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	int completion_status = -1;

#ifdef OHCI_DEBUG 
	printk(KERN_DEBUG "ohci_control_msg %p (ohci_dev: %p) pipe %x, cmd %p, data %p, len %d\n", usb, dev, pipe, cmd, data, len);
#endif
	if (!control_ed) {
		printk("usb-ohci: couldn't get ED for dev %p\n", dev);
		return -1;
	}

	/* get a TD to send this control message with */
	setup_td = ohci_get_free_td(dev);
	if (!setup_td) {
		printk("usb-ohci: couldn't get TD for dev %p [cntl setup]\n", dev);
		ohci_free_ed(control_ed);
		return -1;
	}

	/*
	 * Set the max packet size, device speed, endpoint number, usb
	 * device number (function address), and type of TD.
	 *
	 */
	ohci_fill_ed(dev, control_ed, usb_maxpacket(usb,pipe), usb_pipeslow(pipe),
		usb_pipe_endpdev(pipe), 0 /* normal TDs */);

	/*
	 * Build the control TD
	 */

	/*
	 * Set the not accessed condition code, allow odd sized data,
	 * and set the data transfer type to SETUP.  Setup DATA always
	 * uses a DATA0 packet.
	 *
	 * The setup packet contains a devrequest (usb.h) which
	 * will always be 8 bytes long.  FIXME: the cmd parameter
	 * should be a pointer to one of these instead of a void* !!!
	 */
	ohci_fill_new_td(setup_td, OHCI_TD_D_SETUP, TOGGLE_DATA0,
			OHCI_TD_IOC_OFF,
			cmd, 8,		/* cmd is always 8 bytes long */
			NULL, NULL);

	/* allocate the next TD */
	data_td = ohci_get_free_td(dev);
	if (!data_td) {
		printk("usb-ohci: couldn't get TD for dev %p [cntl data]\n", dev);
		ohci_free_td(setup_td);
		ohci_free_ed(control_ed);
		return -1;
	}

	/* link to the next TD */
	setup_td->next_td = virt_to_bus(data_td);

	if (len > 0) {

		/* build the Control DATA TD, it starts with a DATA1. */
		ohci_fill_new_td(data_td, td_set_dir_out(usb_pipeout(pipe)),
				TOGGLE_DATA1,
				OHCI_TD_ROUND | OHCI_TD_IOC_OFF,
				data, len,
				NULL, NULL);

		/*
		 * TODO: Normal TDs can transfer up to 8192 bytes on OHCI.
		 * However, for that to happen, the data must -start-
		 * on a nice 4kb page.  We need to check for data
		 * sizes > 4096 and, if they cross more than two 4096
		 * byte pages of memory one or more additional TDs
		 * will need to be created.  (repeat doing this in a
		 * loop until all of the DATA is on a TD)
		 *
		 * Control transfers are -highly unlikely- to need to
		 * transfer this much data.. but who knows.. sadistic
		 * hardware is sure to exist.
		 */

		status_td = ohci_get_free_td(dev);  /* TODO check for NULL */
		if (!status_td) {
			printk("usb-ohci: couldn't get TD for dev %p [cntl status]\n", dev);
			ohci_free_td(setup_td);
			ohci_free_td(data_td);
			ohci_free_ed(control_ed);
			return -1;
		}

		data_td->next_td = virt_to_bus(status_td);
	} else {
		status_td = data_td; /* no data_td, use it for status */
	}

	/* The control status packet always uses a DATA1
	 * Give "dev_id" the address of completion_status so that the
	 * TDs status can be passed back to us from the IRQ. */
	ohci_fill_new_td(status_td,
			td_set_dir_in(usb_pipeout(pipe) | (len == 0)),
			TOGGLE_DATA1,
			0 /* flags */,
			NULL /* data */, 0 /* data len */,
			&completion_status, ohci_control_completed);
	status_td->next_td = 0; /* end of TDs */

	/*
	 * Add the chain of 2-3 control TDs to the control ED's TD list
	 */
	spin_lock_irqsave(&ohci_edtd_lock, flags);
	control_ed->status |= OHCI_ED_SKIP;
	ohci_add_td_to_ed(setup_td, control_ed);
	if (data_td != status_td)
		ohci_add_td_to_ed(data_td, control_ed);
	ohci_add_td_to_ed(status_td, control_ed);
	control_ed->status &= ~OHCI_ED_SKIP;
	ohci_unhalt_ed(control_ed);
	spin_unlock_irqrestore(&ohci_edtd_lock, flags);

#ifdef OHCI_DEBUG
	if (MegaDebug) {
	/* complete transaction debugging output (before) */
	printk(KERN_DEBUG " Control ED %lx:\n", virt_to_bus(control_ed));
	show_ohci_ed(control_ed);
	printk(KERN_DEBUG " Setup TD %lx:\n", virt_to_bus(setup_td));
	show_ohci_td(setup_td);
	if (data_td != status_td) {
		printk(KERN_DEBUG " Data TD %lx:\n", virt_to_bus(data_td));
		show_ohci_td(data_td);
	}
	printk(KERN_DEBUG " Status TD %lx:\n", virt_to_bus(status_td));
	show_ohci_td(status_td);
	printk(KERN_DEBUG " Controller Status:\n");
	show_ohci_status(dev->ohci);
	}
#endif

	/*
	 * Start the control transaction..
	 */
	current->state = TASK_UNINTERRUPTIBLE;
	add_wait_queue(&control_wakeup, &wait);

	/* Give the ED to the HC */
	ohci_add_control_ed(dev->ohci, control_ed);

	schedule_timeout(HZ/10);

	remove_wait_queue(&control_wakeup, &wait);

#ifdef OHCI_DEBUG
	if (MegaDebug) {
	/* complete transaction debugging output (after) */
	printk(KERN_DEBUG " *after* Control ED %lx:\n", virt_to_bus(control_ed));
	show_ohci_ed(control_ed);
	printk(KERN_DEBUG " *after* Setup TD %lx:\n", virt_to_bus(setup_td));
	show_ohci_td(setup_td);
	if (data_td != status_td) {
		printk(KERN_DEBUG " *after* Data TD %lx:\n", virt_to_bus(data_td));
		show_ohci_td(data_td);
	}
	printk(KERN_DEBUG " *after* Status TD %lx:\n", virt_to_bus(status_td));
	show_ohci_td(status_td);
	printk(KERN_DEBUG " *after* Controller Status:\n");
	show_ohci_status(dev->ohci);
	}
#endif

	/* clean up */
	ohci_free_td(setup_td);
	if (data_td != status_td)
		ohci_free_td(data_td);
	ohci_free_td(status_td);
	/* remove the control ED from the HC */
	ohci_remove_control_ed(dev->ohci, control_ed);
	ohci_free_ed(control_ed);	 /* return it to the pool */

#if 0
	printk(KERN_DEBUG "leaving ohci_control_msg\n");
#endif
	return completion_status;
} /* ohci_control_msg() */


/*
 * Allocate a new USB device to be attached to an OHCI controller
 */
static struct usb_device *ohci_usb_allocate(struct usb_device *parent)
{
	struct usb_device *usb_dev;
	struct ohci_device *dev;
	int idx;

	/*
	 * Allocate the generic USB device
	 */
	usb_dev = kmalloc(sizeof(*usb_dev), GFP_KERNEL);
	if (!usb_dev)
		return NULL;

	memset(usb_dev, 0, sizeof(*usb_dev));

	/*
	 * Allocate an OHCI device (EDs and TDs for this device)
	 */
	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		kfree(usb_dev);
		return NULL;
	}

	memset(dev, 0, sizeof(*dev));

	/* Initialize all EDs in a new device with the skip flag so that
	 * they are ignored by the controller until set otherwise. */
	for (idx = 0; idx < NUM_EDS; ++idx) {
		dev->ed[idx].status |= OHCI_ED_SKIP;
	}

	/*
	 * Link them together
	 */
	usb_dev->hcpriv = dev;
	dev->usb = usb_dev;

	/*
	 * Link the device to its parent (hub, etc..) if any.
	 */
	usb_dev->parent = parent;

	if (parent) {
		usb_dev->bus = parent->bus;
		dev->ohci = usb_to_ohci(parent)->ohci;
	}

	return usb_dev;
} /* ohci_usb_allocate() */


/*
 * Free a usb device.
 *
 * TODO This function needs to take better care of the EDs and TDs, etc.
 */
static int ohci_usb_deallocate(struct usb_device *usb_dev)
{
	kfree(usb_to_ohci(usb_dev));
	kfree(usb_dev);
	return 0;
}

/* FIXME! */
#define ohci_bulk_msg NULL

/*
 * functions for the generic USB driver
 */
struct usb_operations ohci_device_operations = {
	ohci_usb_allocate,
	ohci_usb_deallocate,
	ohci_control_msg,
	ohci_bulk_msg,
	ohci_request_irq,
};


/*
 * Reset an OHCI controller.  Returns >= 0 on success.
 *
 * Afterwards the HC will be in the "suspend" state which prevents you
 * from writing to some registers.  Bring it to the operational state
 * ASAP.
 */
static int reset_hc(struct ohci *ohci)
{
	int timeout = 10000;  /* prevent an infinite loop */

#if 0
	printk(KERN_INFO "usb-ohci: resetting HC %p\n", ohci);
#endif

	writel(~0x0, &ohci->regs->intrdisable);    /* Disable HC interrupts */
	writel(1, &ohci->regs->cmdstatus);	   /* HC Reset */
	writel_mask(0x3f, &ohci->regs->control);   /* move to UsbReset state */

	while ((readl(&ohci->regs->cmdstatus) & OHCI_CMDSTAT_HCR) != 0) {
		if (!--timeout) {
			printk("usb-ohci: USB HC reset timed out!\n");
			return -1;
		}
		udelay(1);
	}

	printk(KERN_INFO "usb-ohci: HC %p reset.\n", ohci);

	return 0;
} /* reset_hc() */


/*
 * Reset and start an OHCI controller.  Returns >= 0 on success.
 */
static int start_hc(struct ohci *ohci)
{
	int ret = 0;
	int fminterval;
	__u32 what_to_enable;

	struct ohci_device *root_hub=usb_to_ohci(ohci->bus->root_hub);

	fminterval = readl(&ohci->regs->fminterval) & 0x3fff;
#if 0
	printk(KERN_DEBUG "entering start_hc %p\n", ohci);
#endif

	if (reset_hc(ohci) < 0)
		return -1;

	/* restore registers cleared by the reset */
	writel(virt_to_bus(root_hub->hcca), &ohci->regs->hcca);

	/*
	 * XXX Should fminterval also be set here?
	 * The spec suggests 0x2edf [11,999]. (FIXME: make this a constant)
	 */
	fminterval |= (0x2edf << 16);
	writel(fminterval, &ohci->regs->fminterval);
	/* Start periodic transfers at 90% of fminterval (fmremaining
	 * counts down; this will put them in the first 10% of the
	 * frame). */
	writel((0x2edf*9)/10, &ohci->regs->periodicstart);

	/*
	 * FNO (frame number overflow) could be enabled...  they
	 * occur every 32768 frames (every 32-33 seconds).  This is
	 * useful for debugging and as a bus heartbeat. -greg
	 */
	/* Choose the interrupts we care about */
	what_to_enable = OHCI_INTR_MIE |
#ifdef OHCI_RHSC_INT
			OHCI_INTR_RHSC |
#endif
			/* | OHCI_INTR_FNO */
			OHCI_INTR_WDH;
	writel( what_to_enable, &ohci->regs->intrenable);

	/* Enter the USB Operational state & start the frames a flowing.. */
	writel_set(OHCI_USB_OPER, &ohci->regs->control);
	
	/* Enable control lists */
	writel_set(OHCI_USB_IE | OHCI_USB_CLE | OHCI_USB_BLE, &ohci->regs->control);

	/* Force global power enable -gal@cs.uni-magdeburg.de */
	/* 
	 * This turns on global power switching for all the ports
	 * and tells the HC that all of the ports should be powered on
	 * all of the time.
	 *
	 * TODO: This could be battery draining for laptops.. We
	 *       should implement power switching.
	 */
	writel_set( OHCI_ROOT_A_NPS, &ohci->regs->roothub.a );
	writel_mask( ~((__u32)OHCI_ROOT_A_PSM), &ohci->regs->roothub.a );

	/* Turn on power to the root hub ports (thanks Roman!) */
	writel( OHCI_ROOT_LPSC, &ohci->regs->roothub.status );

	printk("usb-ohci: host controller operational\n");

	return ret;
} /* start_hc() */


/*
 * Reset a root hub port
 */
static void ohci_reset_port(struct ohci *ohci, unsigned int port)
{
	int status;

	/* Don't allow overflows. */
	if (port >= MAX_ROOT_PORTS) {
		printk("usb-ohci: bad port #%d in ohci_reset_port\n", port);
		port = MAX_ROOT_PORTS-1;
	}

	writel(PORT_PRS, &ohci->regs->roothub.portstatus[port]);  /* Reset */

	/*
	 * Wait for the reset to complete.
	 */
	wait_ms(20);

	/* check port status to see that the reset completed */
	status = readl(&ohci->regs->roothub.portstatus[port]);
	if (status & PORT_PRS) {
		/* reset failed, try harder? */
		printk("usb-ohci: port %d reset failed, retrying\n", port);
		writel(PORT_PRS, &ohci->regs->roothub.portstatus[port]);
		wait_ms(50);
	}

	/* TODO we might need to re-enable the port here or is that
	 * done elsewhere? */

} /* ohci_reset_port */


/*
 * This gets called if the connect status on the root hub changes.
 */
static void ohci_connect_change(struct ohci * ohci, int port)
{
	struct usb_device *usb_dev;
	struct ohci_device *dev;
	struct ohci_device *root_hub=usb_to_ohci(ohci->bus->root_hub);
	/* memory I/O address of the port status register */
	__u32 *portaddr = &ohci->regs->roothub.portstatus[port];
	int portstatus;	

#ifdef OHCI_DEBUG
	printk(KERN_DEBUG "ohci_connect_change on port %d\n", port);
#endif

	/*
	 * Because of the status change we have to forget
	 * everything we think we know about the device
	 * on this root hub port.  It may have changed.
	 */
	usb_disconnect(root_hub->usb->children + port);

	portstatus = readl(portaddr);

	/* disable the port if nothing is connected */
	if (!(portstatus & PORT_CCS)) {
		writel(PORT_CCS, portaddr);
		/* We need to reset the CSC bit -after- disabling the
		 * port because it causes the CSC bit to come on
		 * again... */
		wait_ms(20);
		writel(PORT_CSC, portaddr);
#ifdef OHCI_DEBUG
		printk(KERN_DEBUG "ohci port %d disabled, nothing connected.\n", port);
#endif
		return;
	}

	/*
	 * Allocate a device for the new thingy that's been attached
	 */
	usb_dev = ohci_usb_allocate(root_hub->usb);
	dev = usb_dev->hcpriv;

	dev->ohci = ohci;

	usb_connect(dev->usb);

	/* link it into the bus's device tree */
	root_hub->usb->children[port] = usb_dev;

	wait_ms(200); /* wait for powerup; XXX is this needed? */
	ohci_reset_port(ohci, port);

	/* Get information on speed by using LSD */
	usb_dev->slow = readl(portaddr) & PORT_LSDA ? 1 : 0;

	/*
	 * Do generic USB device tree processing on the new device.
	 */
	usb_new_device(usb_dev);

} /* ohci_connect_change() */


/*
 * This gets called when the root hub configuration
 * has changed.  Just go through each port, seeing if
 * there is something interesting happening.
 */
static void ohci_check_configuration(struct ohci *ohci)
{
	struct ohci_regs *regs = ohci->regs;
	int num = 0;
	int maxport = readl(&ohci->regs->roothub) & 0xff;
	__u32 rh_change_flags = PORT_CSC | PORT_PESC;	/* root hub status changes */

#ifdef OHCI_DEBUG
	printk(KERN_DEBUG "entering ohci_check_configuration %p\n", ohci);
#endif

	do {
		__u32 *portstatus_p = &regs->roothub.portstatus[num];
		if (readl(portstatus_p) & rh_change_flags) {
			/* acknowledge the root hub status changes */
			writel_set(rh_change_flags, portstatus_p);
			/* disable the port if nothing is on it */
			/* check the port for a nifty device */
			ohci_connect_change(ohci, num);
		}
	} while (++num < maxport);

#if 0
	printk(KERN_DEBUG "leaving ohci_check_configuration %p\n", ohci);
#endif
} /* ohci_check_configuration() */



/*
 * Check root hub port status and wake the control thread up if
 * anything has changed.
 *
 * This function is called from the interrupt handler.
 */
static void ohci_root_hub_events(struct ohci *ohci)
{
		int num = 0;
	struct ohci_device *root_hub=usb_to_ohci(ohci->bus->root_hub);
	int maxport = root_hub->usb->maxchild;

	if (!waitqueue_active(&ohci_configure))
		return;
		do {
			__u32 *portstatus_p = &ohci->regs->roothub.portstatus[num];
			if (readl(portstatus_p) & PORT_CSC) {
				if (waitqueue_active(&ohci_configure))
					wake_up(&ohci_configure);
				return;
			}
		} while (++num < maxport);
	
} /* ohci_root_hub_events() */


/*
 * The done list is in reverse order; we need to process TDs in the
 * order they were finished (FIFO).  This function builds the FIFO
 * list using the next_dl_td pointer.
 *
 * This function originally by Roman Weissgaerber (weissg@vienna.at)
 *
 * This function is called from the interrupt handler.
 */
static struct ohci_td * ohci_reverse_donelist(struct ohci * ohci)
{
	__u32 td_list_hc;
	struct ohci_device *root_hub=usb_to_ohci(ohci->bus->root_hub);
	struct ohci_hcca *hcca = root_hub->hcca;
	struct ohci_td *td_list = NULL;
	struct ohci_td *td_rev = NULL;
  	
	td_list_hc = hcca->donehead & 0xfffffff0;
	hcca->donehead = 0;

 	while(td_list_hc) {
		td_list = (struct ohci_td *) bus_to_virt(td_list_hc);
		td_list->next_dl_td = td_rev;
			
		td_rev = td_list;
		td_list_hc = td_list->next_td & 0xfffffff0;
	}

	return td_list;
} /* ohci_reverse_donelist() */


/*
 * Collect this interrupt's goodies off of the list of finished TDs
 * that the OHCI controller is kind enough to setup for us.
 *
 * This function is called from the interrupt handler.
 */
static void ohci_reap_donelist(struct ohci *ohci)
{
	struct ohci_td *td;		/* used for walking the list */

	spin_lock(&ohci_edtd_lock);

	/* create the FIFO ordered donelist */
	td = ohci_reverse_donelist(ohci);

	while (td != NULL) {
		struct ohci_td *next_td = td->next_dl_td;

		if (td_dummy(*td))
			printk("yikes! reaping a dummy TD\n");

		/* FIXME: munge td->info into a future standard status format */
		/* Check if TD should be re-queued */
		if ((td->completed != NULL) &&
		    (td->completed(OHCI_TD_CC_GET(td->info), td->data, td->dev_id)))
		{
			/* Mark the TD as active again:
			 * Set the not accessed condition code
			 * Reset the Error count
			 * [FIXME: report errors to the device's driver]
			 */
			td->info |= OHCI_TD_CC_NEW;
			clear_td_errorcount(td);

			/* point it back to the start of the data buffer */
			td->cur_buf = virt_to_bus(td->data);

			/* insert it back on its ED */
			ohci_add_td_to_ed(td, td->ed);
		} else {
			/* return it to the pool of free TDs */
			ohci_free_td(td);
		}

		td = next_td;
	}

	spin_unlock(&ohci_edtd_lock);
} /* ohci_reap_donelist() */


/*
 * Get annoyed at the controller for bothering us.
 * This pretty much follows the OHCI v1.0a spec, section 5.3.
 */
static void ohci_interrupt(int irq, void *__ohci, struct pt_regs *r)
{
	struct ohci *ohci = __ohci;
	struct ohci_regs *regs = ohci->regs;
	struct ohci_device *root_hub=usb_to_ohci(ohci->bus->root_hub);
	struct ohci_hcca *hcca = root_hub->hcca;
	__u32 status, context;

	/* Save the status of the interrupts that are enabled */
	status = readl(&regs->intrstatus);
	status &= readl(&regs->intrenable);

	/* make context = the interrupt status bits that we care about */
	if (hcca->donehead != 0) {
		context = OHCI_INTR_WDH;   /* hcca donehead needs processing */
		if (hcca->donehead & 1) {
			context |= status;  /* other status change to check */
		}
	} else {
		context = status;
		if (!context) {
			/* TODO increment a useless interrupt counter here */
			return;
		}
	}

	/* Disable HC interrupts */
	writel(OHCI_INTR_MIE, &regs->intrdisable);

	/* Process the done list */
	if (context & OHCI_INTR_WDH) {
		/* See which TD's completed.. */
		ohci_reap_donelist(ohci);

		/* reset the done queue and tell the controller */
		hcca->donehead = 0;
		writel(OHCI_INTR_WDH, &regs->intrstatus);

		context &= ~OHCI_INTR_WDH;  /* mark this as checked */
	}

#ifdef OHCI_RHSC_INT
	/* NOTE: this is very funky on some USB controllers (ie: it
	 * doesn't work right).  Using the ohci_timer instead to poll
	 * the root hub is a much better choice. */
	/* Process any root hub status changes */
	if (context & OHCI_INTR_RHSC) {
		/* Wake the thread to process root hub events */
		if (waitqueue_active(&ohci_configure))
			wake_up(&ohci_configure);

		writel(OHCI_INTR_RHSC, &regs->intrstatus);
		/* 
		 * Don't unset RHSC in context; it should be disabled.
		 * The control thread will re-enable it after it has
		 * checked the root hub status.
		 */
	}
#endif

	/* Start of Frame interrupts, used during safe ED removal */
	if (context & (OHCI_INTR_SF)) {
		writel(OHCI_INTR_SF, &regs->intrstatus);
		if (waitqueue_active(&start_of_frame_wakeup))
			wake_up(&start_of_frame_wakeup);
		/* Do NOT mark the frame start interrupt as checked
		 * as we don't want to receive any more of them until
		 * asked. */
	}

	/* Check those "other" pesky bits */
	if (context & (OHCI_INTR_FNO)) {
		writel(OHCI_INTR_FNO, &regs->intrstatus);
		context &= ~OHCI_INTR_FNO;  /* mark this as checked */
	}
	if (context & OHCI_INTR_SO) {
		writel(OHCI_INTR_SO, &regs->intrstatus);
		context &= ~OHCI_INTR_SO;  /* mark this as checked */
	}
	if (context & OHCI_INTR_RD) {
		writel(OHCI_INTR_RD, &regs->intrstatus);
		context &= ~OHCI_INTR_RD;  /* mark this as checked */
	}
	if (context & OHCI_INTR_UE) {
		/* FIXME: need to have the control thread reset the
		 * controller now and keep a count of unrecoverable
		 * errors.  If there are too many, it should just shut
		 * the broken controller down entirely. */
		writel(OHCI_INTR_UE, &regs->intrstatus);
		context &= ~OHCI_INTR_UE;  /* mark this as checked */
	}
	if (context & OHCI_INTR_OC) {
		writel(OHCI_INTR_OC, &regs->intrstatus);
		context &= ~OHCI_INTR_OC;  /* mark this as checked */
	}

	/* Mask out any remaining unprocessed or unmasked interrupts
	 * so that we don't get any more of them. */
	if (context & ~OHCI_INTR_MIE) {
		writel(context, &regs->intrdisable);
	}

	/* Re-enable HC interrupts */
	writel(OHCI_INTR_MIE, &regs->intrenable);

} /* ohci_interrupt() */


/*
 * Allocate the resources required for running an OHCI controller.
 * Host controller interrupts must not be running while calling this
 * function or the penguins will get angry.
 *
 * The mem_base parameter must be the usable -virtual- address of the
 * host controller's memory mapped I/O registers.
 */
static struct ohci *alloc_ohci(void* mem_base)
{
	int i;
	struct ohci *ohci;
	struct usb_bus *bus;
	struct ohci_device *dev;
	struct usb_device *usb;

#if 0
	printk(KERN_DEBUG "entering alloc_ohci %p\n", mem_base);
#endif

	ohci = kmalloc(sizeof(*ohci), GFP_KERNEL);
	if (!ohci)
		return NULL;

	memset(ohci, 0, sizeof(*ohci));

	ohci->irq = -1;
	ohci->regs = mem_base;
	INIT_LIST_HEAD(&ohci->interrupt_list);

	bus = kmalloc(sizeof(*bus), GFP_KERNEL);
	if (!bus)
		return NULL;

	memset(bus, 0, sizeof(*bus));

	ohci->bus = bus;
	bus->hcpriv = ohci;
	bus->op = &ohci_device_operations;

	/*
	 * Allocate the USB device structure and root hub.
	 *
	 * Here we allocate our own root hub and TDs as well as the
	 * OHCI host controller communications area.  The HCCA is just
	 * a nice pool of memory with pointers to endpoint descriptors
	 * for the different interrupts.
	 */
	usb = ohci_usb_allocate(NULL);
	if (!usb)
		return NULL;

	dev = usb_to_ohci(usb);
	ohci->bus->root_hub= ohci_to_usb(dev);
	usb->bus = bus;

	/* Initialize the root hub */
	dev->ohci = ohci;    /* link back to the controller */

	/*
	 * Allocate the Host Controller Communications Area on a 256
	 * byte boundary.  XXX take the easy way out and just grab a
	 * page as that's guaranteed to have a nice boundary.
	 */
	dev->hcca = (struct ohci_hcca *) __get_free_page(GFP_KERNEL);

	/* Tell the controller where the HCCA is */
	writel(virt_to_bus(dev->hcca), &ohci->regs->hcca);

#if 0
	printk(KERN_DEBUG "usb-ohci: HCCA allocated at %p (bus %p)\n", dev->hcca, (void*)virt_to_bus(dev->hcca));
#endif

	/* Get the number of ports on the root hub */
	usb->maxchild = readl(&ohci->regs->roothub.a) & 0xff;
	if (usb->maxchild > MAX_ROOT_PORTS) {
		printk("usb-ohci: Limited to %d ports\n", MAX_ROOT_PORTS);
		usb->maxchild = MAX_ROOT_PORTS;
	}
	if (usb->maxchild < 1) {
		printk("usb-ohci: Less than one root hub port? Impossible!\n");
		usb->maxchild = 1;
	}
	printk("usb-ohci: %d root hub ports found\n", usb->maxchild);

	/*
	 * Initialize the ED polling "tree" (for simplicity's sake in
	 * this driver many nodes in the tree will be identical)
	 */
	dev->ed[ED_INT_32].next_ed = virt_to_bus(&dev->ed[ED_INT_16]);
	dev->ed[ED_INT_16].next_ed = virt_to_bus(&dev->ed[ED_INT_8]);
	dev->ed[ED_INT_8].next_ed = virt_to_bus(&dev->ed[ED_INT_4]);
	dev->ed[ED_INT_4].next_ed = virt_to_bus(&dev->ed[ED_INT_2]);
	dev->ed[ED_INT_2].next_ed = virt_to_bus(&dev->ed[ED_INT_1]);

	/*
	 * Initialize the polling table to call interrupts at the
	 * intended intervals.  Note that these EDs are just
	 * placeholders.  They have their SKIP bit set and are used as
	 * list heads to insert real EDs onto.
	 */
	dev->hcca->int_table[0] = virt_to_bus(&dev->ed[ED_INT_1]);
	for (i = 1; i < NUM_INTS; i++) {
		if (i & 16)
			dev->hcca->int_table[i] =
				virt_to_bus(&dev->ed[ED_INT_32]);
		if (i & 8)
			dev->hcca->int_table[i] =
				virt_to_bus(&dev->ed[ED_INT_16]);
		if (i & 4)
			dev->hcca->int_table[i] =
				virt_to_bus(&dev->ed[ED_INT_8]);
		if (i & 2)
			dev->hcca->int_table[i] =
				virt_to_bus(&dev->ed[ED_INT_4]);
		if (i & 1)
			dev->hcca->int_table[i] =
				virt_to_bus(&dev->ed[ED_INT_2]);
	}

	/*
	 * Tell the controller where the control and bulk lists are.
	 * The lists start out empty.
	 */
	writel(0, &ohci->regs->ed_controlhead);
	writel(0, &ohci->regs->ed_bulkhead);

#ifdef OHCI_DEBUG
	printk(KERN_INFO "alloc_ohci(): controller\n");
	show_ohci_status(ohci);
#endif

#if 0
	printk(KERN_DEBUG "leaving alloc_ohci %p\n", ohci);
#endif

	return ohci;
} /* alloc_ohci() */


/*
 * De-allocate all resoueces..
 */
static void release_ohci(struct ohci *ohci)
{
	printk(KERN_INFO "Releasing OHCI controller 0x%p\n", ohci);

#ifdef OHCI_TIMER
	/* stop our timer */
	del_timer(&ohci_timer);
#endif
	if (ohci->irq >= 0) {
		free_irq(ohci->irq, ohci);
		ohci->irq = -1;
	}

	/* stop all OHCI interrupts */
	writel(~0x0, &ohci->regs->intrdisable);

	if (ohci->bus->root_hub) {
		struct ohci_device *root_hub=usb_to_ohci(ohci->bus->root_hub);
		/* ensure that HC is stopped before releasing the HCCA */
		writel(OHCI_USB_SUSPEND, &ohci->regs->control);
		free_page((unsigned long) root_hub->hcca);
		kfree(ohci->bus->root_hub);
		root_hub->hcca = NULL;
		ohci->bus->root_hub = NULL;
	}

	/* unmap the IO address space */
	iounmap(ohci->regs);

	kfree(ohci);

	MOD_DEC_USE_COUNT;

	/* If the ohci itself were dynamic we'd free it here */

	printk(KERN_DEBUG "usb-ohci: HC resources released.\n");
} /* release_ohci() */


/*
 * USB OHCI control thread
 */
static int ohci_control_thread(void * __ohci)
{
	struct ohci *ohci = (struct ohci *)__ohci;

	/*
	 * I'm unfamiliar with the SMP kernel locking.. where should
	 * this be released and what does it do?  -greg
	 */
	lock_kernel();

	/*
	 * This thread doesn't need any user-level access,
	 * so get rid of all of our resources..
	 */
	printk(KERN_INFO "ohci-control thread code for 0x%p code at 0x%p\n", __ohci, &ohci_control_thread);
	exit_mm(current);
	exit_files(current);
	exit_fs(current);

	strcpy(current->comm, "ohci-control");

	usb_register_bus(ohci->bus);

	/*
	 * Damn the torpedoes, full speed ahead
	 */
	if (start_hc(ohci) < 0) {
		printk("usb-ohci: failed to start the controller\n");
		release_ohci(ohci);
		usb_deregister_bus(ohci->bus);
		printk(KERN_INFO "leaving ohci_control_thread %p\n", __ohci);
		return 0;
	}

	for(;;) {
		siginfo_t info;
		int unsigned long signr;

		wait_ms(200);

		/* check the root hub configuration for changes. */
		ohci_check_configuration(ohci);

		/* re-enable root hub status change interrupts. */
#ifdef OHCI_RHSC_INT
		writel(OHCI_INTR_RHSC, &ohci->regs->intrenable);
#endif

		printk(KERN_INFO "ohci-control thread sleeping\n");
		interruptible_sleep_on(&ohci_configure);
#ifdef CONFIG_APM
		if (apm_resume) {
			apm_resume = 0;
			if (start_hc(ohci) < 0)
				break;
			continue;
		}
#endif

		/*
		 * If we were woken up by a signal, see if its useful,
		 * otherwise exit.
		 */
		if (signal_pending(current)) {
			/* sending SIGUSR1 makes us print out some info */
			spin_lock_irq(&current->sigmask_lock);
			signr = dequeue_signal(&current->blocked, &info);
			spin_unlock_irq(&current->sigmask_lock);

			if(signr == SIGUSR1) {
				/* TODO: have it do a full ed/td queue dump? */
				printk(KERN_DEBUG "OHCI status dump:\n");
				show_ohci_status(ohci);
			} else if (signr == SIGUSR2) {
				/* toggle mega TD/ED debugging output */
				MegaDebug = !MegaDebug;
				printk(KERN_DEBUG "usb-ohci: Mega debugging %sabled.\n",
						MegaDebug ? "en" : "dis");
			} else {
				/* unknown signal, exit the thread */
				break;
			}
		}
	} /* for (;;) */

	reset_hc(ohci);
	release_ohci(ohci);
	usb_deregister_bus(ohci->bus);
	printk(KERN_INFO "ohci-control thread for 0x%p exiting\n", __ohci);

	return 0;
} /* ohci_control_thread() */


#ifdef CONFIG_APM
static int handle_apm_event(apm_event_t event)
{
	static int down = 0;

	switch (event) {
	case APM_SYS_SUSPEND:
	case APM_USER_SUSPEND:
		if (down) {
			printk(KERN_DEBUG "usb-ohci: received extra suspend event\n");
			break;
		}
		down = 1;
		break;
	case APM_NORMAL_RESUME:
	case APM_CRITICAL_RESUME:
		if (!down) {
			printk(KERN_DEBUG "usb-ohci: received bogus resume event\n");
			break;
		}
		down = 0;
		if (waitqueue_active(&ohci_configure)) {
			apm_resume = 1;
			wake_up(&ohci_configure);
		}
		break;
	}
	return 0;
} /* handle_apm_event() */
#endif


#ifdef OHCI_TIMER
/*
 * Inspired by I�aky's driver.  This function is a timer routine that
 * is called every OHCI_TIMER_FREQ ms.  It polls the root hub for
 * status changes as on my system the RHSC interrupt just doesn't
 * play well with others.. (so RHSC is turned off by default in this
 * driver)
 * [my controller is a "SiS 7001 USB (rev 16)"]
 * -greg
 */
static void ohci_timer_func (unsigned long ohci_ptr)
{
	struct ohci *ohci = (struct ohci*)ohci_ptr;

	ohci_root_hub_events(ohci);

	/* set the next timer */
	mod_timer(&ohci_timer, jiffies + ((OHCI_TIMER_FREQ*HZ)/1000));

} /* ohci_timer_func() */
#endif


/*
 * Increment the module usage count, start the control thread and
 * return success if the controller is good.
 */
static int found_ohci(int irq, void* mem_base)
{
	int retval;
	struct ohci *ohci;

#if 0
	printk(KERN_DEBUG "entering found_ohci %d %p\n", irq, mem_base);
#endif

	/* Allocate the running OHCI structures */
	ohci = alloc_ohci(mem_base);
	if (!ohci) {
		return -ENOMEM;
	}

#ifdef OHCI_TIMER
	init_timer(&ohci_timer);
	ohci_timer.expires = jiffies + ((OHCI_TIMER_FREQ*HZ)/1000);
	ohci_timer.data = (unsigned long)ohci;
	ohci_timer.function = ohci_timer_func;
	add_timer(&ohci_timer);
#endif

	retval = -EBUSY;
	if (request_irq(irq, ohci_interrupt, SA_SHIRQ, "usb-ohci", ohci) == 0) {
		int pid;

		ohci->irq = irq;

#ifdef OHCI_DEBUG
		printk(KERN_INFO "usb-ohci: forking ohci-control thread for 0x%p\n", ohci);
#endif

		/* fork off the handler */
		pid = kernel_thread(ohci_control_thread, ohci,
				CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
		if (pid >= 0) {
			return 0;
		}

		retval = pid;
	} else {
		printk("usb-ohci: Couldn't allocate interrupt %d\n", irq);
	}
	release_ohci(ohci);

#ifdef OHCI_DEBUG
	printk(KERN_DEBUG "leaving found_ohci %d %p\n", irq, mem_base);
#endif

	return retval;
} /* found_ohci() */


/*
 * If this controller is for real, map the IO memory and proceed
 */
static int init_ohci(struct pci_dev *dev)
{
	unsigned long mem_base = dev->base_address[0];
	
	/* If its OHCI, its memory */
	if (mem_base & PCI_BASE_ADDRESS_SPACE_IO)
		return -ENODEV;

	/* Get the memory address and map it for IO */
	mem_base &= PCI_BASE_ADDRESS_MEM_MASK;

	/* no interrupt won't work... */
	if (dev->irq == 0) {
		printk("usb-ohci: no irq assigned? check your BIOS settings.\n");
		return -ENODEV;
	}

	/* 
	 * FIXME ioremap_nocache isn't implemented on all CPUs (such
	 * as the Alpha) [?]  What should I use instead...
	 *
	 * The iounmap() is done on in release_ohci.
	 */
	mem_base = (unsigned long) ioremap_nocache(mem_base, 4096);

	if (!mem_base) {
		printk("Error mapping OHCI memory\n");
		return -EFAULT;
	}
        MOD_INC_USE_COUNT;

#ifdef OHCI_DEBUG
	printk("usb-ohci: Warning! Gobs of debugging output has been enabled.\n");
	printk("          Check your kern.debug logs for the bulk of it.\n");
#endif

	if (found_ohci(dev->irq, (void *) mem_base) < 0) {
		MOD_DEC_USE_COUNT;
		return -1;
	}

	return 0;
} /* init_ohci() */

/* TODO this should be named following Linux convention and go in pci.h */
#define PCI_CLASS_SERIAL_USB_OHCI ((PCI_CLASS_SERIAL_USB << 8) | 0x0010)

/*
 * Search the PCI bus for an OHCI USB controller and set it up
 *
 * If anyone wants multiple controllers this will need to be
 * updated..  Right now, it just picks the first one it finds.
 */
int ohci_init(void)
{
	int retval;
	struct pci_dev *dev = NULL;
	/*u8 type;*/

	if (sizeof(struct ohci_device) > 4096) {
		printk("usb-ohci: struct ohci_device to large\n");
		return -ENODEV;
	}

	printk("OHCI USB Driver loading\n");

	retval = -ENODEV;
	for (;;) {
		/* Find an OHCI USB controller */
		dev = pci_find_class(PCI_CLASS_SERIAL_USB_OHCI, dev);
		if (!dev)
			break;

		/* Verify that its OpenHCI by checking for MMIO */
		/* pci_read_config_byte(dev, PCI_CLASS_PROG, &type);
		if (!type)
			continue; */

		/* Ok, set it up */
		retval = init_ohci(dev);
		if (retval < 0)
			continue;

#ifdef CONFIG_APM
		apm_register_callback(&handle_apm_event);
#endif

		return 0; /* no error */
	}
	return retval;
} /* ohci_init */


/* vim:sw=8
 */

#ifdef MODULE
/*
 *  Clean up when unloading the module
 */
void cleanup_module(void){
#	ifdef CONFIG_APM
	apm_unregister_callback(&handle_apm_event);
#	endif
	printk("usb-ohci: module unloaded\n");
}

int init_module(void){
	return ohci_init();
}
#endif //MODULE


