/*
 *  sr.c Copyright (C) 1992 David Giller
 *           Copyright (C) 1993, 1994, 1995, 1999 Eric Youngdale
 *
 *  adapted from:
 *      sd.c Copyright (C) 1992 Drew Eckhardt
 *      Linux scsi disk driver by
 *              Drew Eckhardt <drew@colorado.edu>
 *
 *      Modified by Eric Youngdale ericy@andante.org to
 *      add scatter-gather, multiple outstanding request, and other
 *      enhancements.
 *
 *          Modified by Eric Youngdale eric@andante.org to support loadable
 *          low-level scsi drivers.
 *
 *       Modified by Thomas Quinot thomas@melchior.cuivre.fdn.fr to
 *       provide auto-eject.
 *
 *          Modified by Gerd Knorr <kraxel@cs.tu-berlin.de> to support the
 *          generic cdrom interface
 *
 *       Modified by Jens Axboe <axboe@suse.de> - Uniform sr_packet()
 *       interface, capabilities probe additions, ioctl cleanups, etc.
 *
 *       Modified by Richard Gooch <rgooch@atnf.csiro.au> to support devfs
 *
 *       Modified by Jens Axboe <axboe@suse.de> - support DVD-RAM
 *	 transparently and loose the GHOST hack
 *
 *	 Modified by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *	 check resource allocation in sr_init and some cleanups
 *
 */

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/bio.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/cdrom.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define MAJOR_NR SCSI_CDROM_MAJOR
#define LOCAL_END_REQUEST
#define DEVICE_NR(device) (minor(device))
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "sr.h"
#include <scsi/scsi_ioctl.h>	/* For the door lock/unlock commands */

MODULE_PARM(xa_test, "i");	/* see sr_ioctl.c */

#define MAX_RETRIES	3
#define SR_TIMEOUT	(30 * HZ)

static int sr_init(void);
static void sr_finish(void);
static int sr_attach(Scsi_Device *);
static int sr_detect(Scsi_Device *);
static void sr_detach(Scsi_Device *);

static int sr_init_command(Scsi_Cmnd *);

static struct Scsi_Device_Template sr_template =
{
	module:THIS_MODULE,
	name:"cdrom",
	tag:"sr",
	scsi_type:TYPE_ROM,
	major:SCSI_CDROM_MAJOR,
	blk:1,
	detect:sr_detect,
	init:sr_init,
	finish:sr_finish,
	attach:sr_attach,
	detach:sr_detach,
	init_command:sr_init_command
};

Scsi_CD *scsi_CDs;
static int *sr_sizes;

static int sr_open(struct cdrom_device_info *, int);
void get_sectorsize(int);
void get_capabilities(int);

static int sr_media_change(struct cdrom_device_info *, int);
static int sr_packet(struct cdrom_device_info *, struct cdrom_generic_command *);

static void sr_release(struct cdrom_device_info *cdi)
{
	Scsi_CD *SCp = cdi->handle;

	if (SCp->device->sector_size > 2048)
		sr_set_blocklength(minor(cdi->dev), 2048);
	SCp->device->access_count--;
	if (SCp->device->host->hostt->module)
		__MOD_DEC_USE_COUNT(SCp->device->host->hostt->module);
	if (sr_template.module)
		__MOD_DEC_USE_COUNT(sr_template.module);
}

static struct cdrom_device_ops sr_dops =
{
	open:			sr_open,
	release:		sr_release,
	drive_status:		sr_drive_status,
	media_changed:		sr_media_change,
	tray_move:		sr_tray_move,
	lock_door:		sr_lock_door,
	select_speed:		sr_select_speed,
	get_last_session:	sr_get_last_session,
	get_mcn:		sr_get_mcn,
	reset:			sr_reset,
	audio_ioctl:		sr_audio_ioctl,
	dev_ioctl:		sr_dev_ioctl,
	capability:		CDC_CLOSE_TRAY | CDC_OPEN_TRAY | CDC_LOCK |
				CDC_SELECT_SPEED | CDC_SELECT_DISC |
				CDC_MULTI_SESSION | CDC_MCN |
				CDC_MEDIA_CHANGED | CDC_PLAY_AUDIO |
				CDC_RESET | CDC_IOCTLS | CDC_DRIVE_STATUS |
				CDC_CD_R | CDC_CD_RW | CDC_DVD | CDC_DVD_R |
				CDC_DVD_RAM | CDC_GENERIC_PACKET,
	generic_packet:		sr_packet,
};

