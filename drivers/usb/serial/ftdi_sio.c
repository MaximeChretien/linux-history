/*
 * USB FTDI SIO driver
 *
 * 	Copyright (C) 1999 - 2001
 * 	    Greg Kroah-Hartman (greg@kroah.com)
 *          Bill Ryder (bryder@sgi.com)
 *	Copyright (C) 2002
 *	    Kuba Ober (kuba@mareimbrium.org)
 *
 * 	This program is free software; you can redistribute it and/or modify
 * 	it under the terms of the GNU General Public License as published by
 * 	the Free Software Foundation; either version 2 of the License, or
 * 	(at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this driver
 *
 * See http://ftdi-usb-sio.sourceforge.net for upto date testing info
 *	and extra documentation
 *
 * (23/Feb/2003) Bill Ryder
 *      Added matrix orb device vid/pids from Wayne Wylupski
 *
 * (19/Feb/2003) Ian Abbott
 *      For TIOCSSERIAL, set alt_speed to 0 when ASYNC_SPD_MASK value has
 *      changed to something other than ASYNC_SPD_HI, ASYNC_SPD_VHI,
 *      ASYNC_SPD_SHI or ASYNC_SPD_WARP.  Also, unless ASYNC_SPD_CUST is in
 *      force, don't bother changing baud rate when custom_divisor has changed.
 *
 * (18/Feb/2003) Ian Abbott
 *      Fixed TIOCMGET handling to include state of DTR and RTS, the state
 *      of which are now saved by set_dtr() and set_rts().
 *      Fixed improper storage class for buf in set_dtr() and set_rts().
 *      Added FT232BM chip type and support for its extra baud rates (compared
 *      to FT8U232AM).
 *      Took account of special case divisor values for highest baud rates of
 *      FT8U232AM and FT232BM.
 *      For TIOCSSERIAL, forced alt_speed to 0 when ASYNC_SPD_CUST kludge used,
 *      as previous alt_speed setting is now stale.
 *      Moved startup code common between the startup routines for the
 *      different chip types into a common subroutine.
 *
 * (17/Feb/2003) Bill Ryder
 *      Added write urb buffer pool on a per device basis
 *      Added more checking for open file on callbacks (fixed OOPS)
 *      Added CrystalFontz 632 and 634 PIDs 
 *         (thanx to CrystalFontz for the sample devices - they flushed out
 *           some driver bugs)
 *      Minor debugging message changes
 *      Added throttle, unthrottle and chars_in_buffer functions
 *      Fixed FTDI_SIO (the original device) bug
 *      Fixed some shutdown handling
 *      
 * 
 * 
 * 
 * (07/Jun/2002) Kuba Ober
 *	Changed FTDI_SIO_BASE_BAUD_TO_DIVISOR macro into ftdi_baud_to_divisor
 *	function. It was getting too complex.
 *	Fix the divisor calculation logic which was setting divisor of 0.125
 *	instead of 0.5 for fractional parts of divisor equal to 5/8, 6/8, 7/8.
 *	Also make it bump up the divisor to next integer in case of 7/8 - it's
 *	a better approximation.
 *
 * (25/Jul/2002) Bill Ryder inserted Dmitri's TIOCMIWAIT patch
 *      Not tested by me but it doesn't break anything I use.
 * 
 * (04/Jan/2002) Kuba Ober
 *	Implemented 38400 baudrate kludge, where it can be substituted with other
 *	  values. That's the only way to set custom baudrates.
 *	Implemented TIOCSSERIAL, TIOCGSERIAL ioctl's so that setserial is happy.
 *	FIXME: both baudrate things should eventually go to usbserial.c as other
 *	  devices may need that functionality too. Actually, it can probably be
 *	  merged in serial.c somehow - too many drivers repeat this code over
 *	  and over.
 *	Fixed baudrate forgetfulness - open() used to reset baudrate to 9600 every time.
 *	Divisors for baudrates are calculated by a macro.
 *	Small code cleanups. Ugly whitespace changes for Plato's sake only ;-].
 *
 * (04/Nov/2001) Bill Ryder
 *	Fixed bug in read_bulk_callback where incorrect urb buffer was used.
 *	Cleaned up write offset calculation
 *	Added write_room since default values can be incorrect for sio
 *	Changed write_bulk_callback to use same queue_task as other drivers
 *        (the previous version caused panics)
 *	Removed port iteration code since the device only has one I/O port and it
 *	  was wrong anyway.
 * 
 * (31/May/2001) gkh
 *	Switched from using spinlock to a semaphore, which fixes lots of problems.
 *
 * (23/May/2001)   Bill Ryder
 *	Added runtime debug patch (thanx Tyson D Sawyer).
 *	Cleaned up comments for 8U232
 *	Added parity, framing and overrun error handling
 *	Added receive break handling.
 * 
 * (04/08/2001) gb
 *	Identify version on module load.
 *       
 * (18/March/2001) Bill Ryder
 *	(Not released)
 *	Added send break handling. (requires kernel patch too)
 *	Fixed 8U232AM hardware RTS/CTS etc status reporting.
 *	Added flipbuf fix copied from generic device
 * 
 * (12/3/2000) Bill Ryder
 *	Added support for 8U232AM device.
 *	Moved PID and VIDs into header file only.
 *	Turned on low-latency for the tty (device will do high baudrates)
 *	Added shutdown routine to close files when device removed.
 *	More debug and error message cleanups.
 *
 * (11/13/2000) Bill Ryder
 *	Added spinlock protected open code and close code.
 *	Multiple opens work (sort of - see webpage mentioned above).
 *	Cleaned up comments. Removed multiple PID/VID definitions.
 *	Factorised cts/dtr code
 *	Made use of __FUNCTION__ in dbg's
 *      
 * (11/01/2000) Adam J. Richter
 *	usb_device_id table support
 * 
 * (10/05/2000) gkh
 *	Fixed bug with urb->dev not being set properly, now that the usb
 *	core needs it.
 * 
 * (09/11/2000) gkh
 *	Removed DEBUG #ifdefs with call to usb_serial_debug_data
 *
 * (07/19/2000) gkh
 *	Added module_init and module_exit functions to handle the fact that this
 *	driver is a loadable module now.
 *
 * (04/04/2000) Bill Ryder 
 *	Fixed bugs in TCGET/TCSET ioctls (by removing them - they are
 *        handled elsewhere in the tty io driver chain).
 *
 * (03/30/2000) Bill Ryder 
 *	Implemented lots of ioctls
 *	Fixed a race condition in write
 *	Changed some dbg's to errs
 *
 * (03/26/2000) gkh
 *	Split driver up into device specific pieces.
 *
 */

/* Bill Ryder - bryder@sgi.com - wrote the FTDI_SIO implementation */
/* Thanx to FTDI for so kindly providing details of the protocol required */
/*   to talk to the device */
/* Thanx to gkh and the rest of the usb dev group for all code I have assimilated :-) */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/serial.h>
#ifdef CONFIG_USB_SERIAL_DEBUG
	static int debug = 1;
#else
	static int debug;
#endif

#include "usb-serial.h"
#include "ftdi_sio.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.3.2"
#define DRIVER_AUTHOR "Greg Kroah-Hartman <greg@kroah.com>, Bill Ryder <bryder@sgi.com>, Kuba Ober <kuba@mareimbrium.org>"
#define DRIVER_DESC "USB FTDI Serial Converters Driver"

static struct usb_device_id id_table_sio [] = {
	{ USB_DEVICE(FTDI_VID, FTDI_SIO_PID) },
	{ }						/* Terminating entry */
};

