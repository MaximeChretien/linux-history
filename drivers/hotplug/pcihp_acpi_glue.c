/*
 * ACPI PCI HotPlug glue functions to ACPI CA subsystem
 *
 * Copyright (c) 2002 Takayoshi Kochi (t-kouchi@cq.jp.nec.com)
 * Copyright (c) 2002 Hiroshi Aono (h-aono@ap.jp.nec.com)
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
 * Send feedback to <t-kouchi@cq.jp.nec.com>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include "pcihp_acpi.h"

/*
 * TODO:
 * resource management
 * irq related interface? (_PRT)
 * consider locking
 */

static LIST_HEAD(bridge_list);

static int debug = 1;			/* XXX set 0 after debug */
#define MY_NAME "pcihp_acpi_glue"

static void handle_hotplug_event (acpi_handle, u32, void *);

/*
 * initialization & terminatation routines
 */

/*
 * Ejectable slot satisfies at least these conditions:
 *  1. has _ADR method
 *  2. has _STA method
 *  3. has _EJ0 method
 *
 * optionally
 *  1. has _PS0 method
 *  2. has _PS3 method
 *  3. TBD...
 */

/* callback routine to check the existence of ejectable slots */
static acpi_status
is_ejectable_slot (acpi_handle handle, u32 lvl,	void *context, void **rv)
{
	acpi_status status;
	acpi_handle tmp;
	int *count = (int *)context;

	status = acpi_get_handle(handle, "_ADR", &tmp);

	if (ACPI_FAILURE(status)) {
		return AE_OK;
	}

	status = acpi_get_handle(handle, "_STA", &tmp);

	if (ACPI_FAILURE(status)) {
		return AE_OK;
	}

	status = acpi_get_handle(handle, "_EJ0", &tmp);

	if (ACPI_FAILURE(status)) {
		return AE_OK;
	}

	(*count)++;

	/* only one ejectable slot is enough */
	return AE_CTRL_TERMINATE;
}


/* callback routine to register each ACPI PCI slot object */
static acpi_status
register_slot (acpi_handle handle, u32 lvl, void *context, void **rv)
{
	struct pcihp_acpi_bridge *bridge = (struct pcihp_acpi_bridge *)context;
	struct pcihp_acpi_slot *slot, *newslot;
	acpi_handle tmp;
	acpi_status status = AE_OK;
	static int num_slots = 0;	/* XXX */
	unsigned long adr, sun, sta;

	status = acpi_evaluate_integer(handle, "_ADR", NULL, &adr);

	if (ACPI_FAILURE(status)) {
		return AE_OK;
	}

	status = acpi_get_handle(handle, "_EJ0", &tmp);

	if (ACPI_FAILURE(status)) {
		dbg("This slot doesn't have _EJ0");
		//return AE_OK;
	}

	status = acpi_evaluate_integer(handle, "_STA", NULL, &sta);

	if (ACPI_FAILURE(status)) {
		dbg("This slot doesn't have _STA");
		//return AE_OK;
	}

	newslot = kmalloc(sizeof(struct pcihp_acpi_slot), GFP_KERNEL);
	if (!newslot) {
		return AE_NO_MEMORY;
	}

	memset(newslot, 0, sizeof(struct pcihp_acpi_slot));

	INIT_LIST_HEAD(&newslot->sibling);
	newslot->bridge = bridge;
	newslot->handle = handle;
	newslot->device = (adr >> 16) & 0xffff;
	newslot->function = adr & 0xffff;
	newslot->status = sta;
	newslot->sun = -1;
	newslot->flags = SLOT_HAS_EJ0;
	newslot->id = num_slots++;
	bridge->nr_slots++;

	dbg("new slot id=%d device=0x%d function=0x%x", newslot->id, newslot->device, newslot->function);

	status = acpi_evaluate_integer(handle, "_SUN", NULL, &sun);
	if (ACPI_SUCCESS(status)) {
		newslot->sun = sun;
	}

	if (ACPI_SUCCESS(acpi_get_handle(handle, "_PS0", &tmp))) {
		newslot->flags |= SLOT_HAS_PS0;
	}

	if (ACPI_SUCCESS(acpi_get_handle(handle, "_PS3", &tmp))) {
		newslot->flags |= SLOT_HAS_PS3;
	}

	/* search for objects that share the same slot */
	for (slot = bridge->slots; slot; slot = slot->next)
		if (slot->device == newslot->device) {
			dbg("found a sibling slot!");
			list_add(&slot->sibling, &newslot->sibling);
			newslot->id = slot->id;
			num_slots --;
			bridge->nr_slots --;
			break;
		}

	/* link myself to bridge's slot list */
	newslot->next = bridge->slots;
	bridge->slots = newslot;

	return AE_OK;
}

