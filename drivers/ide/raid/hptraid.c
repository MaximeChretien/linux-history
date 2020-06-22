/*
   hptraid.c  Copyright (C) 2001 Red Hat, Inc. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
   
   Authors: 	Arjan van de Ven <arjanv@redhat.com>

   Based on work
   	Copyleft  (C) 2001 by Wilfried Weissmann <wweissmann@gmx.at>
	Copyright (C) 1994-96 Marc ZYNGIER <zyngier@ufr-info-p7.ibp.fr>
   Based on work done by Søren Schmidt for FreeBSD

   
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/genhd.h>
#include <linux/ioctl.h>

#include <linux/ide.h>
#include <asm/uaccess.h>

#include "ataraid.h"

static int hptraid_open(struct inode * inode, struct file * filp);
static int hptraid_release(struct inode * inode, struct file * filp);
static int hptraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int hptraidspan_make_request (request_queue_t *q, int rw, struct buffer_head * bh);
static int hptraid0_make_request (request_queue_t *q, int rw, struct buffer_head * bh);
static int hptraid1_make_request (request_queue_t *q, int rw, struct buffer_head * bh);



struct hptdisk {
	kdev_t	device;
	unsigned long sectors;
	struct block_device *bdev;
	unsigned long last_pos;
};

struct hptraid {
	unsigned int stride;
	unsigned int disks;
	unsigned long sectors;
        u_int32_t magic_0;
	struct geom geom;
	
	struct hptdisk disk[8];
	unsigned long cutoff[8];
	unsigned int cutoff_disks[8];	
};

struct hptraid_dev {
	int major;
	int minor;
	int device;
};

static struct hptraid_dev devlist[]=
{

	{IDE0_MAJOR,  0, -1},
	{IDE0_MAJOR, 64, -1},
	{IDE1_MAJOR,  0, -1},
	{IDE1_MAJOR, 64, -1},
	{IDE2_MAJOR,  0, -1},
	{IDE2_MAJOR, 64, -1},
	{IDE3_MAJOR,  0, -1},
	{IDE3_MAJOR, 64, -1},
	{IDE4_MAJOR,  0, -1},
	{IDE4_MAJOR, 64, -1},
	{IDE5_MAJOR,  0, -1},
	{IDE5_MAJOR, 64, -1},
	{IDE6_MAJOR,  0, -1},
	{IDE6_MAJOR, 64, -1}
};

static struct raid_device_operations hptraidspan_ops = {
	open:                   hptraid_open,
	release:                hptraid_release,
	ioctl:			hptraid_ioctl,
	make_request:		hptraidspan_make_request
};

static struct raid_device_operations hptraid0_ops = {
	open:                   hptraid_open,
	release:                hptraid_release,
	ioctl:			hptraid_ioctl,
	make_request:		hptraid0_make_request
};

static struct raid_device_operations hptraid1_ops = {
	open:                   hptraid_open,
	release:                hptraid_release,
	ioctl:			hptraid_ioctl,
	make_request:		hptraid1_make_request
};

static struct hptraid raid[14];

static int hptraid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	unsigned char val;
	unsigned long sectors;
	
	if (!inode || !inode->i_rdev) 	
		return -EINVAL;

	minor = MINOR(inode->i_rdev)>>SHIFT;
	
	switch (cmd) {
         	case BLKGETSIZE:   /* Return device size */
			if (!arg)  return -EINVAL;
			sectors = ataraid_gendisk.part[MINOR(inode->i_rdev)].nr_sects;
			if (MINOR(inode->i_rdev)&15)
				return put_user(sectors, (unsigned long *) arg);
			return put_user(raid[minor].sectors , (unsigned long *) arg);
			break;
			

		case HDIO_GETGEO:
		{
			struct hd_geometry *loc = (struct hd_geometry *) arg;
			unsigned short bios_cyl;
			
			if (!loc) return -EINVAL;
			val = 255;
			if (put_user(val, (byte *) &loc->heads)) return -EFAULT;
			val=63;
			if (put_user(val, (byte *) &loc->sectors)) return -EFAULT;
			bios_cyl = raid[minor].sectors/63/255;
			if (put_user(bios_cyl, (unsigned short *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)ataraid_gendisk.part[MINOR(inode->i_rdev)].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}

		case HDIO_GETGEO_BIG:
		{
			struct hd_big_geometry *loc = (struct hd_big_geometry *) arg;
			unsigned int bios_cyl;
			if (!loc) return -EINVAL;
			val = 255;
			if (put_user(val, (byte *) &loc->heads)) return -EFAULT;
			val = 63;
			if (put_user(val, (byte *) &loc->sectors)) return -EFAULT;
			bios_cyl = raid[minor].sectors/63/255;
			if (put_user(bios_cyl, (unsigned int *) &loc->cylinders)) return -EFAULT;
			if (put_user((unsigned)ataraid_gendisk.part[MINOR(inode->i_rdev)].start_sect,
				(unsigned long *) &loc->start)) return -EFAULT;
			return 0;
		}
			
		default:
			return blk_ioctl(inode->i_rdev, cmd, arg);
	};

	return 0;
}


