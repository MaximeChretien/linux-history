/*
 * Copyright (C) 2001 Momchil Velikov
 * Portions Copyright (C) 2001 Christoph Hellwig
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef _LINUX_RADIX_TREE_H
#define _LINUX_RADIX_TREE_H

#include <linux/preempt.h>

struct radix_tree_node;

struct radix_tree_root {
	unsigned int		height;
	int			gfp_mask;
	struct radix_tree_node	*rnode;
};

#define RADIX_TREE_INIT(mask)	{0, (mask), NULL}

#define RADIX_TREE(name, mask) \
	struct radix_tree_root name = RADIX_TREE_INIT(mask)

#define INIT_RADIX_TREE(root, mask)	\
do {					\
	(root)->height = 0;		\
	(root)->gfp_mask = (mask);	\
	(root)->rnode = NULL;		\
} while (0)

extern int radix_tree_insert(struct radix_tree_root *, unsigned long, void *);
extern void *radix_tree_lookup(struct radix_tree_root *, unsigned long);
extern int radix_tree_delete(struct radix_tree_root *, unsigned long);
extern unsigned int
radix_tree_gang_lookup(struct radix_tree_root *root, void **results,
			unsigned long first_index, unsigned int max_items);
int radix_tree_preload(int gfp_mask);

static inline void radix_tree_preload_end(void)
{
	preempt_enable();
}

#endif /* _LINUX_RADIX_TREE_H */
