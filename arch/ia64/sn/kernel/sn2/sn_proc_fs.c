/*
 *
 * Copyright (C) 2000-2002 Silicon Graphics, Inc. All rights reserved.
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

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <asm/sn/sn_sal.h>


static int partition_id_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data) {

	return sprintf(page, "%d\n", sn_local_partid());
}

struct proc_dir_entry * sgi_proc_dir = NULL;

void
register_sn_partition_id(void) {
	struct proc_dir_entry *entry;

	if (!sgi_proc_dir) {
		sgi_proc_dir = proc_mkdir("sgi_sn", 0);
	}
	entry = create_proc_entry("partition_id", 0444, sgi_proc_dir);
	if (entry) {
		entry->nlink = 1;
		entry->data = 0;
		entry->read_proc = partition_id_read_proc;
		entry->write_proc = NULL;
	}
}

static int
system_serial_number_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data) {
	return sprintf(page, "%s\n", sn_system_serial_number());
}

static int
licenseID_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data) {
	return sprintf(page, "0x%lx\n",sn_partition_serial_number_val());
}

void
register_sn_serial_numbers(void) {
	struct proc_dir_entry *entry;

	if (!sgi_proc_dir) {
		sgi_proc_dir = proc_mkdir("sgi_sn", 0);
	}
	entry = create_proc_entry("system_serial_number", 0444, sgi_proc_dir);
	if (entry) {
		entry->nlink = 1;
		entry->data = 0;
		entry->read_proc = system_serial_number_read_proc;
		entry->write_proc = NULL;
	}
	entry = create_proc_entry("licenseID", 0444, sgi_proc_dir);
	if (entry) {
		entry->nlink = 1;
		entry->data = 0;
		entry->read_proc = licenseID_read_proc;
		entry->write_proc = NULL;
	}
}

#endif /* CONFIG_PROC_FS */