/* see if it's worth managing this brige */
static int
detect_ejectable_slots (acpi_handle *root)
{
	acpi_status status;
	int count;

	count = 0;
	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, root, ACPI_UINT32_MAX,
				     is_ejectable_slot, (void *)&count, NULL);

	dbg("%s: count=%d", __FUNCTION__, count);
	return count;
}


/*
 * push one resource to resource list
 *
 * TBD: use hotplug_resource_sort_and_combine
 * TBD: 64bit resource handling (is it really used?)
 */
static void
push_resource (u32 base, u32 length, struct pci_resource **resource)
{
	struct pci_resource *resp, *newres;
	int coalesced = 0;

	if (length == 0) {
		dbg("zero sized resource. ignored.");
		return;
	}

	for (resp = *resource; resp; resp = resp->next) {

		/* coalesce contiguous region */

		if (resp->base + resp->length == base) {
			resp->length += length;
			coalesced = 1;
			break;
		}
	}

	if (!coalesced) {
		newres = kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
		if (!newres) {
			/* TBD panic? */
			return;
		}
		newres->base = base;
		newres->length = length;
		newres->next = (*resource);
		*resource = newres;
	}
}


/* decode ACPI _CRS data and convert into our internal resource list */
static void
decode_acpi_resource (acpi_resource *resource, struct pcihp_acpi_bridge *bridge)
{
	acpi_resource_address16 *address16_data;
	acpi_resource_address32 *address32_data;
	//acpi_resource_address64 *address64_data;

	u32 resource_type, producer_consumer, min_address_range, max_address_range, address_length;
	u16 cache_attribute = 0;

	int done = 0, found;

	/* shut up gcc */
	resource_type = producer_consumer = min_address_range = max_address_range = address_length = 0;

	while (!done) {
		found = 0;

		switch (resource->id) {
		case ACPI_RSTYPE_ADDRESS16:
			address16_data = (acpi_resource_address16 *)&resource->data;
			resource_type = address16_data->resource_type;
			producer_consumer = address16_data->producer_consumer;
			min_address_range = address16_data->min_address_range;
			max_address_range = address16_data->max_address_range;
			address_length = address16_data->address_length;
			if (resource_type == ACPI_MEMORY_RANGE)
				cache_attribute = address16_data->attribute.memory.cache_attribute;
			found = 1;
			break;

		case ACPI_RSTYPE_ADDRESS32:
			address32_data = (acpi_resource_address32 *)&resource->data;
			resource_type = address32_data->resource_type;
			producer_consumer = address32_data->producer_consumer;
			min_address_range = address32_data->min_address_range;
			max_address_range = address32_data->max_address_range;
			address_length = address32_data->address_length;
			if (resource_type == ACPI_MEMORY_RANGE)
				cache_attribute = address32_data->attribute.memory.cache_attribute;
			found = 1;
			break;
/*
		case ACPI_RSTYPE_ADDRESS64:
			address64_data = (acpi_resource_address64 *)&resource->data;
			resource_type = address64_data->resource_type;
			break;
*/
		case ACPI_RSTYPE_END_TAG:
			done = 1;
			break;

		default:
			/* ignore */
			break;
		}

		resource = (acpi_resource *)((char*)resource + resource->length);
		if (found && producer_consumer == ACPI_PRODUCER) {
			switch (resource_type) {
			case ACPI_MEMORY_RANGE:
				if (cache_attribute == ACPI_PREFETCHABLE_MEMORY) {
					dbg("resource type: prefetchable memory 0x%x - 0x%x", min_address_range, max_address_range);
					push_resource(min_address_range,
						      address_length,
						      &bridge->free_prefetch);
				} else {
					dbg("resource type: memory 0x%x - 0x%x", min_address_range, max_address_range);
					push_resource(min_address_range,
						      address_length,
						      &bridge->free_mem);
				}
				break;
			case ACPI_IO_RANGE:
				dbg("resource type: io 0x%x - 0x%x", min_address_range, max_address_range);
				push_resource(min_address_range,
					      address_length,
					      &bridge->free_io);
				break;
			case ACPI_BUS_NUMBER_RANGE:
				dbg("resource type: bus number %d - %d", min_address_range, max_address_range);
				push_resource(min_address_range,
					      address_length,
					      &bridge->free_bus);
				break;
			default:
				/* invalid type */
				break;
			}
		}
	}
}