/*
 * This function checks to see if the media has been changed in the
 * CDROM drive.  It is possible that we have already sensed a change,
 * or the drive may have sensed one and not yet reported it.  We must
 * be ready for either case. This function always reports the current
 * value of the changed bit.  If flag is 0, then the changed bit is reset.
 * This function could be done as an ioctl, but we would need to have
 * an inode for that to work, and we do not always have one.
 */

int sr_media_change(struct cdrom_device_info *cdi, int slot)
{
	Scsi_CD *SCp = cdi->handle;
	int retval;

	if (CDSL_CURRENT != slot) {
		/* no changer support */
		return -EINVAL;
	}

	retval = scsi_ioctl(SCp->device, SCSI_IOCTL_TEST_UNIT_READY, 0);
	if (retval) {
		/* Unable to test, unit probably not ready.  This usually
		 * means there is no disc in the drive.  Mark as changed,
		 * and we will figure it out later once the drive is
		 * available again.  */
		SCp->device->changed = 1;
		return 1;	/* This will force a flush, if called from
				 * check_disk_change */
	};

	retval = SCp->device->changed;
	SCp->device->changed = 0;
	/* If the disk changed, the capacity will now be different,
	 * so we force a re-read of this information */
	if (retval) {
		/* check multisession offset etc */
		sr_cd_check(cdi);

		/* 
		 * If the disk changed, the capacity will now be different,
		 * so we force a re-read of this information 
		 * Force 2048 for the sector size so that filesystems won't
		 * be trying to use something that is too small if the disc
		 * has changed.
		 */
		SCp->needs_sector_size = 1;
		SCp->device->sector_size = 2048;
	}
	return retval;
}

/*
 * rw_intr is the interrupt routine for the device driver.  It will be notified on the
 * end of a SCSI read / write, and will take on of several actions based on success or failure.
 */

static void rw_intr(Scsi_Cmnd * SCpnt)
{
	int result = SCpnt->result;
	int this_count = SCpnt->bufflen >> 9;
	int good_sectors = (result == 0 ? this_count : 0);
	int block_sectors = 0;
	int device_nr = DEVICE_NR(SCpnt->request.rq_dev);
	Scsi_CD *SCp = &scsi_CDs[device_nr];

#ifdef DEBUG
	printk("sr.c done: %x %p\n", result, SCpnt->request.bh->b_data);
#endif
	/*
	   Handle MEDIUM ERRORs or VOLUME OVERFLOWs that indicate partial success.
	   Since this is a relatively rare error condition, no care is taken to
	   avoid unnecessary additional work such as memcpy's that could be avoided.
	 */


	if (driver_byte(result) != 0 &&		/* An error occurred */
	    SCpnt->sense_buffer[0] == 0xF0 &&	/* Sense data is valid */
	    (SCpnt->sense_buffer[2] == MEDIUM_ERROR ||
	     SCpnt->sense_buffer[2] == VOLUME_OVERFLOW ||
	     SCpnt->sense_buffer[2] == ILLEGAL_REQUEST)) {
		long error_sector = (SCpnt->sense_buffer[3] << 24) |
		(SCpnt->sense_buffer[4] << 16) |
		(SCpnt->sense_buffer[5] << 8) |
		SCpnt->sense_buffer[6];
		if (SCpnt->request.bio != NULL)
			block_sectors = bio_sectors(SCpnt->request.bio);
		if (block_sectors < 4)
			block_sectors = 4;
		if (SCp->device->sector_size == 2048)
			error_sector <<= 2;
		error_sector &= ~(block_sectors - 1);
		good_sectors = error_sector - SCpnt->request.sector;
		if (good_sectors < 0 || good_sectors >= this_count)
			good_sectors = 0;
		/*
		 * The SCSI specification allows for the value returned by READ
		 * CAPACITY to be up to 75 2K sectors past the last readable
		 * block.  Therefore, if we hit a medium error within the last
		 * 75 2K sectors, we decrease the saved size value.
		 */
		if ((error_sector >> 1) < sr_sizes[device_nr] &&
		    SCp->capacity - error_sector < 4 * 75)
			sr_sizes[device_nr] = error_sector >> 1;
	}

	/*
	 * This calls the generic completion function, now that we know
	 * how many actual sectors finished, and how many sectors we need
	 * to say have failed.
	 */
	scsi_io_completion(SCpnt, good_sectors, block_sectors);
}


