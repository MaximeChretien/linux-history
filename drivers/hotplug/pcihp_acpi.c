/*
 * ACPI PCI Hot Plug Controller Driver
 *
 * Copyright (c) 2001-2002 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (c) 2001-2002 IBM Corp.
 * Copyright (c) 2002 Hiroshi Aono (h-aono@ap.jp.nec.com)
 * Copyright (c) 2002 Takayoshi Kochi (t-kouchi@cq.jp.nec.com)
 * Copyright (c) 2002 NEC Corporation
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <gregkh@us.ibm.com>,
 *                  <h-aono@ap.jp.nec.com>,
 *		    <t-kouchi@cq.jp.nec.com>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/init.h>
#include "pci_hotplug.h"
#include "pcihp_acpi.h"

static LIST_HEAD(slot_list);

#if !defined(CONFIG_HOTPLUG_PCI_ACPI_MODULE)
	#define MY_NAME	"pcihp_acpi"
#else
	#define MY_NAME	THIS_MODULE->name
#endif

/* local variables */
static int debug = 1;			/* XXX */
static int num_slots;

#define DRIVER_VERSION	"0.2"
#define DRIVER_AUTHOR	"Greg Kroah-Hartman <gregkh@us.ibm.com>"
#define DRIVER_DESC	"ACPI Hot Plug PCI Controller Driver"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debugging mode enabled or not");

static int enable_slot		(struct hotplug_slot *slot);
static int disable_slot		(struct hotplug_slot *slot);
static int set_attention_status (struct hotplug_slot *slot, u8 value);
static int hardware_test	(struct hotplug_slot *slot, u32 value);
static int get_power_status	(struct hotplug_slot *slot, u8 *value);
static int get_attention_status	(struct hotplug_slot *slot, u8 *value);
static int get_latch_status	(struct hotplug_slot *slot, u8 *value);
static int get_adapter_status	(struct hotplug_slot *slot, u8 *value);

static struct hotplug_slot_ops acpi_hotplug_slot_ops = {
	owner:			THIS_MODULE,
	enable_slot:		enable_slot,
	disable_slot:		disable_slot,
	set_attention_status:	set_attention_status,
	hardware_test:		hardware_test,
	get_power_status:	get_power_status,
	get_attention_status:	get_attention_status,
	get_latch_status:	get_latch_status,
	get_adapter_status:	get_adapter_status,
};


/* Inline functions to check the sanity of a pointer that is passed to us */
static inline int slot_paranoia_check (struct slot *slot, const char *function)
{
	if (!slot) {
		dbg("%s - slot == NULL", function);
		return -1;
	}
	if (slot->magic != SLOT_MAGIC) {
		dbg("%s - bad magic number for slot", function);
		return -1;
	}
	if (!slot->hotplug_slot) {
		dbg("%s - slot->hotplug_slot == NULL!", function);
		return -1;
	}
	return 0;
}

static inline struct slot *get_slot (struct hotplug_slot *hotplug_slot, const char *function)
{ 
	struct slot *slot;

	if (!hotplug_slot) {
		dbg("%s - hotplug_slot == NULL", function);
		return NULL;
	}

	slot = (struct slot *)hotplug_slot->private;
	if (slot_paranoia_check (slot, function))
                return NULL;
	return slot;
}


static int enable_slot (struct hotplug_slot *hotplug_slot)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	int retval = 0;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg ("%s - physical_slot = %s", __FUNCTION__, hotplug_slot->name);

	/* enable the specified slot */
	retval = pcihp_acpi_enable_slot (slot->acpi_slot);

	return retval;
}

static int disable_slot (struct hotplug_slot *hotplug_slot)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	int retval = 0;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg ("%s - physical_slot = %s", __FUNCTION__, hotplug_slot->name);

	/* disable the specified slot */
	retval = pcihp_acpi_disable_slot (slot->acpi_slot);

	return retval;
}

static int set_attention_status (struct hotplug_slot *hotplug_slot, u8 status)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	int retval = 0;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg ("%s - physical_slot = %s", __FUNCTION__, hotplug_slot->name);

	/* TBD
	 * ACPI doesn't have known method to manipulate
	 * attention status LED
	 */
	switch (status) {
		case 0:
			/* FIXME turn light off */
			slot->attention_status = 0;
			break;

		case 1:
		default:
			/* FIXME turn light on */
			slot->attention_status = 1;
			break;
	}

	return retval;
}

static int hardware_test (struct hotplug_slot *hotplug_slot, u32 value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	int retval = 0;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg ("%s - physical_slot = %s", __FUNCTION__, hotplug_slot->name);

	err ("No hardware tests are defined for this driver");
	retval = -ENODEV;

	return retval;
}

static int get_power_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	int retval = 0;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s", __FUNCTION__, hotplug_slot->name);

	*value = pcihp_acpi_get_power_status (slot->acpi_slot);

	return retval;
}