/* allocate and initialize bridge data structure */
static int add_bridge (acpi_handle *handle)
{
	struct pcihp_acpi_bridge *bridge;
 	acpi_status status;
	acpi_buffer buffer;
	unsigned long tmp;
	acpi_handle dummy_handle;
	int sta = -1;

	status = acpi_get_handle(handle, "_STA", &dummy_handle);
	if (ACPI_SUCCESS(status)) {
		status = acpi_evaluate_integer(handle, "_STA", NULL, &tmp);
		if (ACPI_FAILURE(status)) {
			dbg("%s: _STA evaluation failure", __FUNCTION__);
			return 0;
		}
		sta = tmp;
	}

	if (sta >= 0 && !(sta & ACPI_STA_PRESENT))
		/* don't register this object */
		return 0;

	dbg("%s: _STA: 0x%x", __FUNCTION__, (unsigned int)sta);

	/* check if this bridge has ejectable slots */

	detect_ejectable_slots(handle);
	//if (detect_ejectable_slots(handle) == 0)
	//return 0;

	/* allocate per-bridge data structure and fill in */

	bridge = kmalloc(sizeof(struct pcihp_acpi_bridge), GFP_KERNEL);
	if (bridge == NULL)
		return -ENOMEM;

	memset(bridge, 0, sizeof(struct pcihp_acpi_bridge));

	if (sta >= 0)
		bridge->flags |= BRIDGE_HAS_STA;

	/* get PCI segment number */
	status = acpi_evaluate_integer(handle, "_SEG", NULL, &tmp);

	if (ACPI_SUCCESS(status)) {
		bridge->seg = tmp;
	} else {
		bridge->seg = 0;
	}

	/* get PCI bus number */
	status = acpi_evaluate_integer(handle, "_BBN", NULL, &tmp);

	if (ACPI_SUCCESS(status)) {
		bridge->bus = tmp;
	} else {
		bridge->bus = 0;
	}

	/* to be overridden when we decode _CRS	*/
	bridge->sub = bridge->bus;

	/* register all slot objects under this bridge */
	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, handle, ACPI_UINT32_MAX,
				     register_slot, bridge, NULL);

	/* decode resources */
	buffer.length = 0;
	buffer.pointer = NULL;

	
	/* TBD use new ACPI_ALLOCATE_BUFFER */
	status = acpi_get_current_resources(handle, &buffer);
	if (status != AE_BUFFER_OVERFLOW) {
		return -1;
	}

	buffer.pointer = kmalloc(buffer.length, GFP_KERNEL);
	if (!buffer.pointer) {
		return -1;
	}

	status = acpi_get_current_resources(handle, &buffer);
	if (ACPI_FAILURE(status)) {
		return -1;
	}

	decode_acpi_resource(buffer.pointer, bridge);

	/* TBD decode _HPP (hot plug parameters) */
	// decode_hpp(bridge);

	kfree(buffer.pointer);

	/* check already allocated resources */
	/* TBD */

	/* install notify handler */
	dbg("installing notify handler");
	status = acpi_install_notify_handler(handle,
					     ACPI_SYSTEM_NOTIFY,
					     handle_hotplug_event, NULL);

	if (ACPI_FAILURE(status)) {
		err("failed to register interrupt notify handler");
	}

	list_add(&bridge->list, &bridge_list);

	return 0;
}