static request_queue_t *sr_find_queue(kdev_t dev)
{
	Scsi_CD *SCp;

	if (minor(dev) >= sr_template.dev_max)
		return NULL;
	SCp = &scsi_CDs[minor(dev)];
	if (!SCp->device)
		return NULL;
	return &SCp->device->request_queue;
}

static int sr_init_command(Scsi_Cmnd * SCpnt)
{
	int dev, devm, block=0, this_count, s_size;
	Scsi_CD *SCp;

	devm = minor(SCpnt->request.rq_dev);
	dev = DEVICE_NR(SCpnt->request.rq_dev);
	SCp = &scsi_CDs[dev];

	SCSI_LOG_HLQUEUE(1, printk("Doing sr request, dev = %d, block = %d\n", devm, block));

	if (dev >= sr_template.nr_dev || !SCp->device || !SCp->device->online) {
		SCSI_LOG_HLQUEUE(2, printk("Finishing %ld sectors\n", SCpnt->request.nr_sectors));
		SCSI_LOG_HLQUEUE(2, printk("Retry with 0x%p\n", SCpnt));
		return 0;
	}

	if (SCp->device->changed) {
		/*
		 * quietly refuse to do anything to a changed disc until the
		 * changed bit has been reset
		 */
		return 0;
	}

	if (!(SCpnt->request.flags & REQ_CMD)) {
		blk_dump_rq_flags(&SCpnt->request, "sr unsup command");
		return 0;
	}

	/*
	 * we do lazy blocksize switching (when reading XA sectors,
	 * see CDROMREADMODE2 ioctl) 
	 */
	s_size = SCp->device->sector_size;
	if (s_size > 2048) {
		if (!in_interrupt())
			sr_set_blocklength(DEVICE_NR(CURRENT->rq_dev), 2048);
		else
			printk("sr: can't switch blocksize: in interrupt\n");
	}

	if (s_size != 512 && s_size != 1024 && s_size != 2048) {
		printk("sr: bad sector size %d\n", s_size);
		return 0;
	}

	if (rq_data_dir(&SCpnt->request) == WRITE) {
		if (!SCp->device->writeable)
			return 0;
		SCpnt->cmnd[0] = WRITE_10;
		SCpnt->sc_data_direction = SCSI_DATA_WRITE;
	} else if (rq_data_dir(&SCpnt->request) == READ) {
		SCpnt->cmnd[0] = READ_10;
		SCpnt->sc_data_direction = SCSI_DATA_READ;
	} else {
		blk_dump_rq_flags(&SCpnt->request, "Unknown sr command");
		return 0;
	}

	/*
	 * request doesn't start on hw block boundary, add scatter pads
	 */
	if ((SCpnt->request.sector % (s_size >> 9)) || (SCpnt->request_bufflen % s_size)) {
		printk("sr: unaligned transfer\n");
		return 0;
	}

	this_count = (SCpnt->request_bufflen >> 9) / (s_size >> 9);


	SCSI_LOG_HLQUEUE(2, printk("sr%d : %s %d/%ld 512 byte blocks.\n",
                                   devm,
		   (rq_data_dir(&SCpnt->request) == WRITE) ? "writing" : "reading",
				 this_count, SCpnt->request.nr_sectors));

	SCpnt->cmnd[1] = (SCpnt->device->scsi_level <= SCSI_2) ?
			 ((SCpnt->lun << 5) & 0xe0) : 0;

	block = SCpnt->request.sector / (s_size >> 9);

	if (this_count > 0xffff)
		this_count = 0xffff;

	SCpnt->cmnd[2] = (unsigned char) (block >> 24) & 0xff;
	SCpnt->cmnd[3] = (unsigned char) (block >> 16) & 0xff;
	SCpnt->cmnd[4] = (unsigned char) (block >> 8) & 0xff;
	SCpnt->cmnd[5] = (unsigned char) block & 0xff;
	SCpnt->cmnd[6] = SCpnt->cmnd[9] = 0;
	SCpnt->cmnd[7] = (unsigned char) (this_count >> 8) & 0xff;
	SCpnt->cmnd[8] = (unsigned char) this_count & 0xff;

	/*
	 * We shouldn't disconnect in the middle of a sector, so with a dumb
	 * host adapter, it's safe to assume that we can at least transfer
	 * this many bytes between each connect / disconnect.
	 */
	SCpnt->transfersize = SCp->device->sector_size;
	SCpnt->underflow = this_count << 9;

	SCpnt->allowed = MAX_RETRIES;
	SCpnt->timeout_per_command = SR_TIMEOUT;

	/*
	 * This is the completion routine we use.  This is matched in terms
	 * of capability to this function.
	 */
	SCpnt->done = rw_intr;

	{
		struct scatterlist *sg = SCpnt->request_buffer;
		int i, size = 0;
		for (i = 0; i < SCpnt->use_sg; i++)
			size += sg[i].length;

		if (size != SCpnt->request_bufflen && SCpnt->use_sg) {
			printk("sr: mismatch count %d, bytes %d\n", size, SCpnt->request_bufflen);
			SCpnt->request_bufflen = size;
		}
	}

	/*
	 * This indicates that the command is ready from our end to be
	 * queued.
	 */
	return 1;
}