/*
 * The 8U232AM has the same API as the sio except for:
 * - it can support MUCH higher baudrates; up to:
 *   o 921600 for RS232 and 2000000 for RS422/485 at 48MHz
 *   o 230400 at 12MHz
 *   so .. 8U232AM's baudrate setting codes are different
 * - it has a two byte status code.
 * - it returns characters every 16ms (the FTDI does it every 40ms)
 *
 * the bcdDevice value is used to differentiate FT232BM and FT245BM from
 * the earlier FT8U232AM and FT8U232BM.  For now, include all known VID/PID
 * combinations in both tables.
 * FIXME: perhaps bcdDevice can also identify 12MHz devices, but I don't know
 * if those ever went into mass production. [Ian Abbott]
 */


static struct usb_device_id id_table_8U232AM [] = {
	{ USB_DEVICE_VER(FTDI_VID, FTDI_8U232AM_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_NF_RIC_VID, FTDI_NF_RIC_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_VID, FTDI_XF_634_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_VID, FTDI_XF_632_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_0_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_1_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_2_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_3_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_4_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_5_PID, 0, 0x3ff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_6_PID, 0, 0x3ff) },
	{ }						/* Terminating entry */
};


static struct usb_device_id id_table_FT232BM [] = {
	{ USB_DEVICE_VER(FTDI_VID, FTDI_8U232AM_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_NF_RIC_VID, FTDI_NF_RIC_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_VID, FTDI_XF_634_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_VID, FTDI_XF_632_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_0_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_1_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_2_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_3_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_4_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_5_PID, 0x400, 0xffff) },
	{ USB_DEVICE_VER(FTDI_MTXORB_VID, FTDI_MTXORB_6_PID, 0x400, 0xffff) },

	{ }						/* Terminating entry */
};


static __devinitdata struct usb_device_id id_table_combined [] = {
	{ USB_DEVICE(FTDI_VID, FTDI_SIO_PID) },
	{ USB_DEVICE(FTDI_VID, FTDI_8U232AM_PID) },
	{ USB_DEVICE(FTDI_VID, FTDI_XF_634_PID) },
	{ USB_DEVICE(FTDI_VID, FTDI_XF_632_PID) },
	{ USB_DEVICE(FTDI_NF_RIC_VID, FTDI_NF_RIC_PID) },
	{ USB_DEVICE(FTDI_MTXORB_VID, FTDI_MTXORB_0_PID) },
	{ USB_DEVICE(FTDI_MTXORB_VID, FTDI_MTXORB_1_PID) },
	{ USB_DEVICE(FTDI_MTXORB_VID, FTDI_MTXORB_2_PID) },
	{ USB_DEVICE(FTDI_MTXORB_VID, FTDI_MTXORB_3_PID) },
	{ USB_DEVICE(FTDI_MTXORB_VID, FTDI_MTXORB_4_PID) },
	{ USB_DEVICE(FTDI_MTXORB_VID, FTDI_MTXORB_5_PID) },
	{ USB_DEVICE(FTDI_MTXORB_VID, FTDI_MTXORB_6_PID) },	{ }						/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table_combined);


/* constants which set the number of write urb buffers */
#define NUM_URBS			32
/* Don't be tempted to increase this buffer to > 64 ! I tried it and it doesn't work */
#define URB_TRANSFER_BUFFER_SIZE	64 /* the device's max packet size */


struct ftdi_private {
	ftdi_chip_type_t chip_type;
				/* type of the device, either SIO or FT8U232AM */
	int baud_base;		/* baud base clock for divisor setting */
	int custom_divisor;	/* custom_divisor kludge, this is for baud_base (different from what goes to the chip!) */
	__u16 last_set_data_urb_value ;
				/* the last data state set - needed for doing a break */
        int write_offset;       /* This is the offset in the usb data block to write the serial data - 
				 * it is different between devices
				 */
	int flags;		/* some ASYNC_xxxx flags are supported */
	unsigned long last_dtr_rts;	/* saved modem control outputs */
        wait_queue_head_t delta_msr_wait; /* Used for TIOCMIWAIT */
 	char prev_status, diff_status;        /* Used for TIOCMIWAIT */

	struct urb	*write_urb_pool[NUM_URBS];
	spinlock_t	write_urb_pool_lock;

};

/* Used for TIOCMIWAIT */
#define FTDI_STATUS_B0_MASK	(FTDI_RS0_CTS | FTDI_RS0_DSR | FTDI_RS0_RI | FTDI_RS0_RLSD)
#define FTDI_STATUS_B1_MASK	(FTDI_RS_BI)
/* End TIOCMIWAIT */

#define FTDI_IMPL_ASYNC_FLAGS = ( ASYNC_SPD_HI | ASYNC_SPD_VHI \
 ASYNC_SPD_CUST | ASYNC_SPD_SHI | ASYNC_SPD_WARP )

/* function prototypes for a FTDI serial converter */
static int  ftdi_SIO_startup		(struct usb_serial *serial);
static int  ftdi_8U232AM_startup	(struct usb_serial *serial);
static int  ftdi_FT232BM_startup	(struct usb_serial *serial);
static void ftdi_shutdown		(struct usb_serial *serial);
static int  ftdi_open			(struct usb_serial_port *port, struct file *filp);
static void ftdi_close			(struct usb_serial_port *port, struct file *filp);
static int  ftdi_write			(struct usb_serial_port *port, int from_user, const unsigned char *buf, int count);
static int  ftdi_write_room		(struct usb_serial_port *port);
static int  ftdi_chars_in_buffer	(struct usb_serial_port *port);
static void ftdi_write_bulk_callback	(struct urb *urb);
static void ftdi_read_bulk_callback	(struct urb *urb);
static void ftdi_set_termios		(struct usb_serial_port *port, struct termios * old);
static int  ftdi_ioctl			(struct usb_serial_port *port, struct file * file, unsigned int cmd, unsigned long arg);
static void ftdi_break_ctl		(struct usb_serial_port *port, int break_state );
static void ftdi_throttle		(struct usb_serial_port *port);
static void ftdi_unthrottle		(struct usb_serial_port *port);

static unsigned short int ftdi_232am_baud_base_to_divisor (int baud, int base);
static unsigned short int ftdi_232am_baud_to_divisor (int baud);
static __u32 ftdi_232bm_baud_base_to_divisor (int baud, int base);
static __u32 ftdi_232bm_baud_to_divisor (int baud);

static struct usb_serial_device_type ftdi_SIO_device = {
	.owner =		THIS_MODULE,
	.name =			"FTDI SIO",
	.id_table =		id_table_sio,
	.num_interrupt_in =	0,
	.num_bulk_in =		1,
	.num_bulk_out =		1,
	.num_ports =		1,
	.open =			ftdi_open,
	.close =		ftdi_close,
	.throttle =		ftdi_throttle,
	.unthrottle =		ftdi_unthrottle,
	.write =		ftdi_write,
	.write_room =		ftdi_write_room,
	.chars_in_buffer =	ftdi_chars_in_buffer,
	.read_bulk_callback =	ftdi_read_bulk_callback,
	.write_bulk_callback =	ftdi_write_bulk_callback,
	.ioctl =		ftdi_ioctl,
	.set_termios =		ftdi_set_termios,
	.break_ctl =		ftdi_break_ctl,
	.startup =		ftdi_SIO_startup,
	.shutdown =		ftdi_shutdown,
};

