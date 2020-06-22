/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 *  ifconfig_bus - SGI's Persistent PCI Bus Numbering.
 *
 * Copyright (C) 1992-1997, 2000-2002 Silicon Graphics, Inc.  All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/pci.h>

#include <asm/sn/sgi.h>
#include <linux/devfs_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/io.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm//sn/sn_sal.h>
#include <asm/sn/addrs.h>

#define SGI_IFCONFIG_BUS "SGI-PERSISTENT PCI BUS NUMBERING"
#define SGI_IFCONFIG_BUS_VERSION "1.0"

/*
 * Some Global definitions.
 */
devfs_handle_t ioconfig_bus_handle = NULL;
unsigned long ioconfig_bus_debug = 0;

#define IFCONFIG_BUS_DEBUG 1
#ifdef IFCONFIG_BUS_DEBUG
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif

u64 ioconfig_file = 0;
u64 ioconfig_file_size = 0;


/*
 * PCI Bus Name for PCI
 * 		 module/001c02/Ibrick/xtalk/14/pci - only 1 bus per widget
 *
 * PCI Bus Name for PCI-X
 *		 module/001c02/Ibrick/xtalk/14/pci-x/0 - PCI bus 0 on Widget 14
 *		 module/001c02/Ibrick/xtalk/14/pci-x/1 - PCI bus 1 on Widget 14
 */

struct	pcipath_to_busnum{
	unsigned int  num;			/* persistent bus number */
	unsigned int  len;			/* pci path length */
	unsigned char	path[120];		/* pci path name */
};

struct  pcipath_to_busnum *ioconfig_bus_table;
u64 ioconfig_bus_table_size = 0;


struct pcipath_to_busnum pcibus_names[256];

/*
 * highest_pcibus_num : The highest PCI bus number allocated.  Use for next 
 *			PCI Bus Number to be assigned.
 * pcibus_names_index : The starting entry of any new configuration.  It also 
 *			tracks the number of old config in the table.
 * current_index : Next free index in the table for new config.
 *		   current_index - pcibus_names_index = number new config.
 */

int	highest_pcibus_num = 0;
int	pcibus_names_index = 0; /* Start of new config */
int	current_index = 0; /* Next entry for new config */

/*
 * ioconfig_get_busnum():
 *	ioconfig_get_busnum() returns the bus number given a pci path name.
 *	If the pci path name exist in the persistent naming table, the
 *	corresponding bun number is returned.  If the pci path name does not
 *	exist, the next bus number is assigned to that path, added to the
 *	persistent naming table and the new bus num returned.
 */
void
ioconfig_get_busnum(char *pci_path, int *bus_num)
{
	struct	pcipath_to_busnum *temp;
	int index;

	*bus_num = -1;
	temp = ioconfig_bus_table;
	for (index = 0; index < current_index; ioconfig_bus_table++, index++) {
		if ( (strncmp(pci_path, ioconfig_bus_table->path, strlen(pci_path) == 0)) ){
			*bus_num = ioconfig_bus_table->num;
			return;
		}
	}

	/*
	 * This is a new path and it is not in the config table.
	 * Add it to the end.
	 */
	*bus_num = highest_pcibus_num;
	highest_pcibus_num++;
	ioconfig_bus_table->num = *bus_num;
	strcpy((char *)&(ioconfig_bus_table->path), pci_path);
	ioconfig_bus_table->len = strlen(pci_path);
	current_index++;

}

void
dump_ioconfig_table(struct pcipath_to_busnum *ioconfig_bus_table)
{

	int index = 0;
	struct pcipath_to_busnum *temp;

	temp = ioconfig_bus_table;
	while (index < pcibus_names_index) {
		printk("Bus Number %d PCI path %s\n", temp->num, temp->path);
		temp++;
		index++;
	}
		
}
/*
 * nextline
 *	This routine returns the nextline in the buffer.
 */
int nextline(char *buffer, char **next, char *line)
{

	char *temp;

	if (buffer[0] == 0x0) {
		return(0);
	}

	temp = buffer;
	while (*temp != 0) {
		*line = *temp;
		if (*temp != '\n'){
			*line = *temp;
			temp++; line++;
		} else
			break;
	}

	if (*temp == 0)
		*next = temp;
	else
		*next = ++temp;

	return(1);
}

/*
 * build_pcibus_name
 *	This routine parses the ioconfig contents read into
 *	memory by ioconfig command in EFI and builds the
 *	persistent pci bus naming table.
 */
