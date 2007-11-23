#ifndef _RAID1_H
#define _RAID1_H

#include <linux/raid/md.h>

struct mirror_info {
	int		number;
	int		raid_disk;
	kdev_t		dev;
	int		next;
	int		sect_limit;

	/*
	 * State bits:
	 */
	int		operational;
	int		write_only;
	int		spare;

	int		used_slot;
};

struct raid1_private_data {
	mddev_t			*mddev;
	struct mirror_info	mirrors[MD_SB_DISKS];
	int			nr_disks;
	int			raid_disks;
	int			working_disks;
	int			last_used;
	unsigned long		next_sect;
	int			sect_count;
	mdk_thread_t		*thread, *resync_thread;
	int			resync_mirrors;
	struct mirror_info	*spare;
};

typedef struct raid1_private_data raid1_conf_t;

/*
 * this is the only point in the RAID code where we violate
 * C type safety. mddev->private is an 'opaque' pointer.
 */
#define mddev_to_conf(mddev) ((raid1_conf_t *) mddev->private)

/*
 * this is our 'private' 'collective' RAID1 buffer head.
 * it contains information about what kind of IO operations were started
 * for this RAID1 operation, and about their status:
 */

struct raid1_bh {
	atomic_t		remaining; /* 'have we finished' count,
					    * used from IRQ handlers
					    */
	int			cmd;
	unsigned long		state;
	mddev_t			*mddev;
	struct buffer_head	*master_bh;
	struct buffer_head	*mirror_bh [MD_SB_DISKS];
	struct buffer_head	bh_req;
	struct buffer_head	*next_retry;
};

#endif