static struct usb_serial_device_type ftdi_8U232AM_device = {
	.owner =		THIS_MODULE,
	.name =			"FTDI 8U232AM Compatible",
	.id_table =		id_table_8U232AM,
	.num_interrupt_in =	0,
	.num_bulk_in =		1,
	.num_bulk_out =		1,
	.num_ports =		1,
	.open =			ftdi_open,
	.close =		ftdi_close,
	.throttle =		ftdi_throttle,
	.unthrottle =		ftdi_unthrottle,
	.write =		ftdi_write,
	.write_room =		ftdi_write_room,
	.chars_in_buffer =	ftdi_chars_in_buffer,
	.read_bulk_callback =	ftdi_read_bulk_callback,
	.write_bulk_callback =	ftdi_write_bulk_callback,
	.ioctl =		ftdi_ioctl,
	.set_termios =		ftdi_set_termios,
	.break_ctl =		ftdi_break_ctl,
	.startup =		ftdi_8U232AM_startup,
	.shutdown =		ftdi_shutdown,
};

static struct usb_serial_device_type ftdi_FT232BM_device = {
	.owner =		THIS_MODULE,
	.name =			"FTDI FT232BM Compatible",
	.id_table =		id_table_FT232BM,
	.num_interrupt_in =	0,
	.num_bulk_in =		1,
	.num_bulk_out =		1,
	.num_ports =		1,
	.open =			ftdi_open,
	.close =		ftdi_close,
	.throttle =		ftdi_throttle,
	.unthrottle =		ftdi_unthrottle,
	.write =		ftdi_write,
	.write_room =		ftdi_write_room,
	.chars_in_buffer =	ftdi_chars_in_buffer,
	.read_bulk_callback =	ftdi_read_bulk_callback,
	.write_bulk_callback =	ftdi_write_bulk_callback,
	.ioctl =		ftdi_ioctl,
	.set_termios =		ftdi_set_termios,
	.break_ctl =		ftdi_break_ctl,
	.startup =		ftdi_FT232BM_startup,
	.shutdown =		ftdi_shutdown,
};



#define WDR_TIMEOUT (HZ * 5 ) /* default urb timeout */

/* High and low are for DTR, RTS etc etc */
#define HIGH 1
#define LOW 0

/*
 * ***************************************************************************
 * Utlity functions
 * ***************************************************************************
 */

static unsigned short int ftdi_232am_baud_base_to_divisor(int baud, int base)
{
	unsigned short int divisor;
	int divisor3 = base / 2 / baud; // divisor shifted 3 bits to the left
	if ((divisor3 & 0x7) == 7) divisor3 ++; // round x.7/8 up to x+1
	divisor = divisor3 >> 3;
	divisor3 &= 0x7;
	if (divisor3 == 1) divisor |= 0xc000; else // 0.125
	if (divisor3 >= 4) divisor |= 0x4000; else // 0.5
	if (divisor3 != 0) divisor |= 0x8000;      // 0.25
	if (divisor == 1) divisor = 0;	/* special case for maximum baud rate */
	return divisor;
}

static unsigned short int ftdi_232am_baud_to_divisor(int baud)
{
	 return(ftdi_232am_baud_base_to_divisor(baud, 48000000));
}

static __u32 ftdi_232bm_baud_base_to_divisor(int baud, int base)
{
	static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
	__u32 divisor;
	int divisor3 = base / 2 / baud; // divisor shifted 3 bits to the left
	divisor = divisor3 >> 3;
	divisor |= (__u32)divfrac[divisor3 & 0x7] << 14;
	/* Deal with special cases for highest baud rates. */
	if (divisor == 1) divisor = 0; else	// 1.0
	if (divisor == 0x4001) divisor = 1;	// 1.5
	return divisor;
}

static __u32 ftdi_232bm_baud_to_divisor(int baud)
{
	 return(ftdi_232bm_baud_base_to_divisor(baud, 48000000));
}

static int set_rts(struct usb_serial_port *port, int high_or_low)
{
	struct ftdi_private * priv = (struct ftdi_private *)port->private;
	char buf[1];
	unsigned ftdi_high_or_low;
	if (high_or_low) {
		ftdi_high_or_low = FTDI_SIO_SET_RTS_HIGH;
		priv->last_dtr_rts |= TIOCM_RTS;
	} else {
		ftdi_high_or_low = FTDI_SIO_SET_RTS_LOW;
		priv->last_dtr_rts &= ~TIOCM_RTS;
	}
	return(usb_control_msg(port->serial->dev,
			       usb_sndctrlpipe(port->serial->dev, 0),
			       FTDI_SIO_SET_MODEM_CTRL_REQUEST, 
			       FTDI_SIO_SET_MODEM_CTRL_REQUEST_TYPE,
			       ftdi_high_or_low, 0, 
			       buf, 0, WDR_TIMEOUT));
}


static int set_dtr(struct usb_serial_port *port, int high_or_low)
{
	struct ftdi_private * priv = (struct ftdi_private *)port->private;
	char buf[1];
	unsigned ftdi_high_or_low;
	if (high_or_low) {
		ftdi_high_or_low = FTDI_SIO_SET_DTR_HIGH;
		priv->last_dtr_rts |= TIOCM_DTR;
	} else {
		ftdi_high_or_low = FTDI_SIO_SET_DTR_LOW;
		priv->last_dtr_rts &= ~TIOCM_DTR;
	}
	return(usb_control_msg(port->serial->dev,
			       usb_sndctrlpipe(port->serial->dev, 0),
			       FTDI_SIO_SET_MODEM_CTRL_REQUEST, 
			       FTDI_SIO_SET_MODEM_CTRL_REQUEST_TYPE,
			       ftdi_high_or_low, 0, 
			       buf, 0, WDR_TIMEOUT));
}


static __u32 get_ftdi_divisor(struct usb_serial_port * port);


static int change_speed(struct usb_serial_port *port)
{
	char buf[1];
        __u16 urb_value;
	__u16 urb_index;
	__u32 urb_index_value;

	urb_index_value = get_ftdi_divisor(port);
	urb_value = (__u16)urb_index_value;
	urb_index = (__u16)(urb_index_value >> 16);
	
	return (usb_control_msg(port->serial->dev,
			    usb_sndctrlpipe(port->serial->dev, 0),
			    FTDI_SIO_SET_BAUDRATE_REQUEST,
			    FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE,
			    urb_value, urb_index,
			    buf, 0, 100) < 0);
}