/* callback routine to enumerate all the bridges in ACPI namespace */
static acpi_status
check_pci_bridge (acpi_handle handle, u32 lvl, void *context, void **rv)
{
	acpi_status status;
	acpi_device_info info;
	char objname[5];
	acpi_buffer buffer = { sizeof(objname), objname };

	status = acpi_get_object_info(handle, &info);
	if (ACPI_FAILURE(status)) {
		dbg("%s: failed to get bridge information", __FUNCTION__);
		return AE_OK;		/* continue */
	}

	info.hardware_id[sizeof(info.hardware_id)-1] = '\0';

	if (strcmp(info.hardware_id, ACPI_PCI_ROOT_HID) == 0) {

		acpi_get_name(handle, ACPI_SINGLE_NAME, &buffer);
		dbg("%s: found PCI root bridge[%s]", __FUNCTION__, objname);

		add_bridge(handle);
	}
	return AE_OK;
}


/* interrupt handler */
static void handle_hotplug_event (acpi_handle handle, u32 type, void *data)
{
	char objname[5];
	acpi_buffer buffer = { sizeof(objname), objname };

	acpi_get_name(handle, ACPI_SINGLE_NAME, &buffer);

	switch (type) {
	case ACPI_NOTIFY_BUS_CHECK:
		/* hot insertion/surprise removal */
		/* TBD */
		dbg("%s: Bus check notify on %s", __FUNCTION__, objname);
		break;

	case ACPI_NOTIFY_DEVICE_CHECK:
		/* TBD */
		dbg("%s: Device check notify on %s", __FUNCTION__, objname);
		break;

	case ACPI_NOTIFY_EJECT_REQUEST:
		/* eject button pushed */
		/* TBD */
		dbg("%s: Device eject notify on %s", __FUNCTION__, objname);
		break;

	default:
		warn("notify_handler: unknown event type 0x%x", type);
		break;
	}
}


/*
 * external interfaces
 */

int pcihp_acpi_glue_init (void)
{
	acpi_status status;

	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				     ACPI_UINT32_MAX, check_pci_bridge,
				     NULL, NULL);

	if (ACPI_FAILURE(status)) {
		dbg("%s: acpi_walk_namespace() failed", __FUNCTION__);
	}

	return 0;
}

static void free_all_resources (struct pcihp_acpi_bridge *bridge)
{
	struct pci_resource *res, *next;;

	for (res = bridge->free_io; res; ) {
		next = res->next;
		kfree(res);
		res = next;
	}

	for (res = bridge->free_mem; res; ) {
		next = res->next;
		kfree(res);
		res = next;
	}

	for (res = bridge->free_prefetch; res; ) {
		next = res->next;
		kfree(res);
		res = next;
	}

	for (res = bridge->free_bus; res; ) {
		next = res->next;
		kfree(res);
		res = next;
	}
}


void pcihp_acpi_glue_exit (void)
{
	struct list_head *node;
	struct pcihp_acpi_bridge *bridge;
	struct pcihp_acpi_slot *slot, *next;

	list_for_each(node, &bridge_list) {
		bridge = (struct pcihp_acpi_bridge *)node;
		slot = bridge->slots;
		while (slot) {
			next = slot->next;
			kfree(slot);
			slot = next;
		}
		free_all_resources(bridge);
		kfree(bridge);
	}
}


int pcihp_acpi_get_num_slots (void)
{
	struct list_head *node;
	struct pcihp_acpi_bridge *bridge;
	int num_slots;

	num_slots = 0;

	list_for_each(node, &bridge_list) {
		bridge = (struct pcihp_acpi_bridge *)node;
		dbg("Bus:%d num_slots:%d", bridge->bus, bridge->nr_slots);
		num_slots += bridge->nr_slots;
	}

	dbg("num_slots = %d", num_slots);
	return num_slots;
}


/*  TBD: improve performance */
struct pcihp_acpi_slot *get_slot_from_id (int id)
{
	struct list_head *node;
	struct pcihp_acpi_bridge *bridge;
	struct pcihp_acpi_slot *slot;

