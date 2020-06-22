/*
 * ACPI PCI HotPlug Utility functions
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
 * Send feedback to <gregkh@us.ibm.com>,<h-aono@ap.jp.nec.com>
 *
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <linux/string.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/spinlock.h>

#include <linux/ioctl.h>
#include <linux/fcntl.h>

#include <linux/list.h>

#include <asm/uaccess.h>

#define _LINUX                          /* for acpi subcomponent */

#include "include/acpi.h"

#include "pci_hotplug.h"
#include "pchihp_acpi.h"

#if !defined(CONFIG_HOTPLUG_PCI_MODULE)
	#define MY_NAME "pci_hotplug"
#else
	#define MY_NAME THIS_MODULE->name
#endif

#define dbg(fmt, arg...) do { if (debug) printk(KERN_WARNING "%s: "__FUNCTION__": " fmt , MY_NAME , ## arg); } while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format , MY_NAME , ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format , MY_NAME , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format , MY_NAME , ## arg)


/* local variables */
static int debug = 0;

/*
 * sort_by_size
 *
 * Sorts nodes on the list by their length.
 * Smallest first.
 *
 */
static int sort_by_size(struct pci_resource **head)
{
	struct pci_resource *current_res;
	struct pci_resource *next_res;
	int out_of_order = 1;

	if (!(*head))
		return(1);

	if (!((*head)->next))
		return(0);

	while (out_of_order) {
		out_of_order = 0;

		// Special case for swapping list head
		if (((*head)->next) &&
		    ((*head)->length > (*head)->next->length)) {
			out_of_order++;
			current_res = *head;
			*head = (*head)->next;
			current_res->next = (*head)->next;
			(*head)->next = current_res;
		}

		current_res = *head;

		while (current_res->next && current_res->next->next) {
			if (current_res->next->length > current_res->next->next->length) {
				out_of_order++;
				next_res = current_res->next;
				current_res->next = current_res->next->next;
				current_res = current_res->next;
				next_res->next = current_res->next;
				current_res->next = next_res;
			} else
				current_res = current_res->next;
		}
	}  // End of out_of_order loop

	return(0);
}


/*
 * sort_by_max_size
 *
 * Sorts nodes on the list by their length.
 * Largest first.
 *
 */
static int sort_by_max_size(struct pci_resource **head)
{
	struct pci_resource *current_res;
	struct pci_resource *next_res;
	int out_of_order = 1;

	if (!(*head))
		return(1);

	if (!((*head)->next))
		return(0);

	while (out_of_order) {
		out_of_order = 0;

		// Special case for swapping list head
		if (((*head)->next) &&
		    ((*head)->length < (*head)->next->length)) {
			out_of_order++;
			current_res = *head;
			*head = (*head)->next;
			current_res->next = (*head)->next;
			(*head)->next = current_res;
		}

		current_res = *head;

		while (current_res->next && current_res->next->next) {
			if (current_res->next->length < current_res->next->next->length) {
				out_of_order++;
				next_res = current_res->next;
				current_res->next = current_res->next->next;
				current_res = current_res->next;
				next_res->next = current_res->next;
				current_res->next = next_res;
			} else
				current_res = current_res->next;
		}
	}  // End of out_of_order loop

	return(0);
}

/*
 * get_io_resource
 *
 * this function sorts the resource list by size and then
 * returns the first node of "size" length that is not in the
 * ISA aliasing window.  If it finds a node larger than "size"
 * it will split it up.
 *
 * size must be a power of two.
 */
struct pci_resource *hotplug_get_io_resource (struct pci_resource **head, u32 size)
{
	struct pci_resource *prevnode;
	struct pci_resource *node;
	struct pci_resource *split_node;
	u32 temp_dword;

	if (!(*head))
		return(NULL);

	if ( hotplug_resource_sort_and_combine(head) )
		return(NULL);

	if ( sort_by_size(head) )
		return(NULL);

	for (node = *head; node; node = node->next) {
		if (node->length < size)
			continue;

		if (node->base & (size - 1)) {
			// this one isn't base aligned properly
			// so we'll make a new entry and split it up
			temp_dword = (node->base | (size-1)) + 1;

			// Short circuit if adjusted size is too small
			if ((node->length - (temp_dword - node->base)) < size)
				continue;

			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);

			if (!split_node)
				return(NULL);

			split_node->base = node->base;
			split_node->length = temp_dword - node->base;
			node->base = temp_dword;
			node->length -= split_node->length;

			// Put it in the list
			split_node->next = node->next;
			node->next = split_node;
		} // End of non-aligned base

		// Don't need to check if too small since we already did
		if (node->length > size) {
			// this one is longer than we need
			// so we'll make a new entry and split it up
			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);

			if (!split_node)
				return(NULL);

			split_node->base = node->base + size;
			split_node->length = node->length - size;
			node->length = size;