static __u32 get_ftdi_divisor(struct usb_serial_port * port)
{ /* get_ftdi_divisor */
	
	struct ftdi_private * priv = (struct ftdi_private *)port->private;
	__u32 div_value = 0;
	int div_okay = 1;
	char *chip_name = "";
	int baud;

	/*
	 * The logic involved in setting the baudrate can be cleanly split in 3 steps.
	 * Obtaining the actual baud rate is a little tricky since unix traditionally
	 * somehow ignored the possibility to set non-standard baud rates.
	 * 1. Standard baud rates are set in tty->termios->c_cflag
	 * 2. If these are not enough, you can set any speed using alt_speed as follows:
	 *    - set tty->termios->c_cflag speed to B38400
	 *    - set your real speed in tty->alt_speed; it gets ignored when
	 *      alt_speed==0, (or)
	 *    - call TIOCSSERIAL ioctl with (struct serial_struct) set as follows:
	 *      flags & ASYNC_SPD_MASK == ASYNC_SPD_[HI, VHI, SHI, WARP], this just
	 *      sets alt_speed to (HI: 57600, VHI: 115200, SHI: 230400, WARP: 460800)
	 * ** Steps 1, 2 are done courtesy of tty_get_baud_rate
	 * 3. You can also set baud rate by setting custom divisor as follows
	 *    - set tty->termios->c_cflag speed to B38400
	 *    - call TIOCSSERIAL ioctl with (struct serial_struct) set as follows:
	 *      o flags & ASYNC_SPD_MASK == ASYNC_SPD_CUST
	 *      o custom_divisor set to baud_base / your_new_baudrate
	 * ** Step 3 is done courtesy of code borrowed from serial.c - I should really
	 *    spend some time and separate+move this common code to serial.c, it is
	 *    replicated in nearly every serial driver you see.
	 */

	/* 1. Get the baud rate from the tty settings, this observes alt_speed hack */

	baud = tty_get_baud_rate(port->tty);
	dbg("%s - tty_get_baud_rate reports speed %d", __FUNCTION__, baud);

	/* 2. Observe async-compatible custom_divisor hack, update baudrate if needed */

	if (baud == 38400 &&
	    ((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST) &&
	     (priv->custom_divisor)) {
		baud = priv->baud_base / priv->custom_divisor;
		dbg("%s - custom divisor %d sets baud rate to %d", __FUNCTION__, priv->custom_divisor, baud);
	}

	/* 3. Convert baudrate to device-specific divisor */

	if (!baud) baud = 9600;	
	switch(priv->chip_type) {
	case SIO: /* SIO chip */
		chip_name = "SIO";
		switch(baud) {
		case 300: div_value = ftdi_sio_b300; break;
		case 600: div_value = ftdi_sio_b600; break;
		case 1200: div_value = ftdi_sio_b1200; break;
		case 2400: div_value = ftdi_sio_b2400; break;
		case 4800: div_value = ftdi_sio_b4800; break;
		case 9600: div_value = ftdi_sio_b9600; break;
		case 19200: div_value = ftdi_sio_b19200; break;
		case 38400: div_value = ftdi_sio_b38400; break;
		case 57600: div_value = ftdi_sio_b57600;  break;
		case 115200: div_value = ftdi_sio_b115200; break;
		} /* baud */
		if (div_value == 0) {
  			dbg("%s - Baudrate (%d) requested is not supported", __FUNCTION__,  baud);
			div_value = ftdi_sio_b9600;
			div_okay = 0;
		}
		break;
	case FT8U232AM: /* 8U232AM chip */
		chip_name = "FT8U232AM";
		if (baud <= 3000000) {
			div_value = ftdi_232am_baud_to_divisor(baud);
		} else {
	                dbg("%s - Baud rate too high!", __FUNCTION__);
			div_value = ftdi_232am_baud_to_divisor(9600);
			div_okay = 0;
		}
		break;
	case FT232BM: /* FT232BM chip */
		chip_name = "FT232BM";
		if (baud <= 3000000) {
			div_value = ftdi_232bm_baud_to_divisor(baud);
		} else {
	                dbg("%s - Baud rate too high!", __FUNCTION__);
			div_value = ftdi_232bm_baud_to_divisor(9600);
			div_okay = 0;
		}
		break;
	} /* priv->chip_type */

	if (div_okay) {
		dbg("%s - Baud rate set to %d (divisor 0x%lX) on chip %s",
			__FUNCTION__, baud, (unsigned long)div_value, chip_name);
	}

	return(div_value);
}


static int get_serial_info(struct usb_serial_port * port, struct serial_struct * retinfo)
{
	struct ftdi_private * priv = (struct ftdi_private*) port->private;
	struct serial_struct tmp;

	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.flags = priv->flags;
	tmp.baud_base = priv->baud_base;
	tmp.custom_divisor = priv->custom_divisor;
	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;
	return 0;
} /* get_serial_info */


static int set_serial_info(struct usb_serial_port * port, struct serial_struct * newinfo)
{ /* set_serial_info */
	struct ftdi_private * priv = (struct ftdi_private *) port->private;
	struct serial_struct new_serial;
	struct ftdi_private old_priv;

	if (copy_from_user(&new_serial, newinfo, sizeof(new_serial)))
		return -EFAULT;
	old_priv = * priv;

	/* Do error checking and permission checking */

	if (!capable(CAP_SYS_ADMIN)) {
		if (((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (priv->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		priv->flags = ((priv->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		priv->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	if ((new_serial.baud_base != priv->baud_base) ||
	    (new_serial.baud_base < 9600))
		return -EINVAL;

	/* Make the changes - these are privileged changes! */

	priv->flags = ((priv->flags & ~ASYNC_FLAGS) |
	               (new_serial.flags & ASYNC_FLAGS));	
	priv->custom_divisor = new_serial.custom_divisor;

	port->tty->low_latency = (priv->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

check_and_exit:
	if ((old_priv.flags & ASYNC_SPD_MASK) !=
	     (priv->flags & ASYNC_SPD_MASK)) {
		if ((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			port->tty->alt_speed = 57600;
		else if ((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			port->tty->alt_speed = 115200;
		else if ((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			port->tty->alt_speed = 230400;
		else if ((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			port->tty->alt_speed = 460800;
		else
			port->tty->alt_speed = 0;
	}
	if (((old_priv.flags & ASYNC_SPD_MASK) !=
	     (priv->flags & ASYNC_SPD_MASK)) ||
	    (((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST) &&
	     (old_priv.custom_divisor != priv->custom_divisor))) {
		change_speed(port);
	}
	
	return (0);

} /* set_serial_info */

/*
 * ***************************************************************************
 * FTDI driver specific functions
 * ***************************************************************************
 */

/* Common startup subroutine */
/* Called from ftdi_SIO_startup, etc. */
static int ftdi_common_startup (struct usb_serial *serial)
{
	struct ftdi_private *priv;
	int i ;
	struct urb *urb; 
	
	dbg("%s",__FUNCTION__);

	priv = serial->port->private = kmalloc(sizeof(struct ftdi_private), GFP_KERNEL);
	if (!priv){
		err("%s- kmalloc(%Zd) failed.", __FUNCTION__, sizeof(struct ftdi_private));
		return -ENOMEM;
	}
	memset(priv, 0, sizeof(*priv));

        init_waitqueue_head(&priv->delta_msr_wait);
	/* This will push the characters through immediately rather
	   than queue a task to deliver them */
	priv->flags = ASYNC_LOW_LATENCY;

	/* create our write urb pool and transfer buffers - shared across all ftdi devices */ 
	spin_lock_init (&priv->write_urb_pool_lock);
	for (i = 0; i < NUM_URBS; ++i) {
		urb = usb_alloc_urb(0);
		priv->write_urb_pool[i] = urb;
		if (urb == NULL) {
			err("Unable to create new urb in urb pool");
			continue;
		}

		urb->transfer_buffer = NULL;
		urb->transfer_buffer = kmalloc (URB_TRANSFER_BUFFER_SIZE, GFP_KERNEL);
		if (!urb->transfer_buffer) {
			err("%s - out of memory for urb buffers.", 
			    __FUNCTION__);
			continue;
		}
	}

	
	return (0);
}


/* Startup for the SIO chip */
/* Called from usbserial:serial_probe */
static int ftdi_SIO_startup (struct usb_serial *serial)
{
	struct ftdi_private *priv;
	int err;

	dbg("%s",__FUNCTION__);

	err = ftdi_common_startup(serial);
	if (err){
		return (err);
	}

	priv = serial->port->private;
	priv->chip_type = SIO;
	priv->baud_base = 12000000 / 16;
	priv->write_offset = 1;
	
	return (0);
}

/* Startup for the 8U232AM chip */
/* Called from usbserial:serial_probe */
static int ftdi_8U232AM_startup (struct usb_serial *serial)
{ /* ftdi_8U232AM_startup */
	struct ftdi_private *priv;
	int err;

	dbg("%s",__FUNCTION__);
	err = ftdi_common_startup(serial);
	if (err){
		return (err);
	}

	priv = serial->port->private;
	priv->chip_type = FT8U232AM;
	priv->baud_base = 48000000 / 2; /* Would be / 16, but FTDI supports 0.125, 0.25 and 0.5 divisor fractions! */
	
	return (0);
} /* ftdi_8U232AM_startup */

/* Startup for the FT232BM chip */
/* Called from usbserial:serial_probe */
static int ftdi_FT232BM_startup (struct usb_serial *serial)
{ /* ftdi_FT232BM_startup */
	struct ftdi_private *priv;
	int err;

	dbg("%s",__FUNCTION__);
	err = ftdi_common_startup(serial);
	if (err){
		return (err);
	}

	priv = serial->port->private;
	priv->chip_type = FT232BM;
	priv->baud_base = 48000000 / 2; /* Would be / 16, but FT232BM supports multiple of 0.125 divisor fractions! */
	
	return (0);
} /* ftdi_FT232BM_startup */

/* ftdi_shutdown is called from usbserial:usb_serial_disconnect 
 *   it is called when the usb device is disconnected
 *
 *   usbserial:usb_serial_disconnect
 *      calls __serial_close for each open of the port
 *      shutdown is called then (ie ftdi_shutdown)
 */


static void ftdi_shutdown (struct usb_serial *serial)
{ /* ftdi_shutdown */
	
	struct usb_serial_port *port = &serial->port[0];	
	struct ftdi_private *priv = serial->port->private; 
	int i;
	unsigned long flags; 

	dbg("%s", __FUNCTION__);

	/* all open ports are closed at this point 
         *    (by usbserial.c:__serial_close, which calls ftdi_close)  
	 */

	
	/* Only execute this if this is the final open port for this device */
	if (port->open_count == 0){
		spin_lock_irqsave (&priv->write_urb_pool_lock, flags);

		for (i = 0; i < NUM_URBS; ++i) {
			if (priv->write_urb_pool[i]) {
				/* FIXME - uncomment the following usb_unlink_urb call when
				 * the host controllers get fixed to set urb->dev = NULL after
				 * the urb is finished.  Otherwise this call oopses. */
				/* usb_unlink_urb(priv->write_urb_pool[i]); */
				if (priv->write_urb_pool[i]->transfer_buffer) {
					kfree(priv->write_urb_pool[i]->transfer_buffer);
					priv->write_urb_pool[i]->transfer_buffer = NULL;
				}
				usb_free_urb (priv->write_urb_pool[i]);
				priv->write_urb_pool[i] = NULL;
			}
		}

		spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);
		/* usb_disconnect shuts down the port->read_urb so don't do it here */
		/* as was done previously */
	}
	if (serial->port->private){
		kfree(serial->port->private);
		serial->port->private = NULL;
	}
} /* ftdi_shutdown */


static int  ftdi_open (struct usb_serial_port *port, struct file *filp)
{ /* ftdi_open */
	struct termios tmp_termios;
	struct usb_serial *serial = port->serial;
	struct ftdi_private *priv = port->private;
	
	int result = 0;
	char buf[1]; /* Needed for the usb_control_msg I think */

	dbg("%s", __FUNCTION__);


	port->tty->low_latency = (priv->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	/* No error checking for this (will get errors later anyway) */
	/* See ftdi_sio.h for description of what is reset */
	usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			FTDI_SIO_RESET_REQUEST, FTDI_SIO_RESET_REQUEST_TYPE, 
			FTDI_SIO_RESET_SIO, 
			0, buf, 0, WDR_TIMEOUT);

	/* Termios defaults are set by usb_serial_init. We don't change
	   port->tty->termios - this would loose speed settings, etc.
	   This is same behaviour as serial.c/rs_open() - Kuba */

	/* ftdi_set_termios  will send usb control messages */
	ftdi_set_termios(port, &tmp_termios);

	/* FIXME: Flow control might be enabled, so it should be checked -
	   we have no control of defaults! */
	/* Turn on RTS and DTR since we are not flow controlling by default */
	if (set_dtr(port, HIGH) < 0) {
		err("%s Error from DTR HIGH urb", __FUNCTION__);
	}
	if (set_rts(port, HIGH) < 0){
		err("%s Error from RTS HIGH urb", __FUNCTION__);
	}

	/* Make sure write_urb is initialised since a write_pool is used now */
	port->write_urb = NULL; /* prevents usbserial.c from trying something silly */


	/* Start reading from the device */
	FILL_BULK_URB(port->read_urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
		      port->read_urb->transfer_buffer, port->read_urb->transfer_buffer_length,
		      ftdi_read_bulk_callback, port);
	result = usb_submit_urb(port->read_urb);
	if (result)
		err("%s - failed submitting read urb, error %d", __FUNCTION__, result);


	return result;
} /* ftdi_open */



/* 
 * usbserial:__serial_close  only calls ftdi_close if the point is open
 *
 *   This only gets called when it is the last close
 *   
 *   
 */

static void ftdi_close (struct usb_serial_port *port, struct file *filp)
{ /* ftdi_close */
	struct usb_serial *serial;
	unsigned int c_cflag = port->tty->termios->c_cflag;
	char buf[1];

	dbg("%s", __FUNCTION__);

	serial = get_usb_serial ( port, __FUNCTION__);
	if (!serial)
		return;

	if (serial->dev) {
		if (c_cflag & HUPCL){
			/* Disable flow control */
			if (usb_control_msg(serial->dev, 
					    usb_sndctrlpipe(serial->dev, 0),
					    FTDI_SIO_SET_FLOW_CTRL_REQUEST,
					    FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
					    0, 0, buf, 0, WDR_TIMEOUT) < 0) {
				err("error from flowcontrol urb");
			}	    

			/* drop DTR */
			if (set_dtr(port, LOW) < 0){
				err("Error from DTR LOW urb");
			}
			/* drop RTS */
			if (set_rts(port, LOW) < 0) {
				err("Error from RTS LOW urb");
			}	
			/* shutdown our bulk read */
			if (port->read_urb) {
				usb_unlink_urb (port->read_urb);	
			}
			/* unlink the running write urbs */
			

		} /* Note change no line is hupcl is off */
	} /* if (serial->dev) */


} /* ftdi_close */


  
/* The SIO requires the first byte to have:
 *  B0 1
 *  B1 0
 *  B2..7 length of message excluding byte 0
 *
 * The new devices do not require this byte
 */
static int ftdi_write (struct usb_serial_port *port, int from_user,
			   const unsigned char *buf, int count)
{ /* ftdi_write */
	struct usb_serial *serial = get_usb_serial ( port, __FUNCTION__);
	struct ftdi_private *priv = (struct ftdi_private *)port->private;
	unsigned char *first_byte;
	int data_offset ;       /* will be 1 for the SIO and 0 otherwise */
	int result;
	int user_bytes_sent = 0 ;   /* amount of user data sent */

	/* Variables for urb pool management */
	unsigned char *current_position = (unsigned char *)buf;
	int i; 
	struct urb *urb; /* pointer to urb from urb pool */
	unsigned long flags;
	/* end of urb pool management */
	
	
	dbg("%s port %d, %d bytes", __FUNCTION__, port->number, count);

	if (count == 0) {
		err("write request of 0 bytes");
		goto exit;
	}
	
	data_offset = priv->write_offset;
        dbg("data_offset set to %d",data_offset);

	while (count > 0) {
		/* urb_byte_count = user_byte_count + data_offset */
		int urb_byte_count;  /* Number of bytes of URB data   */
		int user_byte_count; /* Number of bytes of user data */

		/* Find a free urb in the list */
		urb = NULL;

		spin_lock_irqsave (&(priv->write_urb_pool_lock), flags) ; 

		for (i = 0 ; i < NUM_URBS; i++) {
			if (priv->write_urb_pool[i] -> status != -EINPROGRESS) {
			  urb = priv->write_urb_pool[i];
			  /* Must make sure another device doesn't grab this */
			  /* BUT unfortunately the uhci stack errors if it sees this */
			  /* so have to increase the size of the spin_lock */
			  /* urb->status = -EINPROGRESS; */
				break;
			}
		}


		if (urb == NULL) {
		        spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);
			dbg("%s - no more free urbs", __FUNCTION__);
			goto exit;
		}

		/* Allocate memory for the urb if necessary */
		if (urb->transfer_buffer == NULL) {
			urb->transfer_buffer = kmalloc (URB_TRANSFER_BUFFER_SIZE, GFP_KERNEL);
			if (urb->transfer_buffer == NULL) {
				spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);
				err("%s ran out of kernel memory for urb ...", __FUNCTION__);
				goto exit;
			}
		}
		
		/* The original sio needs the first byte to contain the bytecount 
		 * so the urb may be one byte bigger than the user data 
		 */
		urb_byte_count = min (count + data_offset, URB_TRANSFER_BUFFER_SIZE);
		user_byte_count = urb_byte_count - data_offset; 

		/* Copy in the data to send */
		if (from_user) {
			if (copy_from_user(urb->transfer_buffer + data_offset,
					   current_position, user_byte_count )){
				spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);
				return -EFAULT;
			}
		} else {
			memcpy(urb->transfer_buffer + data_offset,
			       current_position, user_byte_count );
		}  

		first_byte = urb->transfer_buffer;
		if (data_offset > 0){
			/* Write the control byte at the front of the packet*/
			*first_byte = 1 | ((user_byte_count) << 2) ; 
			dbg("%s Bytes: %d, First Byte: 0x%02x", __FUNCTION__,count, first_byte[0]);
		}
		
		usb_serial_debug_data (__FILE__, __FUNCTION__, urb_byte_count, first_byte);
		
		/* fill the buffer and send it */
		FILL_BULK_URB(urb, serial->dev, 
			      usb_sndbulkpipe(serial->dev, port->bulk_out_endpointAddress),
			      urb->transfer_buffer, urb_byte_count,
			      ftdi_write_bulk_callback, port);
		urb->transfer_flags |= USB_QUEUE_BULK;
		
		result = usb_submit_urb(urb);
		if (result) {
			spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);
			err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
			user_bytes_sent = result;
			goto exit;
		}
		spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);

		/* house keeping */
		current_position += user_byte_count;
		user_bytes_sent += user_byte_count;
		count -= user_byte_count;
		
	} /* while  count > 0 */

 exit:

	dbg("%s write returning: %d", __FUNCTION__, user_bytes_sent);
	return user_bytes_sent;

} /* ftdi_write */


/* This function may get called when the device is closed */

static void ftdi_write_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);

	dbg("%s", __FUNCTION__);

	if (port_paranoia_check (port, __FUNCTION__))
		return;
	
	if (urb->status) {
		dbg("nonzero write bulk status received: %d", urb->status);
		return;
	}

	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}

	/* Have to check for validity of queueing up the tasks */
	dbg("%s - port->open_count = %d", __FUNCTION__, port->open_count);

 	if (port->open_count > 0){
		queue_task(&port->tqueue, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
	} 

	return;
} /* ftdi_write_bulk_callback */


static int ftdi_write_room( struct usb_serial_port *port )
{
	struct ftdi_private *priv = (struct ftdi_private *)port->private;
	int room = 0;
	int i;
	unsigned long flags;


	spin_lock_irqsave (&priv->write_urb_pool_lock, flags);

	for (i = 0; i < NUM_URBS; i++) {
		if (priv->write_urb_pool[i]->status != -EINPROGRESS) {
			room += URB_TRANSFER_BUFFER_SIZE - priv->write_offset;
		}
	}
	
	spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);

	dbg("%s - returns %d", __FUNCTION__, room);	
	return(room);
} /* ftdi_write_room */


static int ftdi_chars_in_buffer (struct usb_serial_port *port)
{ /* ftdi_chars_in_buffer */
	unsigned long flags;
	int i;
	int chars = 0;
	struct ftdi_private *priv = (struct ftdi_private *)port->private;
	int data_offset = priv->write_offset;

	dbg("%s - port %d", __FUNCTION__, port->number);

	spin_lock_irqsave (&priv->write_urb_pool_lock, flags);

	/* tally up the number of bytes waiting */
	for (i = 0; i < NUM_URBS; ++i) {
		if (priv->write_urb_pool[i]->status == -EINPROGRESS) {
			chars += URB_TRANSFER_BUFFER_SIZE - data_offset;
		}
	}

	spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);

	dbg("%s - returns %d", __FUNCTION__, chars);

	return (chars);

} /* ftdi_chars_in_buffer */



static void ftdi_read_bulk_callback (struct urb *urb)
{ /* ftdi_read_bulk_callback */
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct usb_serial *serial = get_usb_serial(port,__FUNCTION__);
       	struct tty_struct *tty = port->tty ;
	struct ftdi_private *priv = (struct ftdi_private *) port->private;
	char error_flag;
       	unsigned char *data = urb->transfer_buffer;

	const int data_offset = 2;
	int i;
	int result;

	dbg("%s", __FUNCTION__);

	if (port_paranoia_check (port, __FUNCTION__)) {
		return;
	}
	if (port->open_count <= 0)
		return;

	if (!serial){
		dbg("%s - bad serial pointer - exiting",__FUNCTION__);
		return;
	}
	
	if (!tty) {
		dbg("%s - bad tty pointer - exiting",__FUNCTION__);
		return;
	}


	if (urb->status) {
		/* This will happen at close every time so it is a dbg not an err */
		dbg("(this is ok on close) nonzero read bulk status received: %d", urb->status);
		return;
	}

        /* The first two bytes of every read packet are status */
	if (urb->actual_length > 2) {
		usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, data);
	} else {
                dbg("Status only: %03oo %03oo",data[0],data[1]);
        }


	/* TO DO -- check for hung up line and handle appropriately: */
	/*   send hangup  */
	/* See acm.c - you do a tty_hangup  - eg tty_hangup(tty) */
	/* if CD is dropped and the line is not CLOCAL then we should hangup */

	/* Compare new line status to the old one, signal if different */
	if (priv != NULL) {
		char new_status = data[0] & FTDI_STATUS_B0_MASK;
		if (new_status != priv->prev_status) {
			priv->diff_status |= new_status ^ priv->prev_status;
			wake_up_interruptible(&priv->delta_msr_wait);
			priv->prev_status = new_status;
		}
	}

	/* Handle errors and break */
	error_flag = TTY_NORMAL;
        /* Although the device uses a bitmask and hence can have multiple */
        /* errors on a packet - the order here sets the priority the */
        /* error is returned to the tty layer  */
	
	if ( data[1] & FTDI_RS_OE ) { 
		error_flag = TTY_OVERRUN;
                dbg("OVERRRUN error");
	}
	if ( data[1] & FTDI_RS_BI ) { 
		error_flag = TTY_BREAK;
                dbg("BREAK received");
	}
	if ( data[1] & FTDI_RS_PE ) { 
		error_flag = TTY_PARITY;
                dbg("PARITY error");
	}
	if ( data[1] & FTDI_RS_FE ) { 
		error_flag = TTY_FRAME;
                dbg("FRAMING error");
	}
	if (urb->actual_length > data_offset) {

		for (i = data_offset ; i < urb->actual_length ; ++i) {
			/* have to make sure we don't overflow the buffer
			  with tty_insert_flip_char's */
			if(tty->flip.count >= TTY_FLIPBUF_SIZE) {
				tty_flip_buffer_push(tty);
			}
			/* Note that the error flag is duplicated for 
			   every character received since we don't know
			   which character it applied to */
			tty_insert_flip_char(tty, data[i], error_flag);
		}
	  	tty_flip_buffer_push(tty);
	} 

#ifdef NOT_CORRECT_BUT_KEEPING_IT_FOR_NOW
	/* if a parity error is detected you get status packets forever
	   until a character is sent without a parity error.
	   This doesn't work well since the application receives a never
	   ending stream of bad data - even though new data hasn't been sent.
	   Therefore I (bill) have taken this out.
	   However - this might make sense for framing errors and so on 
	   so I am leaving the code in for now.
	*/
      else {
		if (error_flag != TTY_NORMAL){
			dbg("error_flag is not normal");
				/* In this case it is just status - if that is an error send a bad character */
				if(tty->flip.count >= TTY_FLIPBUF_SIZE) {
					tty_flip_buffer_push(tty);
				}
				tty_insert_flip_char(tty, 0xff, error_flag);
				tty_flip_buffer_push(tty);
		}
	}
#endif

	/* if the port is closed stop trying to read */
	if (port->open_count > 0){
		/* Continue trying to always read  */
		FILL_BULK_URB(port->read_urb, serial->dev, 
			      usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
			      port->read_urb->transfer_buffer, port->read_urb->transfer_buffer_length,
			      ftdi_read_bulk_callback, port);

		result = usb_submit_urb(port->read_urb);
		if (result)
			err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
	}

	return;
} /* ftdi_read_bulk_callback */


static void ftdi_break_ctl( struct usb_serial_port *port, int break_state )
{
	struct usb_serial *serial = port->serial;
	struct ftdi_private *priv = (struct ftdi_private *)port->private;
	__u16 urb_value = 0; 
	char buf[1];
	
	/* break_state = -1 to turn on break, and 0 to turn off break */
	/* see drivers/char/tty_io.c to see it used */
	/* last_set_data_urb_value NEVER has the break bit set in it */

	if (break_state) {
		urb_value = priv->last_set_data_urb_value | FTDI_SIO_SET_BREAK;
	} else {
		urb_value = priv->last_set_data_urb_value; 
	}

	
	if (usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			    FTDI_SIO_SET_DATA_REQUEST, 
			    FTDI_SIO_SET_DATA_REQUEST_TYPE,
			    urb_value , 0,
			    buf, 0, WDR_TIMEOUT) < 0) {
		err("%s FAILED to enable/disable break state (state was %d)", __FUNCTION__,break_state);
	}	   

	dbg("%s break state is %d - urb is %d", __FUNCTION__,break_state, urb_value);
	
}


/* old_termios contains the original termios settings and tty->termios contains
 * the new setting to be used
 * WARNING: set_termios calls this with old_termios in kernel space
 */

static void ftdi_set_termios (struct usb_serial_port *port, struct termios *old_termios)
{ /* ftdi_termios */
	struct usb_serial *serial = port->serial;
	unsigned int cflag = port->tty->termios->c_cflag;
	struct ftdi_private *priv = (struct ftdi_private *)port->private;	
	__u16 urb_value; /* will hold the new flags */
	char buf[1]; /* Perhaps I should dynamically alloc this? */
	
	
	dbg("%s", __FUNCTION__);


	/* FIXME -For this cut I don't care if the line is really changing or 
	   not  - so just do the change regardless  - should be able to 
	   compare old_termios and tty->termios */
	/* NOTE These routines can get interrupted by 
	   ftdi_sio_read_bulk_callback  - need to examine what this 
           means - don't see any problems yet */
	
	/* Set number of data bits, parity, stop bits */
	
	urb_value = 0;
	urb_value |= (cflag & CSTOPB ? FTDI_SIO_SET_DATA_STOP_BITS_2 :
		      FTDI_SIO_SET_DATA_STOP_BITS_1);
	urb_value |= (cflag & PARENB ? 
		      (cflag & PARODD ? FTDI_SIO_SET_DATA_PARITY_ODD : 
		       FTDI_SIO_SET_DATA_PARITY_EVEN) :
		      FTDI_SIO_SET_DATA_PARITY_NONE);
	if (cflag & CSIZE) {
		switch (cflag & CSIZE) {
		case CS5: urb_value |= 5; dbg("Setting CS5"); break;
		case CS6: urb_value |= 6; dbg("Setting CS6"); break;
		case CS7: urb_value |= 7; dbg("Setting CS7"); break;
		case CS8: urb_value |= 8; dbg("Setting CS8"); break;
		default:
			err("CSIZE was set but not CS5-CS8");
		}
	}

	/* This is needed by the break command since it uses the same command - but is
	 *  or'ed with this value  */
	priv->last_set_data_urb_value = urb_value;
	
	if (usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			    FTDI_SIO_SET_DATA_REQUEST, 
			    FTDI_SIO_SET_DATA_REQUEST_TYPE,
			    urb_value , 0,
			    buf, 0, 100) < 0) {
		err("%s FAILED to set databits/stopbits/parity", __FUNCTION__);
	}	   

	/* Now do the baudrate */
	if ((cflag & CBAUD) == B0 ) {
		/* Disable flow control */
		if (usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
				    FTDI_SIO_SET_FLOW_CTRL_REQUEST, 
				    FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
				    0, 0, 
				    buf, 0, WDR_TIMEOUT) < 0) {
			err("%s error from disable flowcontrol urb", __FUNCTION__);
		}	    
		/* Drop RTS and DTR */
		if (set_dtr(port, LOW) < 0){
			err("%s Error from DTR LOW urb", __FUNCTION__);
		}
		if (set_rts(port, LOW) < 0){
			err("%s Error from RTS LOW urb", __FUNCTION__);
		}	
		
	} else {
		/* set the baudrate determined before */
		if (change_speed(port)) {
			err("%s urb failed to set baurdrate", __FUNCTION__);
		}
	}

	/* Set flow control */
	/* Note device also supports DTR/CD (ugh) and Xon/Xoff in hardware */
	if (cflag & CRTSCTS) {
		dbg("%s Setting to CRTSCTS flow control", __FUNCTION__);
		if (usb_control_msg(serial->dev, 
				    usb_sndctrlpipe(serial->dev, 0),
				    FTDI_SIO_SET_FLOW_CTRL_REQUEST, 
				    FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
				    0 , FTDI_SIO_RTS_CTS_HS,
				    buf, 0, WDR_TIMEOUT) < 0) {
			err("urb failed to set to rts/cts flow control");
		}		
		
	} else { 
		/* CHECKME Assuming XON/XOFF handled by tty stack - not by device */
		dbg("%s Turning off hardware flow control", __FUNCTION__);
		if (usb_control_msg(serial->dev, 
				    usb_sndctrlpipe(serial->dev, 0),
				    FTDI_SIO_SET_FLOW_CTRL_REQUEST, 
				    FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
				    0, 0, 
				    buf, 0, WDR_TIMEOUT) < 0) {
			err("urb failed to clear flow control");
		}				
		
	}
	return;
} /* ftdi_termios */


static int ftdi_ioctl (struct usb_serial_port *port, struct file * file, unsigned int cmd, unsigned long arg)
{
	struct usb_serial *serial = port->serial;
	struct ftdi_private *priv = (struct ftdi_private *)port->private;

	__u16 urb_value=0; /* Will hold the new flags */
	char buf[2];
	int  ret, mask;
	
	dbg("%s cmd 0x%04x", __FUNCTION__, cmd);

	/* Based on code from acm.c and others */
	switch (cmd) {

	case TIOCMGET:
		dbg("%s TIOCMGET", __FUNCTION__);
		switch (priv->chip_type) {
		case SIO:
			/* Request the status from the device */
			if ((ret = usb_control_msg(serial->dev, 
						   usb_rcvctrlpipe(serial->dev, 0),
						   FTDI_SIO_GET_MODEM_STATUS_REQUEST, 
						   FTDI_SIO_GET_MODEM_STATUS_REQUEST_TYPE,
						   0, 0, 
						   buf, 1, WDR_TIMEOUT)) < 0 ) {
				err("%s Could not get modem status of device - err: %d", __FUNCTION__,
				    ret);
				return(ret);
			}
			break;
		case FT8U232AM:
		case FT232BM:
			/* the 8U232AM returns a two byte value (the sio is a 1 byte value) - in the same
			   format as the data returned from the in point */
			if ((ret = usb_control_msg(serial->dev, 
						   usb_rcvctrlpipe(serial->dev, 0),
						   FTDI_SIO_GET_MODEM_STATUS_REQUEST, 
						   FTDI_SIO_GET_MODEM_STATUS_REQUEST_TYPE,
						   0, 0, 
						   buf, 2, WDR_TIMEOUT)) < 0 ) {
				err("%s Could not get modem status of device - err: %d", __FUNCTION__,
				    ret);
				return(ret);
			}
			break;
		default:
			return -EFAULT;
			break;
		}

		return put_user((buf[0] & FTDI_SIO_DSR_MASK ? TIOCM_DSR : 0) |
				(buf[0] & FTDI_SIO_CTS_MASK ? TIOCM_CTS : 0) |
				(buf[0]  & FTDI_SIO_RI_MASK  ? TIOCM_RI  : 0) |
				(buf[0]  & FTDI_SIO_RLSD_MASK ? TIOCM_CD  : 0) |
				priv->last_dtr_rts,
				(unsigned long *) arg);
		break;

	case TIOCMSET: /* Turns on and off the lines as specified by the mask */
		dbg("%s TIOCMSET", __FUNCTION__);
		if (get_user(mask, (unsigned long *) arg))
			return -EFAULT;
		urb_value = ((mask & TIOCM_DTR) ? HIGH : LOW);
		if ((ret = set_dtr(port, urb_value)) < 0){
			err("Error from DTR set urb (TIOCMSET)");
			return(ret);
		}
		urb_value = ((mask & TIOCM_RTS) ? HIGH : LOW);
		if ((ret = set_rts(port, urb_value)) < 0){
			err("Error from RTS set urb (TIOCMSET)");
			return(ret);
		}
		return(0);
		break;
					
	case TIOCMBIS: /* turns on (Sets) the lines as specified by the mask */
		dbg("%s TIOCMBIS", __FUNCTION__);
 	        if (get_user(mask, (unsigned long *) arg))
			return -EFAULT;
  	        if (mask & TIOCM_DTR){
			if ((ret = set_dtr(port, HIGH)) < 0) {
				err("Urb to set DTR failed");
				return(ret);
			}
		}
		if (mask & TIOCM_RTS) {
			if ((ret = set_rts(port, HIGH)) < 0){
				err("Urb to set RTS failed");
				return(ret);
			}
		}
		return(0);
		break;

	case TIOCMBIC: /* turns off (Clears) the lines as specified by the mask */
		dbg("%s TIOCMBIC", __FUNCTION__);
 	        if (get_user(mask, (unsigned long *) arg))
			return -EFAULT;
  	        if (mask & TIOCM_DTR){
			if ((ret = set_dtr(port, LOW)) < 0){
				err("Urb to unset DTR failed");
				return(ret);
			}
		}	
		if (mask & TIOCM_RTS) {
			if ((ret = set_rts(port, LOW)) < 0){
				err("Urb to unset RTS failed");
				return(ret);
			}
		}
		return(0);
		break;

		/*
		 * I had originally implemented TCSET{A,S}{,F,W} and
		 * TCGET{A,S} here separately, however when testing I
		 * found that the higher layers actually do the termios
		 * conversions themselves and pass the call onto
		 * ftdi_sio_set_termios. 
		 *
		 */

	case TIOCGSERIAL: /* gets serial port data */
		return get_serial_info(port, (struct serial_struct *) arg);

	case TIOCSSERIAL: /* sets serial port data */
		return set_serial_info(port, (struct serial_struct *) arg);

	/*
	 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
	 * - mask passed in arg for lines of interest
	 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
	 * Caller should use TIOCGICOUNT to see which one it was.
	 *
	 * This code is borrowed from linux/drivers/char/serial.c
	 */
	case TIOCMIWAIT:
		while (priv != NULL) {
			interruptible_sleep_on(&priv->delta_msr_wait);
			/* see if a signal did it */
			if (signal_pending(current))
				return -ERESTARTSYS;
			else {
				char diff = priv->diff_status;

				if (diff == 0) {
					return -EIO; /* no change => error */
				}

				/* Consume all events */
				priv->diff_status = 0;

				/* Return 0 if caller wanted to know about these bits */
				if ( ((arg & TIOCM_RNG) && (diff & FTDI_RS0_RI)) ||
				     ((arg & TIOCM_DSR) && (diff & FTDI_RS0_DSR)) ||
				     ((arg & TIOCM_CD)  && (diff & FTDI_RS0_RLSD)) ||
				     ((arg & TIOCM_CTS) && (diff & FTDI_RS0_CTS)) ) {
					return 0;
				}
				/*
				 * Otherwise caller can't care less about what happened,
				 * and so we continue to wait for more events.
				 */
			}
		}
		return(0);
		break;
	default:
		break;
		
	}


	/* This is not necessarily an error - turns out the higher layers will do 
	 *  some ioctls itself (see comment above)
	 */
	dbg("%s arg not supported - it was 0x%04x - check /usr/include/asm/ioctls.h", __FUNCTION__, cmd);

	return(-ENOIOCTLCMD);
} /* ftdi_ioctl */


static void ftdi_throttle (struct usb_serial_port *port)
{
	dbg("%s - port %d", __FUNCTION__, port->number);
	usb_unlink_urb (port->read_urb);
}


static void ftdi_unthrottle (struct usb_serial_port *port)
{
	int result;
	struct usb_serial *serial = port->serial;

	dbg("%s - port %d", __FUNCTION__, port->number);

	port->read_urb->dev = serial->dev;

	FILL_BULK_URB(port->read_urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
		      port->read_urb->transfer_buffer, port->read_urb->transfer_buffer_length,
		      ftdi_read_bulk_callback, port);

	result = usb_submit_urb(port->read_urb);
	if (result)
		err("%s - failed submitting read urb, error %d", __FUNCTION__, result);
}

static int __init ftdi_init (void)
{

	dbg("%s", __FUNCTION__);
	usb_serial_register (&ftdi_SIO_device);
	usb_serial_register (&ftdi_8U232AM_device);
	usb_serial_register (&ftdi_FT232BM_device);


	info(DRIVER_VERSION ":" DRIVER_DESC);
	return 0;
}


static void __exit ftdi_exit (void)
{

	dbg("%s", __FUNCTION__);

	usb_serial_deregister (&ftdi_FT232BM_device);
	usb_serial_deregister (&ftdi_8U232AM_device);
	usb_serial_deregister (&ftdi_SIO_device);

}


module_init(ftdi_init);
module_exit(ftdi_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");