	list_for_each(node, &bridge_list) {
		bridge = (struct pcihp_acpi_bridge *)node;
		for (slot = bridge->slots; slot; slot = slot->next)
			if (slot->id == id)
				return slot;
	}

	/* should never happen! */
	dbg("%s: no object for id %d",__FUNCTION__, id);
	return 0;
}


/* power on slot */
int pcihp_acpi_enable_slot (struct pcihp_acpi_slot *slot)
{
	acpi_status status;

	if (slot->flags & SLOT_HAS_PS0) {
		dbg("%s: powering on bus%d/dev%d.", __FUNCTION__,
		    slot->bridge->bus, slot->device);
		status = acpi_evaluate_object(slot->handle, "_PS0", NULL, NULL);
		if (ACPI_FAILURE(status)) {
			warn("%s: powering on bus%d/dev%d failed",
			     __FUNCTION__, slot->bridge->bus, slot->device);
			return -1;
		}
	}

	return 0;
}


/* power off slot */
int pcihp_acpi_disable_slot (struct pcihp_acpi_slot *slot)
{
	acpi_status status;

	if (slot->flags & SLOT_HAS_PS3) {
		dbg("%s: powering off bus%d/dev%d.", __FUNCTION__,
		    slot->bridge->bus, slot->device);
		status = acpi_evaluate_object(slot->handle, "_PS3", NULL, NULL);
		if (ACPI_FAILURE(status)) {
			warn("%s: _PS3 on bus%d/dev%d failed",
			     __FUNCTION__, slot->bridge->bus, slot->device);
			return -1;
		}
	}

	if (slot->flags & SLOT_HAS_EJ0) {
		dbg("%s: eject bus%d/dev%d.", __FUNCTION__,
		    slot->bridge->bus, slot->device);
		status = acpi_evaluate_object(slot->handle, "_EJ0", NULL, NULL);
		if (ACPI_FAILURE(status)) {
			warn("%s: _EJ0 bus%d/dev%d failed",
			     __FUNCTION__, slot->bridge->bus, slot->device);
			return -1;
		}
	}

	/* TBD
	 * evaluate _STA to check if state is successfully changed
	 * and update status
	 */

	return 0;
}


static unsigned int get_slot_status(struct pcihp_acpi_slot *slot)
{
	acpi_status status;
	unsigned long sta;

	status = acpi_evaluate_integer(slot->handle, "_STA", NULL, &sta);

	if (ACPI_FAILURE(status)) {
		err("%s: _STA evaluation failed", __FUNCTION__);
		return 0;
	}

	return (int)sta;
}


/*
 * slot enabled:  1
 * slot disabled: 0
 */
u8 pcihp_acpi_get_power_status (struct pcihp_acpi_slot *slot)
{
	unsigned int sta;

	/* TBD
	 * . guarantee check _STA on function# 0
	 * . check configuration space before _STA?
	 */

	sta = get_slot_status(slot);

	return (sta & ACPI_STA_ENABLED) ? 1 : 0;
}


/* XXX this function is not used */
/* 
 * attention LED ON: 1
 *              OFF: 0
 */
u8 pcihp_acpi_get_attention_status (struct pcihp_acpi_slot *slot)
{
	/* TBD
	 * no direct attention led status information via ACPI
	 */

	return 0;
}


/*
 * latch closed:  1
 * latch   open:  0
 */
u8 pcihp_acpi_get_latch_status (struct pcihp_acpi_slot *slot)
{
	unsigned int sta;

	/* TBD
	 * no direct latch information via ACPI
	 */

	sta = get_slot_status(slot);

	return (sta & ACPI_STA_SHOW_IN_UI) ? 1 : 0;
}


/*
 * adapter presence : 2
 *          absence : 0
 */
u8 pcihp_acpi_get_adapter_status (struct pcihp_acpi_slot *slot)
{
	unsigned int sta;

	/* TBD
	 * is this information correct?
	 */

	sta = get_slot_status(slot);

	return (sta == 0) ? 0 : 2;
}