void
build_pcibus_name(char *file_contents, struct pcipath_to_busnum *table)
{
	/*
	 * Read the whole file into memory.
	 */
	int busnum;
	int rc;
	char *name;
	char *temp;
	char *next;
	char *current;
	char *line;
	struct pcipath_to_busnum *pcibus;

	line = kmalloc(256, GFP_KERNEL);
	memset(line, 0,256);
	name = kmalloc(125, GFP_KERNEL);
	memset(name, 0, 125);
	pcibus = table;
	current = file_contents;
	while (nextline(current, &next, line)){
printk("current 0x%lx next 0x%lx\n", current, next);
		temp = line;
		/*
		 * Skip all leading Blank lines ..
		 */
		while (isspace(*temp))
			if (*temp != '\n')
				temp++;
			else
				break;

		if (*temp == '\n') {
			current = next;
			memset(line, 0, 256);
			continue;
		}
 
		/*
		 * Skip comment lines
		 */
		if (*temp == '#') {
			current = next;
			memset(line, 0, 256);
			continue;
		}

		/*
		 * Get the next free entry in the table.
		 */
		rc = sscanf(temp, "%d %s", &busnum, name);
		pcibus->num = busnum;
		pcibus->len = strlen(name);
		strcpy(&pcibus->path[0], name);
		DBG("pci number = %d pci path = %s\n", pcibus->num, pcibus->path);
		if (busnum > highest_pcibus_num)
			highest_pcibus_num = busnum;
		else 
			highest_pcibus_num = busnum; 
 
		pcibus_names_index++;
		pcibus++;
		current = next;
		memset(line, 0, 256);
	}

	DBG("highest_pcibus_num %d pcibus_names_index %d\n", highest_pcibus_num, pcibus_names_index);

	current_index = pcibus_names_index;
	kfree(line);
	kfree(name);

	return;
}

void
ioconfig_bus_init(void)
{

	struct ia64_sal_retval ret_stuff;

	DBG("ioconfig_bus_init called.\n");

	/*
	 * Make SAL call to get the address of the bus configuration table.
	 */
	ret_stuff.status = (uint64_t)0;
	ret_stuff.v0 = (uint64_t)0;
	ret_stuff.v1 = (uint64_t)0;
	ret_stuff.v2 = (uint64_t)0;
	SAL_CALL(ret_stuff, SN_SAL_BUS_CONFIG, 0, get_nasid(), 0, 0, 0, 0, 0);
	if (ret_stuff.status != 0) {
		DBG("ifconfig_bus_init: No Address given\n");
	}

	ioconfig_file = ret_stuff.v0;
	ioconfig_file_size = ret_stuff.v1;
	DBG("ioconfig_bus_init: ioconfig_file %p %d\n", 
		(void *)ioconfig_file, (int)ioconfig_file_size);

	/*
	 * Convert the address to a Cache Address.
	 */
	ioconfig_file = (CACHEABLE_MEM_SPACE | 
		(ioconfig_file & TO_PHYS_MASK));

	ioconfig_bus_table = kmalloc( 512*sizeof(struct pcipath_to_busnum), 
				GFP_KERNEL );

	DBG("ioconfig_bus_init: Kernel virtual ioconfig_file %p ioconfig_bus_table %p\n", 
		(void *)ioconfig_file, (void *)ioconfig_bus_table);

	(void) build_pcibus_name((char *)ioconfig_file, ioconfig_bus_table);

	(void) dump_ioconfig_table(ioconfig_bus_table);

}

/*
 * ioconfig_bus_open - Opens the special device node "/dev/hw/.ioconfig_bus".
 */
static int ioconfig_bus_open(struct inode * inode, struct file * filp)
{
	if (ioconfig_bus_debug) {
        	printk("ioconfig_bus_open called.\n");
	}

        return(0);

}

/*
 * ioconfig_bus_close - Closes the special device node "/dev/hw/.ioconfig_bus".
 */
static int ioconfig_bus_close(struct inode * inode, struct file * filp)
{

	if (ioconfig_bus_debug) {
        	printk("ioconfig_bus_close called.\n");
	}

        return(0);
}

struct file_operations ioconfig_bus_fops = {
	/* ioctl:ioconfig_bus_ioctl, ioctl */
	open:ioconfig_bus_open,		/* open */
	release:ioconfig_bus_close	/* release */
};


/*
 * init_ifconfig_net() - Boot time initialization.  Ensure that it is called 
 *	after devfs has been initialized.
 *
 */
int init_ioconfig_bus(void)
{
	ioconfig_bus_handle = NULL;
	ioconfig_bus_handle = hwgraph_register(hwgraph_root, ".ioconfig_bus",
			0, DEVFS_FL_AUTO_DEVNUM,
			0, 0,
			S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, 0, 0,
			&ioconfig_bus_fops, NULL);

	if (ioconfig_bus_handle == NULL) {
		panic("Unable to create SGI PERSISTENT BUS NUMBERING Driver.\n");
	}

	return(0);

}