static int hptraidspan_make_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	unsigned long rsect;
	unsigned int disk;
	int device;
	struct hptraid *thisraid;

	rsect = bh->b_rsector;

	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	thisraid = &raid[device];

	/* Partitions need adding of the start sector of the partition to the requested sector */
	
	rsect += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect;

	for (disk=0;disk<thisraid->disks;disk++) {
		if (disk==1)
			rsect+=10;
			// the "on next disk" contition check is a bit odd
		if (thisraid->disk[disk].sectors > rsect+1)
			break;
		rsect-=thisraid->disk[disk].sectors-(disk?11:1);
	}

		// request spans over 2 disks => request must be split
	if(rsect+bh->b_size/512 >= thisraid->disk[disk].sectors)
		return -1;
	
	/*
	 * The new BH_Lock semantics in ll_rw_blk.c guarantee that this
	 * is the only IO operation happening on this bh.
	 */
	 
	bh->b_rdev = thisraid->disk[disk].device;
	bh->b_rsector = rsect;

	/*
	 * Let the main block layer submit the IO and resolve recursion:
	 */
	return 1;
}

static int hptraid0_make_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	unsigned long rsect;
	unsigned long rsect_left,rsect_accum = 0;
	unsigned long block;
	unsigned int disk=0,real_disk=0;
	int i;
	int device;
	struct hptraid *thisraid;

	rsect = bh->b_rsector;

	/* Ok. We need to modify this sector number to a new disk + new sector number. 
	 * If there are disks of different sizes, this gets tricky. 
	 * Example with 3 disks (1Gb, 4Gb and 5 GB):
	 * The first 3 Gb of the "RAID" are evenly spread over the 3 disks.
	 * Then things get interesting. The next 2Gb (RAID view) are spread across disk 2 and 3
	 * and the last 1Gb is disk 3 only.
	 *
	 * the way this is solved is like this: We have a list of "cutoff" points where everytime
	 * a disk falls out of the "higher" count, we mark the max sector. So once we pass a cutoff
	 * point, we have to divide by one less.
	 */
	
	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	thisraid = &raid[device];
	if (thisraid->stride==0)
		thisraid->stride=1;

	/* Partitions need adding of the start sector of the partition to the requested sector */
	
	rsect += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect;

	/* Woops we need to split the request to avoid crossing a stride barrier */
	if ((rsect/thisraid->stride) != ((rsect+(bh->b_size/512)-1)/thisraid->stride)) {
		return -1;
	}
			
	rsect_left = rsect;
	
	for (i=0;i<8;i++) {
		if (thisraid->cutoff_disks[i]==0)
			break;
		if (rsect > thisraid->cutoff[i]) {
			/* we're in the wrong area so far */
			rsect_left -= thisraid->cutoff[i];
			rsect_accum += thisraid->cutoff[i]/thisraid->cutoff_disks[i];
		} else {
			block = rsect_left / thisraid->stride;
			disk = block % thisraid->cutoff_disks[i];
			block = (block / thisraid->cutoff_disks[i]) * thisraid->stride;
			rsect = rsect_accum + (rsect_left % thisraid->stride) + block;
			break;
		}
	}
	
	for (i=0;i<8;i++) {
		if ((disk==0) && (thisraid->disk[i].sectors > rsect_accum)) {
			real_disk = i;
			break;
		}
		if ((disk>0) && (thisraid->disk[i].sectors >= rsect_accum)) {
			disk--;
		}
		
	}
	disk = real_disk;
	
	/* All but the first disk have a 10 sector offset */
	if (i>0)
		rsect+=10;
		
	
	/*
	 * The new BH_Lock semantics in ll_rw_blk.c guarantee that this
	 * is the only IO operation happening on this bh.
	 */
	 
	bh->b_rdev = thisraid->disk[disk].device;
	bh->b_rsector = rsect;

	/*
	 * Let the main block layer submit the IO and resolve recursion:
	 */
	return 1;
}

