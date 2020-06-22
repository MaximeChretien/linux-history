/*
 * ACPI PCI Hot Plug Controller Driver
 *
 * Copyright (c) 1995,2001 Compaq Computer Corporation
 * Copyright (c) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (c) 2001 IBM Corp.
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
 *		    <h-aono@ap.jp.nec.com>,
 *		    <t-kouchi@cq.jp.nec.com>
 *
 */

#ifndef _PCIHP_ACPI_H
#define _PCIHP_ACPI_H

#include "include/acpi.h"

#if ACPI_CA_VERSION < 0x20020201
/* until we get a new version of the ACPI driver for both ia32 and ia64 ... */
#define acpi_util_eval_error(h,p,s)

static acpi_status
acpi_evaluate_integer (
	acpi_handle		handle,
	acpi_string		pathname,
	acpi_object_list	*arguments,
	unsigned long		*data)
{
	acpi_status             status = AE_OK;
	acpi_object             element;
	acpi_buffer		buffer = {sizeof(acpi_object), &element};

	if (!data)
		return AE_BAD_PARAMETER;

	status = acpi_evaluate_object(handle, pathname, arguments, &buffer);
	if (ACPI_FAILURE(status)) {
		acpi_util_eval_error(handle, pathname, status);
		return status;
	}

	if (element.type != ACPI_TYPE_INTEGER) {
		acpi_util_eval_error(handle, pathname, AE_BAD_DATA);
		return AE_BAD_DATA;
	}

	*data = element.integer.value;

	return AE_OK;
}
#else  /* ACPI_CA_VERSION < 0x20020201 */
#include "acpi_bus.h"
#endif /* ACPI_CA_VERSION < 0x20020201 */

/* compatibility stuff */
#ifndef ACPI_MEMORY_RANGE
#define ACPI_MEMORY_RANGE MEMORY_RANGE
#endif

#ifndef ACPI_IO_RANGE
#define ACPI_IO_RANGE IO_RANGE
#endif

#ifndef ACPI_BUS_NUMBER_RANGE
#define ACPI_BUS_NUMBER_RANGE BUS_NUMBER_RANGE
#endif

#ifndef ACPI_PREFETCHABLE_MEMORY
#define ACPI_PREFETCHABLE_MEMORY PREFETCHABLE_MEMORY
#endif

#ifndef ACPI_PRODUCER
#define ACPI_PRODUCER PRODUCER
#endif


#define dbg(format, arg...)					\
	do {							\
		if (debug)					\
			printk (KERN_DEBUG "%s: " format "\n",	\
				MY_NAME , ## arg); 		\
	} while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format "\n", MY_NAME , ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format "\n", MY_NAME , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format "\n", MY_NAME , ## arg)

/*
 * types and constants
 */

#define SLOT_MAGIC	0x67267322

struct pcihp_acpi_bridge;
struct pcihp_acpi_slot;

/* slot information for each *physical* slot */

struct slot {
	u32 magic;
	u8 number;
	struct hotplug_slot	*hotplug_slot;
	struct list_head	slot_list;

	int			attention_status;

	struct pci_resource	*mem_head;
	struct pci_resource	*p_mem_head;
	struct pci_resource	*io_head;
	struct pci_resource	*bus_head;

	/* if there are multiple corresponding slot objects,
	   this point to one of them */
	struct pcihp_acpi_slot	*acpi_slot;

	struct pci_dev		*pci_dev;
};

#define RESOURCE_TYPE_IO	(1)
#define RESOURCE_TYPE_MEM	(2)
#define RESOURCE_TYPE_PREFETCH	(3)
#define RESOURCE_TYPE_BUS	(4)

/* TBD 64bit resource support */
struct pci_resource {
	struct pci_resource * next;
	u32 base;
	u32 length;
};

/* bridge information for each bridge device in ACPI namespace */

struct pcihp_acpi_bridge {
	struct list_head list;
	acpi_handle handle;
	struct pcihp_acpi_slot *slots;
	int nr_slots;
	u8 seg;
	u8 bus;
	u8 sub;
	u32 status;
	u32 flags;

	/* resources on this bus (free resources) */
	struct pci_resource *free_io;
	struct pci_resource *free_mem;
	struct pci_resource *free_prefetch;
	struct pci_resource *free_bus;

	/* used resources (embedded or owned resources) */
	struct pci_resource *used_io;
	struct pci_resource *used_mem;
	struct pci_resource *used_prefetch;
	struct pci_resource *used_bus;
};

/*
 * slot information for each slot object in ACPI namespace
 * usually 8 objects per slot (for each PCI function)
 */

struct pcihp_acpi_slot {
	struct pcihp_acpi_slot	*next;
	struct pcihp_acpi_bridge *bridge; /* this slot located on */
	struct list_head sibling;	/* one slot may have different
					   objects (i.e. for each function) */
	acpi_handle	handle;
	u32		id;		/* slot id (this driver specific) */
	u8		device;		/* pci device# */
	u8		function;	/* pci function# */
	u8		pin;		/* pci interrupt pin */
	u32		sun;		/* _SUN */
	u32		flags;		/* see below */
	u32		status;		/* _STA */
};

/* PCI bus bridge HID */
#define ACPI_PCI_ROOT_HID		"PNP0A03"

/* ACPI _STA method value (ignore bit 4; battery present) */
#define ACPI_STA_PRESENT		(0x00000001)
#define ACPI_STA_ENABLED		(0x00000002)
#define ACPI_STA_SHOW_IN_UI		(0x00000004)
#define ACPI_STA_FUNCTIONAL		(0x00000008)
#define ACPI_STA_ALL			(0x0000000f)

/* bridge flags */
#define BRIDGE_HAS_STA	(0x00000001)

/* slot flags */

#define SLOT_HAS_EJ0	(0x00000001)
#define SLOT_HAS_PS0	(0x00000002)
#define SLOT_HAS_PS3	(0x00000004)

/* function prototypes */

/* pcihp_acpi_glue.c */
extern int pcihp_acpi_glue_init (void);
extern void pcihp_acpi_glue_exit (void);
extern int pcihp_acpi_get_num_slots (void);
extern struct pcihp_acpi_slot *get_slot_from_id (int id);

extern int pcihp_acpi_enable_slot (struct pcihp_acpi_slot *slot);
extern int pcihp_acpi_disable_slot (struct pcihp_acpi_slot *slot);
extern u8 pcihp_acpi_get_power_status (struct pcihp_acpi_slot *slot);
extern u8 pcihp_acpi_get_attention_status (struct pcihp_acpi_slot *slot);
extern u8 pcihp_acpi_get_latch_status (struct pcihp_acpi_slot *slot);
extern u8 pcihp_acpi_get_adapter_status (struct pcihp_acpi_slot *slot);

#endif /* _PCIHP_ACPI_H */