struct block_device_operations sr_bdops =
{
	owner:			THIS_MODULE,
	open:			cdrom_open,
	release:		cdrom_release,
	ioctl:			cdrom_ioctl,
	check_media_change:	cdrom_media_changed,
};

static int sr_open(struct cdrom_device_info *cdi, int purpose)
{
	Scsi_CD *SCp = cdi->handle;

	check_disk_change(cdi->dev);

	if (minor(cdi->dev) >= sr_template.dev_max || !SCp->device) {
		return -ENXIO;	/* No such device */
	}
	/*
	 * If the device is in error recovery, wait until it is done.
	 * If the device is offline, then disallow any access to it.
	 */
	if (!scsi_block_when_processing_errors(SCp->device)) {
		return -ENXIO;
	}
	SCp->device->access_count++;
	if (SCp->device->host->hostt->module)
		__MOD_INC_USE_COUNT(SCp->device->host->hostt->module);
	if (sr_template.module)
		__MOD_INC_USE_COUNT(sr_template.module);

	/* If this device did not have media in the drive at boot time, then
	 * we would have been unable to get the sector size.  Check to see if
	 * this is the case, and try again.
	 */

	if (SCp->needs_sector_size)
		get_sectorsize(minor(cdi->dev));

	return 0;
}

static int sr_detect(Scsi_Device * SDp)
{

	if (SDp->type != TYPE_ROM && SDp->type != TYPE_WORM)
		return 0;
	sr_template.dev_noticed++;
	return 1;
}

static int sr_attach(Scsi_Device * SDp)
{
	Scsi_CD *cpnt;
	int i;

	if (SDp->type != TYPE_ROM && SDp->type != TYPE_WORM)
		return 1;

	if (sr_template.nr_dev >= sr_template.dev_max) {
		SDp->attached--;
		return 1;
	}
	for (cpnt = scsi_CDs, i = 0; i < sr_template.dev_max; i++, cpnt++)
		if (!cpnt->device)
			break;

	if (i >= sr_template.dev_max)
		panic("scsi_devices corrupt (sr)");


	scsi_CDs[i].device = SDp;

	sr_template.nr_dev++;
	if (sr_template.nr_dev > sr_template.dev_max)
		panic("scsi_devices corrupt (sr)");

	printk("Attached scsi CD-ROM sr%d at scsi%d, channel %d, id %d, lun %d\n",
	       i, SDp->host->host_no, SDp->channel, SDp->id, SDp->lun);
	return 0;
}