static int hptraid1_read_request (request_queue_t *q, int rw, struct buffer_head * bh)
{
	int device;
	int dist;
	int bestsofar,bestdist,i;
	static int previous;

	/* Reads are simple in principle. Pick a disk and go. 
	   Initially I cheat by just picking the one which the last known
	   head position is closest by.
	   Later on, online/offline checking and performance needs adding */
	
	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	bh->b_rsector += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect;

	bestsofar = 0; 
	bestdist = raid[device].disk[0].last_pos - bh->b_rsector;
	if (bestdist<0) 
		bestdist=-bestdist;
	if (bestdist>4095)
		bestdist=4095;

	for (i=1 ; i<raid[device].disks; i++) {
		dist = raid[device].disk[i].last_pos - bh->b_rsector;
		if (dist<0) 
			dist = -dist;
		if (dist>4095)
			dist=4095;
		
		if (bestdist==dist) {  /* it's a tie; try to do some read balancing */
			if ((previous>bestsofar)&&(previous<=i))  
				bestsofar = i;
			previous = (previous + 1) % raid[device].disks;
		} else if (bestdist>dist) {
			bestdist = dist;
			bestsofar = i;
		}
	
	}
	
	bh->b_rsector += bestsofar?10:0;
	bh->b_rdev = raid[device].disk[bestsofar].device; 
	raid[device].disk[bestsofar].last_pos = bh->b_rsector+(bh->b_size>>9);

	/*
	 * Let the main block layer submit the IO and resolve recursion:
	 */
                          	
	return 1;
}

static int hptraid1_write_request(request_queue_t *q, int rw, struct buffer_head * bh)
{
	struct buffer_head *bh1;
	struct ataraid_bh_private *private;
	int device;
	int i;

	device = (bh->b_rdev >> SHIFT)&MAJOR_MASK;
	private = ataraid_get_private();
	if (private==NULL)
		BUG();

	private->parent = bh;
	
	atomic_set(&private->count,raid[device].disks);


	for (i = 0; i< raid[device].disks; i++) { 
		bh1=ataraid_get_bhead();
		/* If this ever fails we're doomed */
		if (!bh1)
			BUG();
	
		/* dupe the bufferhead and update the parts that need to be different */
		memcpy(bh1, bh, sizeof(*bh));
		
		bh1->b_end_io = ataraid_end_request;
		bh1->b_private = private;
		bh1->b_rsector += ataraid_gendisk.part[MINOR(bh->b_rdev)].start_sect+(i==0?0:10); /* partition offset */
		bh1->b_rdev = raid[device].disk[i].device;

		/* update the last known head position for the drive */
		raid[device].disk[i].last_pos = bh1->b_rsector+(bh1->b_size>>9);

		generic_make_request(rw,bh1);
	}
	return 0;
}