			// Put it in the list
			split_node->next = node->next;
			node->next = split_node;
		}  // End of too big on top end

		// For IO make sure it's not in the ISA aliasing space
		if (node->base & 0x300L)
			continue;

		// If we got here, then it is the right size
		// Now take it out of the list
		if (*head == node) {
			*head = node->next;
		} else {
			prevnode = *head;
			while (prevnode->next != node)
				prevnode = prevnode->next;

			prevnode->next = node->next;
		}
		node->next = NULL;
		// Stop looping
		break;
	}

	return(node);
}


/*
 * get_max_resource
 *
 * Gets the largest node that is at least "size" big from the
 * list pointed to by head.  It aligns the node on top and bottom
 * to "size" alignment before returning it.
 */
struct pci_resource *hotplug_get_max_resource (struct pci_resource **head, u32 size)
{
	struct pci_resource *max;
	struct pci_resource *temp;
	struct pci_resource *split_node;
	u32 temp_dword;

	if (!(*head))
		return(NULL);

	if (hotplug_resource_sort_and_combine(head))
		return(NULL);

	if (sort_by_max_size(head))
		return(NULL);

	for (max = *head;max; max = max->next) {

		// If not big enough we could probably just bail, 
		// instead we'll continue to the next.
		if (max->length < size)
			continue;

		if (max->base & (size - 1)) {
			// this one isn't base aligned properly
			// so we'll make a new entry and split it up
			temp_dword = (max->base | (size-1)) + 1;

			// Short circuit if adjusted size is too small
			if ((max->length - (temp_dword - max->base)) < size)
				continue;

			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);

			if (!split_node)
				return(NULL);

			split_node->base = max->base;
			split_node->length = temp_dword - max->base;
			max->base = temp_dword;
			max->length -= split_node->length;

			// Put it next in the list
			split_node->next = max->next;
			max->next = split_node;
		}

		if ((max->base + max->length) & (size - 1)) {
			// this one isn't end aligned properly at the top
			// so we'll make a new entry and split it up
			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);

			if (!split_node)
				return(NULL);
			temp_dword = ((max->base + max->length) & ~(size - 1));
			split_node->base = temp_dword;
			split_node->length = max->length + max->base
					     - split_node->base;
			max->length -= split_node->length;

			// Put it in the list
			split_node->next = max->next;
			max->next = split_node;
		}

		// Make sure it didn't shrink too much when we aligned it
		if (max->length < size)
			continue;

		// Now take it out of the list
		temp = (struct pci_resource*) *head;
		if (temp == max) {
			*head = max->next;
		} else {
			while (temp && temp->next != max) {
				temp = temp->next;
			}

			temp->next = max->next;
		}

		max->next = NULL;
		return(max);
	}

	// If we get here, we couldn't find one
	return(NULL);
}


/*
 * get_resource
 *
 * this function sorts the resource list by size and then
 * returns the first node of "size" length.  If it finds a node
 * larger than "size" it will split it up.
 *
 * size must be a power of two.
 */
struct pci_resource *hotplug_get_resource (struct pci_resource **head, u32 size)
{
	struct pci_resource *prevnode;
	struct pci_resource *node;
	struct pci_resource *split_node;
	u32 temp_dword;

	if (!(*head))
		return(NULL);

	if ( hotplug_resource_sort_and_combine(head) )
		return(NULL);

	if ( sort_by_size(head) )
		return(NULL);

	for (node = *head; node; node = node->next) {
		dbg(__FUNCTION__": req_size =%x node=%p, base=%x, length=%x\n",
		    size, node, node->base, node->length);
		if (node->length < size)
			continue;

		if (node->base & (size - 1)) {
			dbg(__FUNCTION__": not aligned\n");
			// this one isn't base aligned properly
			// so we'll make a new entry and split it up
			temp_dword = (node->base | (size-1)) + 1;

			// Short circuit if adjusted size is too small
			if ((node->length - (temp_dword - node->base)) < size)
				continue;

			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);

			if (!split_node)
				return(NULL);

			split_node->base = node->base;
			split_node->length = temp_dword - node->base;
			node->base = temp_dword;
			node->length -= split_node->length;

			// Put it in the list
			split_node->next = node->next;
			node->next = split_node;
		} // End of non-aligned base

		// Don't need to check if too small since we already did
		if (node->length > size) {
			dbg(__FUNCTION__": too big\n");
			// this one is longer than we need
			// so we'll make a new entry and split it up
			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);

			if (!split_node)
				return(NULL);

			split_node->base = node->base + size;
			split_node->length = node->length - size;
			node->length = size;