void get_sectorsize(int i)
{
	unsigned char cmd[10];
	unsigned char *buffer;
	int the_result, retries = 3;
	int sector_size;
	Scsi_Request *SRpnt = NULL;
	Scsi_CD *SCp;
	request_queue_t *queue;

	SCp = &scsi_CDs[i];

	buffer = kmalloc(512, GFP_DMA);
	if (!buffer)
		goto Enomem;
	SRpnt = scsi_allocate_request(SCp->device);
	if (!SRpnt)
		goto Enomem;

	do {
		cmd[0] = READ_CAPACITY;
		cmd[1] = (SCp->device->scsi_level <= SCSI_2) ?
			 ((SCp->device->lun << 5) & 0xe0) : 0;
		memset((void *) &cmd[2], 0, 8);
		SRpnt->sr_request.rq_status = RQ_SCSI_BUSY;	/* Mark as really busy */
		SRpnt->sr_cmd_len = 0;

		memset(buffer, 0, 8);

		/* Do the command and wait.. */

		SRpnt->sr_data_direction = SCSI_DATA_READ;
		scsi_wait_req(SRpnt, (void *) cmd, (void *) buffer,
			      8, SR_TIMEOUT, MAX_RETRIES);

		the_result = SRpnt->sr_result;
		retries--;

	} while (the_result && retries);


	scsi_release_request(SRpnt);
	SRpnt = NULL;

	if (the_result) {
		SCp->capacity = 0x1fffff;
		sector_size = 2048;	/* A guess, just in case */
		SCp->needs_sector_size = 1;
	} else {
#if 0
		if (cdrom_get_last_written(&SCp->cdi,
					   &SCp->capacity))
#endif
			SCp->capacity = 1 + ((buffer[0] << 24) |
						    (buffer[1] << 16) |
						    (buffer[2] << 8) |
						    buffer[3]);
		sector_size = (buffer[4] << 24) |
		    (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
		switch (sector_size) {
			/*
			 * HP 4020i CD-Recorder reports 2340 byte sectors
			 * Philips CD-Writers report 2352 byte sectors
			 *
			 * Use 2k sectors for them..
			 */
		case 0:
		case 2340:
		case 2352:
			sector_size = 2048;
			/* fall through */
		case 2048:
			SCp->capacity *= 4;
			/* fall through */
		case 512:
			break;
		default:
			printk("sr%d: unsupported sector size %d.\n",
			       i, sector_size);
			SCp->capacity = 0;
			SCp->needs_sector_size = 1;
		}

		SCp->device->sector_size = sector_size;

		/*
		 * Add this so that we have the ability to correctly gauge
		 * what the device is capable of.
		 */
		SCp->needs_sector_size = 0;
		sr_sizes[i] = SCp->capacity >> (BLOCK_SIZE_BITS - 9);
	}

	queue = &SCp->device->request_queue;
	blk_queue_hardsect_size(queue, sector_size);
out:
	kfree(buffer);
	return;

Enomem:
	SCp->capacity = 0x1fffff;
	sector_size = 2048;	/* A guess, just in case */
	SCp->needs_sector_size = 1;
	if (SRpnt)
		scsi_release_request(SRpnt);
	goto out;
}

void get_capabilities(int i)
{
	Scsi_CD *SCp;
	unsigned char cmd[6];
	unsigned char *buffer;
	int rc, n;

	static char *loadmech[] =
	{
		"caddy",
		"tray",
		"pop-up",
		"",
		"changer",
		"cartridge changer",
		"",
		""
	};

	SCp = &scsi_CDs[i];
	buffer = kmalloc(512, GFP_DMA);
	if (!buffer)
	{
		printk(KERN_ERR "sr: out of memory.\n");
		return;
	}
	cmd[0] = MODE_SENSE;
	cmd[1] = (SCp->device->scsi_level <= SCSI_2) ?
		 ((SCp->device->lun << 5) & 0xe0) : 0;
	cmd[2] = 0x2a;
	cmd[4] = 128;
	cmd[3] = cmd[5] = 0;
	rc = sr_do_ioctl(i, cmd, buffer, 128, 1, SCSI_DATA_READ, NULL);

	if (rc) {
		/* failed, drive doesn't have capabilities mode page */
		SCp->cdi.speed = 1;
		SCp->cdi.mask |= (CDC_CD_R | CDC_CD_RW | CDC_DVD_R |
					 CDC_DVD | CDC_DVD_RAM |
					 CDC_SELECT_DISC | CDC_SELECT_SPEED);
		kfree(buffer);
		printk("sr%i: scsi-1 drive\n", i);
		return;
	}
	n = buffer[3] + 4;
	SCp->cdi.speed = ((buffer[n + 8] << 8) + buffer[n + 9]) / 176;
	SCp->readcd_known = 1;
	SCp->readcd_cdda = buffer[n + 5] & 0x01;
	/* print some capability bits */
	printk("sr%i: scsi3-mmc drive: %dx/%dx %s%s%s%s%s%s\n", i,
	       ((buffer[n + 14] << 8) + buffer[n + 15]) / 176,
	       SCp->cdi.speed,
	       buffer[n + 3] & 0x01 ? "writer " : "",	/* CD Writer */
	       buffer[n + 3] & 0x20 ? "dvd-ram " : "",
	       buffer[n + 2] & 0x02 ? "cd/rw " : "",	/* can read rewriteable */
	       buffer[n + 4] & 0x20 ? "xa/form2 " : "",		/* can read xa/from2 */
	       buffer[n + 5] & 0x01 ? "cdda " : "",	/* can read audio data */
	       loadmech[buffer[n + 6] >> 5]);
	if ((buffer[n + 6] >> 5) == 0)
		/* caddy drives can't close tray... */
		SCp->cdi.mask |= CDC_CLOSE_TRAY;
	if ((buffer[n + 2] & 0x8) == 0)
		/* not a DVD drive */
		SCp->cdi.mask |= CDC_DVD;
	if ((buffer[n + 3] & 0x20) == 0) {
		/* can't write DVD-RAM media */
		SCp->cdi.mask |= CDC_DVD_RAM;
	} else {
		SCp->device->writeable = 1;
	}
	if ((buffer[n + 3] & 0x10) == 0)
		/* can't write DVD-R media */
		SCp->cdi.mask |= CDC_DVD_R;
	if ((buffer[n + 3] & 0x2) == 0)
		/* can't write CD-RW media */
		SCp->cdi.mask |= CDC_CD_RW;
	if ((buffer[n + 3] & 0x1) == 0)
		/* can't write CD-R media */
		SCp->cdi.mask |= CDC_CD_R;
	if ((buffer[n + 6] & 0x8) == 0)
		/* can't eject */
		SCp->cdi.mask |= CDC_OPEN_TRAY;

	if ((buffer[n + 6] >> 5) == mechtype_individual_changer ||
	    (buffer[n + 6] >> 5) == mechtype_cartridge_changer)
		SCp->cdi.capacity =
		    cdrom_number_of_slots(&SCp->cdi);
	if (SCp->cdi.capacity <= 1)
		/* not a changer */
		SCp->cdi.mask |= CDC_SELECT_DISC;
	/*else    I don't think it can close its tray
		SCp->cdi.mask |= CDC_CLOSE_TRAY; */

	kfree(buffer);
}

/*
 * sr_packet() is the entry point for the generic commands generated
 * by the Uniform CD-ROM layer. 
 */
static int sr_packet(struct cdrom_device_info *cdi, struct cdrom_generic_command *cgc)
{
	Scsi_CD *SCp = cdi->handle;
	Scsi_Device *device = SCp->device;
	
	/* set the LUN */
	if (device->scsi_level <= SCSI_2)
		cgc->cmd[1] |= device->lun << 5;

	cgc->stat = sr_do_ioctl(minor(cdi->dev), cgc->cmd, cgc->buffer, cgc->buflen, cgc->quiet, cgc->data_direction, cgc->sense);

	return cgc->stat;
}

static int sr_registered;

static int sr_init()
{
	if (sr_template.dev_noticed == 0)
		return 0;

	if (!sr_registered) {
		if (devfs_register_blkdev(MAJOR_NR, "sr", &sr_bdops)) {
			printk("Unable to get major %d for SCSI-CD\n", MAJOR_NR);
			return 1;
		}
		sr_registered++;
	}
	if (scsi_CDs)
		return 0;

	sr_template.dev_max = sr_template.dev_noticed + SR_EXTRA_DEVS;
	scsi_CDs = kmalloc(sr_template.dev_max * sizeof(Scsi_CD), GFP_ATOMIC);
	if (!scsi_CDs)
		goto cleanup_devfs;
	memset(scsi_CDs, 0, sr_template.dev_max * sizeof(Scsi_CD));

	sr_sizes = kmalloc(sr_template.dev_max * sizeof(int), GFP_ATOMIC);
	if (!sr_sizes)
		goto cleanup_cds;
	memset(sr_sizes, 0, sr_template.dev_max * sizeof(int));
	return 0;

cleanup_cds:
	kfree(scsi_CDs);
cleanup_devfs:
	devfs_unregister_blkdev(MAJOR_NR, "sr");
	sr_registered--;
	return 1;
}

void sr_finish()
{
	int i;
	char name[6];

	blk_dev[MAJOR_NR].queue = sr_find_queue;
	blk_size[MAJOR_NR] = sr_sizes;

	for (i = 0; i < sr_template.nr_dev; ++i) {
		Scsi_CD *SCp = &scsi_CDs[i];
		/* If we have already seen this, then skip it.  Comes up
		 * with loadable modules. */
		if (SCp->capacity)
			continue;
		SCp->capacity = 0x1fffff;
		SCp->device->sector_size = 2048;/* A guess, just in case */
		SCp->needs_sector_size = 1;
		SCp->device->changed = 1;	/* force recheck CD type */
#if 0
		/* seems better to leave this for later */
		get_sectorsize(i);
		printk("Scd sectorsize = %d bytes.\n", SCp->sector_size);
#endif
		SCp->use = 1;

		SCp->device->ten = 1;
		SCp->device->remap = 1;
		SCp->readcd_known = 0;
		SCp->readcd_cdda = 0;
		sr_sizes[i] = SCp->capacity >> (BLOCK_SIZE_BITS - 9);

		SCp->cdi.ops = &sr_dops;
		SCp->cdi.handle = SCp;
		SCp->cdi.dev = mk_kdev(MAJOR_NR, i);
		SCp->cdi.mask = 0;
		SCp->cdi.capacity = 1;
		/*
		 *	FIXME: someone needs to handle a get_capabilities
		 *	failure properly ??
		 */
		get_capabilities(i);
		sr_vendor_init(SCp);

		sprintf(name, "sr%d", i);
		strcpy(SCp->cdi.name, name);
                SCp->cdi.de = devfs_register(SCp->device->de, "cd",
                                    DEVFS_FL_DEFAULT, MAJOR_NR, i,
                                    S_IFBLK | S_IRUGO | S_IWUGO,
                                    &sr_bdops, NULL);
		register_cdrom(&SCp->cdi);
	}
}

static void sr_detach(Scsi_Device * SDp)
{
	Scsi_CD *cpnt;
	int i;

	for (cpnt = scsi_CDs, i = 0; i < sr_template.dev_max; i++, cpnt++) {
		if (cpnt->device == SDp) {
			/*
			 * Since the cdrom is read-only, no need to sync
			 * the device.
			 * We should be kind to our buffer cache, however.
			 */
			invalidate_device(mk_kdev(MAJOR_NR, i), 0);

			/*
			 * Reset things back to a sane state so that one can
			 * re-load a new driver (perhaps the same one).
			 */
			unregister_cdrom(&(cpnt->cdi));
			cpnt->device = NULL;
			cpnt->capacity = 0;
			SDp->attached--;
			sr_template.nr_dev--;
			sr_template.dev_noticed--;
			sr_sizes[i] = 0;
			return;
		}
	}
}

static int __init init_sr(void)
{
	return scsi_register_device(&sr_template);
}

static void __exit exit_sr(void)
{
	scsi_unregister_device(&sr_template);
	devfs_unregister_blkdev(MAJOR_NR, "sr");
	sr_registered--;
	if (scsi_CDs != NULL) {
		kfree(scsi_CDs);

		kfree(sr_sizes);
		sr_sizes = NULL;
	}
	blk_clear(MAJOR_NR);

	sr_template.dev_max = 0;
}

module_init(init_sr);
module_exit(exit_sr);
MODULE_LICENSE("GPL");