static int hptraid1_make_request (request_queue_t *q, int rw, struct buffer_head * bh) {
	/* Read and Write are totally different cases; split them totally here */
	if (rw==READA)
		rw = READ;
	
	if (rw==READ)
		return hptraid1_read_request(q,rw,bh);
	else
		return hptraid1_write_request(q,rw,bh);
}

#include "hptraid.h"

static int read_disk_sb (int major, int minor, unsigned char *buffer,int bufsize)
{
	int ret = -EINVAL;
	struct buffer_head *bh = NULL;
	kdev_t dev = MKDEV(major,minor);
	
	if (blksize_size[major]==NULL)	 /* device doesn't exist */
		return -EINVAL;
	

	/* Superblock is at 4096+412 bytes */
	set_blocksize (dev, 4096);
	bh = bread (dev, 1, 4096);

	
	if (bh) {
		memcpy (buffer, bh->b_data, bufsize);
	} else {
		printk(KERN_ERR "hptraid: Error reading superblock.\n");
		goto abort;
	}
	ret = 0;
abort:
	if (bh)
		brelse (bh);
	return ret;
}

static unsigned long maxsectors (int major,int minor)
{
	unsigned long lba = 0;
	kdev_t dev;
	ide_drive_t *ideinfo;
	
	dev = MKDEV(major,minor);
	ideinfo = get_info_ptr (dev);
	if (ideinfo==NULL)
		return 0;
	
	
	/* first sector of the last cluster */
	if (ideinfo->head==0) 
		return 0;
	if (ideinfo->sect==0)
		return 0;
	lba = (ideinfo->capacity);

	return lba;
}

static void __init probedisk(struct hptraid_dev *disk, int device, u_int8_t type)
{
	int i;
        struct highpoint_raid_conf *prom;
	static unsigned char block[4096];
	struct block_device *bdev;
	
 	if (disk->device != -1)	/* disk is occupied? */
 		return;
 
 	if (maxsectors(disk->major,disk->minor)==0)
		return;
	
        if (read_disk_sb(disk->major,disk->minor,(unsigned char*)&block,sizeof(block)))
        	return;
                                                                                                                 
        prom = (struct highpoint_raid_conf*)&block[512];
                
        if (prom->magic!=  0x5a7816f0)
        	return;
        switch (prom->type) {
		case HPT_T_SPAN:
		case HPT_T_RAID_0:
		case HPT_T_RAID_1:
			if(prom->type != type)
				return;
			break;
		default:
			printk(KERN_INFO "hptraid: only SPAN, RAID0 and RAID1 is currently supported \n");
			return;
        }

 		/* disk from another array? */
 	if(raid[device].disks && prom->magic_0 != raid[device].magic_0)
 		return;

	i = prom->disk_number;
	if (i<0)
		return;
	if (i>8) 
		return;

	bdev = bdget(MKDEV(disk->major,disk->minor));
	if (bdev && blkdev_get(bdev,FMODE_READ|FMODE_WRITE,0,BDEV_RAW) == 0) {
        	int j=0;
        	struct gendisk *gd;
		raid[device].disk[i].bdev = bdev;
        	/* This is supposed to prevent others from stealing our underlying disks */
		/* now blank the /proc/partitions table for the wrong partition table,
		   so that scripts don't accidentally mount it and crash the kernel */
		 /* XXX: the 0 is an utter hack  --hch */
		gd=get_gendisk(MKDEV(disk->major, 0));
		if (gd!=NULL) {
 			if (gd->major==disk->major)
 				for (j=1+(disk->minor<<gd->minor_shift);
					j<((disk->minor+1)<<gd->minor_shift);
					j++) gd->part[j].nr_sects=0;					
		}
        }
	raid[device].disk[i].device = MKDEV(disk->major,disk->minor);
	raid[device].disk[i].sectors = maxsectors(disk->major,disk->minor);
	raid[device].stride = (1<<prom->raid0_shift);
	raid[device].disks = prom->raid_disks;
	raid[device].sectors = prom->total_secs-(prom->total_secs%(255*63));
	raid[device].sectors += raid[device].sectors&1?1:0;
	raid[device].magic_0=prom->magic_0;
	disk->device=device;
			
}