			// Put it in the list
			split_node->next = node->next;
			node->next = split_node;
		}  // End of too big on top end

		dbg(__FUNCTION__": got one!!!\n");
		// If we got here, then it is the right size
		// Now take it out of the list
		if (*head == node) {
			*head = node->next;
		} else {
			prevnode = *head;
			while (prevnode->next != node)
				prevnode = prevnode->next;

			prevnode->next = node->next;
		}
		node->next = NULL;
		// Stop looping
		break;
	}
	return(node);
}

/*
 * get_resource_with_base
 *
 * this function 
 * returns the first node of "size" length located at specified base address.
 * If it finds a node larger than "size" it will split it up.
 *
 * size must be a power of two.
 */
struct pci_resource *hotplug_get_resource_with_base (struct pci_resource **head, u32 base, u32 size)
{
	struct pci_resource *prevnode;
	struct pci_resource *node;
	struct pci_resource *split_node;
	u32 temp_dword;

	if (!(*head))
		return(NULL);

	if ( hotplug_resource_sort_and_combine(head) )
		return(NULL);

	for (node = *head; node; node = node->next) {
		dbg(": 1st req_base=%x req_size =%x node=%p, base=%x, length=%x\n",
		    base, size, node, node->base, node->length);
		if (node->base > base)
			continue;

		if ((node->base + node->length) < (base + size))
			continue;

		if (node->base < base) {
			dbg(": split 1\n");
			// this one isn't base aligned properly
			// so we'll make a new entry and split it up
			temp_dword = base;

			// Short circuit if adjusted size is too small
			if ((node->length - (temp_dword - node->base)) < size)
				continue;

			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);

			if (!split_node)
				return(NULL);

			split_node->base = node->base;
			split_node->length = temp_dword - node->base;
			node->base = temp_dword;
			node->length -= split_node->length;

			// Put it in the list
			split_node->next = node->next;
			node->next = split_node;
		}

		dbg(": 2st req_base=%x req_size =%x node=%p, base=%x, length=%x\n",
		    base, size, node, node->base, node->length);

		// Don't need to check if too small since we already did
		if (node->length >= size) {
			dbg(": split 2\n");
			// this one is longer than we need
			// so we'll make a new entry and split it up
			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);

			if (!split_node)
				return(NULL);

			split_node->base = node->base + size;
			split_node->length = node->length - size;
			node->length = size;

			// Put it in the list
			split_node->next = node->next;
			node->next = split_node;
		}  // End of too big on top end

		dbg(": got one!!!\n");
		// If we got here, then it is the right size
		// Now take it out of the list
		if (*head == node) {
			*head = node->next;
		} else {
			prevnode = *head;
			while (prevnode->next != node)
				prevnode = prevnode->next;

			prevnode->next = node->next;
		}
		node->next = NULL;
		// Stop looping
		break;
	}
	return(node);
}

/*
 * hotplug_resource_sort_and_combine
 *
 * Sorts all of the nodes in the list in ascending order by
 * their base addresses.  Also does garbage collection by
 * combining adjacent nodes.
 *
 * returns 0 if success
 */
int hotplug_resource_sort_and_combine(struct pci_resource **head)
{
	struct pci_resource *node1;
	struct pci_resource *node2;
	int out_of_order = 1;

	dbg(__FUNCTION__": head = %p, *head = %p\n", head, *head);

	if (!(*head))
		return(1);

	dbg("*head->next = %p\n",(*head)->next);

	if (!(*head)->next)
		return(0);	/* only one item on the list, already sorted! */

	dbg("*head->base = 0x%x\n",(*head)->base);
	dbg("*head->next->base = 0x%x\n",(*head)->next->base);
	while (out_of_order) {
		out_of_order = 0;

		// Special case for swapping list head
		if (((*head)->next) &&
		    ((*head)->base > (*head)->next->base)) {
			node1 = *head;
			(*head) = (*head)->next;
			node1->next = (*head)->next;
			(*head)->next = node1;
			out_of_order++;
		}

		node1 = (*head);

		while (node1->next && node1->next->next) {
			if (node1->next->base > node1->next->next->base) {
				out_of_order++;
				node2 = node1->next;
				node1->next = node1->next->next;
				node1 = node1->next;
				node2->next = node1->next;
				node1->next = node2;
			} else
				node1 = node1->next;
		}
	}  // End of out_of_order loop

	node1 = *head;

	while (node1 && node1->next) {
		if ((node1->base + node1->length) == node1->next->base) {
			// Combine
			dbg("8..\n");
			node1->length += node1->next->length;
			node2 = node1->next;
			node1->next = node1->next->next;
			kfree(node2);
		} else
			node1 = node1->next;
	}

	return(0);
}

/*
EXPORT_SYMBOL(hotplug_get_io_resource);
EXPORT_SYMBOL(hotplug_get_max_resource);
EXPORT_SYMBOL(hotplug_get_resource);
EXPORT_SYMBOL(hotplug_resource_sort_and_combine);
*/