static int get_attention_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	int retval = 0;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s", __FUNCTION__, hotplug_slot->name);

	*value = slot->attention_status;

	return retval;
}

static int get_latch_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	int retval = 0;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s", __FUNCTION__, hotplug_slot->name);

	*value = pcihp_acpi_get_latch_status (slot->acpi_slot);

	return retval;
}

static int get_adapter_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	int retval = 0;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s", __FUNCTION__, hotplug_slot->name);

	*value = pcihp_acpi_get_adapter_status (slot->acpi_slot);

	return retval;
}

static int init_acpi (void)
{
	int retval;

	dbg("init_acpi");		/* XXX */
	/* initialize internal data structure etc. */
	retval = pcihp_acpi_glue_init();

	/* read initial number of slots */
	if (!retval) {
		num_slots = pcihp_acpi_get_num_slots();
		if (num_slots == 0)
			retval = -ENODEV;
	}

	return retval;
}

static void exit_acpi (void)
{
	/* deallocate internal data structures etc. */
	pcihp_acpi_glue_exit();
}

#define SLOT_NAME_SIZE	10
static void make_slot_name (struct slot *slot)
{
	/* FIXME - get this from the ACPI representation of the slot */
	snprintf (slot->hotplug_slot->name, SLOT_NAME_SIZE, "ACPI%d", slot->number);
}

static int init_slots (void)
{
	struct slot *slot;
	int retval = 0;
	int i;

	for (i = 0; i < num_slots; ++i) {
		slot = kmalloc (sizeof (struct slot), GFP_KERNEL);
		if (!slot)
			return -ENOMEM;
		memset(slot, 0, sizeof(struct slot));

		slot->hotplug_slot = kmalloc (sizeof (struct hotplug_slot), GFP_KERNEL);
		if (!slot->hotplug_slot) {
			kfree (slot);
			return -ENOMEM;
		}
		memset(slot->hotplug_slot, 0, sizeof (struct hotplug_slot));

		slot->hotplug_slot->info = kmalloc (sizeof (struct hotplug_slot_info), GFP_KERNEL);
		if (!slot->hotplug_slot->info) {
			kfree (slot->hotplug_slot);
			kfree (slot);
			return -ENOMEM;
		}
		memset(slot->hotplug_slot->info, 0, sizeof (struct hotplug_slot_info));

		slot->hotplug_slot->name = kmalloc (SLOT_NAME_SIZE, GFP_KERNEL);
		if (!slot->hotplug_slot->name) {
			kfree (slot->hotplug_slot->info);
			kfree (slot->hotplug_slot);
			kfree (slot);
			return -ENOMEM;
		}

		slot->magic = SLOT_MAGIC;
		slot->number = i;

		slot->hotplug_slot->private = slot;
		make_slot_name (slot);
		slot->hotplug_slot->ops = &acpi_hotplug_slot_ops;
		
		slot->acpi_slot = get_slot_from_id (i);
		slot->hotplug_slot->info->power_status = pcihp_acpi_get_power_status(slot->acpi_slot);
		slot->hotplug_slot->info->attention_status = pcihp_acpi_get_attention_status(slot->acpi_slot);
		slot->hotplug_slot->info->latch_status = pcihp_acpi_get_latch_status(slot->acpi_slot);
		slot->hotplug_slot->info->adapter_status = pcihp_acpi_get_adapter_status(slot->acpi_slot);

		dbg ("registering slot %d", i);
		retval = pci_hp_register (slot->hotplug_slot);
		if (retval) {
			err ("pci_hp_register failed with error %d", retval);
			kfree (slot->hotplug_slot->info);
			kfree (slot->hotplug_slot->name);
			kfree (slot->hotplug_slot);
			kfree (slot);
			return retval;
		}

		/* add slot to our internal list */
		list_add (&slot->slot_list, &slot_list);
	}

	return retval;
}
		
static void cleanup_slots (void)
{
	struct list_head *tmp;
	struct slot *slot;

	list_for_each (tmp, &slot_list) {
		slot = list_entry (tmp, struct slot, slot_list);
		list_del (&slot->slot_list);
		pci_hp_deregister (slot->hotplug_slot);
		kfree (slot->hotplug_slot->info);
		kfree (slot->hotplug_slot->name);
		kfree (slot->hotplug_slot);
		kfree (slot);
	}

	return;
}

static int __init pcihp_acpi_init(void)
{
	int retval;

	/* read all the ACPI info from the system */
	retval = init_acpi();
	if (retval)
		return retval;

	retval = init_slots();
	if (retval)
		return retval;

	info (DRIVER_DESC " version: " DRIVER_VERSION);
	return 0;
}

static void __exit pcihp_acpi_exit(void)
{
	cleanup_slots();
	exit_acpi();
}

module_init(pcihp_acpi_init);
module_exit(pcihp_acpi_exit);