static void __init fill_cutoff(int device)
{
	int i,j;
	unsigned long smallest;
	unsigned long bar;
	int count;
	
	bar = 0;
	for (i=0;i<8;i++) {
		smallest = ~0;
		for (j=0;j<8;j++) 
			if ((raid[device].disk[j].sectors < smallest) && (raid[device].disk[j].sectors>bar))
				smallest = raid[device].disk[j].sectors;
		count = 0;
		for (j=0;j<8;j++) 
			if (raid[device].disk[j].sectors >= smallest)
				count++;
		
		smallest = smallest * count;		
		bar = smallest;
		raid[device].cutoff[i] = smallest;
		raid[device].cutoff_disks[i] = count;
		
	}
}


static __init int hptraid_init_one(int device, u_int8_t type)
{
	int i,count;

	memset(raid+device, 0, sizeof(struct hptraid));
	for(i=0; i < 14; i++) {
		probedisk(devlist+i, device, type);
	}

	if(type == HPT_T_RAID_0)
		fill_cutoff(device);
	
	/* Initialize the gendisk structure */
	
	ataraid_register_disk(device,raid[device].sectors);

	count=0;
		
	for (i=0;i<8;i++) {
		if (raid[device].disk[i].device!=0) {
			printk(KERN_INFO "Drive %i is %li Mb \n",
				i,raid[device].disk[i].sectors/2048);
			count++;
		}
	}
	if (count) {
		printk(KERN_INFO "Raid array consists of %i drives. \n",count);
		return 0;
	} else {
		printk(KERN_INFO "No raid array found\n");
		return -ENODEV;
	}
	
}

static __init int hptraid_init(void)
{
 	int retval,device,count=0;
  	
	printk(KERN_INFO "Highpoint HPT370 Softwareraid driver for linux version 0.01-ww1\n");

 	do
 	{
 		device=ataraid_get_device(&hptraid0_ops);
 		if (device<0)
 			return (count?0:-ENODEV);
 		retval = hptraid_init_one(device, HPT_T_RAID_0);
 		if (retval)
 			ataraid_release_device(device);
 		else
 			count++;
 	} while(!retval);
 	do
 	{
 		device=ataraid_get_device(&hptraid1_ops);
 		if (device<0)
 			return (count?0:-ENODEV);
 		retval = hptraid_init_one(device, HPT_T_RAID_1);
 		if (retval)
 			ataraid_release_device(device);
 		else
 			count++;
 	} while(!retval);
 	do
 	{
 		device=ataraid_get_device(&hptraidspan_ops);
 		if (device<0)
 			return (count?0:-ENODEV);
 		retval = hptraid_init_one(device, HPT_T_SPAN);
 		if (retval)
 			ataraid_release_device(device);
 		else
 			count++;
 	} while(!retval);
 	return (count?0:retval);
}

static void __exit hptraid_exit (void)
{
	int i,device;
	for (device = 0; device<16; device++) {
		for (i=0;i<8;i++)  {
			struct block_device *bdev = raid[device].disk[i].bdev;
			raid[device].disk[i].bdev = NULL;
			if (bdev)
				blkdev_put(bdev, BDEV_RAW);
		}       
		if (raid[device].sectors)
			ataraid_release_device(device);
	}
}

static int hptraid_open(struct inode * inode, struct file * filp) 
{
	MOD_INC_USE_COUNT;
	return 0;
}
static int hptraid_release(struct inode * inode, struct file * filp)
{	
	MOD_DEC_USE_COUNT;
	return 0;
}

module_init(hptraid_init);
module_exit(hptraid_exit);
MODULE_LICENSE("GPL");
