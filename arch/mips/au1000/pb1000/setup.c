/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Alchemy Pb1000 board setup.
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/mc146818rtc.h>
#include <linux/delay.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/keyboard.h>
#include <asm/mipsregs.h>
#include <asm/reboot.h>
#include <asm/pgtable.h>
#include <asm/au1000.h>
#include <asm/pb1000.h>

#ifdef CONFIG_USB_OHCI
// Enable the workaround for the OHCI DoneHead
// register corruption problem.
#define CONFIG_AU1000_OHCI_FIX
#endif

#if defined(CONFIG_AU1000_SERIAL_CONSOLE)
extern void console_setup(char *, int *);
char serial_console[20];
#endif

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
extern void * __rd_start, * __rd_end;
#endif

#ifdef CONFIG_BLK_DEV_IDE
extern struct ide_ops std_ide_ops;
extern struct ide_ops *ide_ops;
#endif

void (*__wbflush) (void);
extern struct rtc_ops no_rtc_ops;
extern char * __init prom_getcmdline(void);
extern void au1000_restart(char *);
extern void au1000_halt(void);
extern void au1000_power_off(void);
extern struct resource ioport_resource;
extern struct resource iomem_resource;


void au1000_wbflush(void)
{
	__asm__ volatile ("sync");
}

void __init au1000_setup(void)
{
	char *argptr;
	u32 pin_func, static_cfg0;
	u32 sys_freqctrl, sys_clksrc;
	u32 prid = read_32bit_cp0_register(CP0_PRID);
	
	argptr = prom_getcmdline();

	/* Various early Au1000 Errata corrected by this */
	set_cp0_config(1<<19); /* Config[OD] */

#ifdef CONFIG_AU1000_SERIAL_CONSOLE
	if ((argptr = strstr(argptr, "console=")) == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " console=ttyS0,115200");
	}
#endif	  

	rtc_ops = &no_rtc_ops;
        __wbflush = au1000_wbflush;
	_machine_restart = au1000_restart;
	_machine_halt = au1000_halt;
	_machine_power_off = au1000_power_off;

	// IO/MEM resources. 
	set_io_port_base(0);
	ioport_resource.start = 0x10000000;
	ioport_resource.end = 0xffffffff;
	iomem_resource.start = 0x10000000;
	iomem_resource.end = 0xffffffff;

#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	initrd_start = (unsigned long)&__rd_start;
	initrd_end = (unsigned long)&__rd_end;
#endif

	// set AUX clock to 12MHz * 8 = 96 MHz
	outl(8, SYS_AUXPLL);
	outl(0, SYS_PINSTATERD);
	udelay(100);

#if defined (CONFIG_USB_OHCI) || defined (CONFIG_AU1000_USB_DEVICE)
#ifdef CONFIG_USB_OHCI
	if ((argptr = strstr(argptr, "usb_ohci=")) == NULL) {
	        char usb_args[80];
		argptr = prom_getcmdline();
		memset(usb_args, 0, sizeof(usb_args));
		sprintf(usb_args, " usb_ohci=base:0x%x,len:0x%x,irq:%d",
			USB_OHCI_BASE, USB_OHCI_LEN, AU1000_USB_HOST_INT);
		strcat(argptr, usb_args);
	}
#endif

	/* zero and disable FREQ2 */
	sys_freqctrl = inl(SYS_FREQCTRL0);
	sys_freqctrl &= ~0xFFF00000;
	outl(sys_freqctrl, SYS_FREQCTRL0);

	/* zero and disable USBH/USBD clocks */
	sys_clksrc = inl(SYS_CLKSRC);
	sys_clksrc &= ~0x00007FE0;
	outl(sys_clksrc, SYS_CLKSRC);

	sys_freqctrl = inl(SYS_FREQCTRL0);
	sys_freqctrl &= ~0xFFF00000;

	sys_clksrc = inl(SYS_CLKSRC);
	sys_clksrc &= ~0x00007FE0;

	switch (prid & 0x000000FF)
	{
	case 0x00: /* DA */
	case 0x01: /* HA */
	case 0x02: /* HB */
	/* CPU core freq to 48MHz to slow it way down... */
	outl(4, SYS_CPUPLL);

	/*
	 * Setup 48MHz FREQ2 from CPUPLL for USB Host
	 */
	/* FRDIV2=3 -> div by 8 of 384MHz -> 48MHz */
	sys_freqctrl |= ((3<<22) | (1<<21) | (0<<20));
	outl(sys_freqctrl, SYS_FREQCTRL0);

	/* CPU core freq to 384MHz */
	outl(0x20, SYS_CPUPLL);

	printk("Au1000: 48MHz OHCI workaround enabled\n");
		break;

	default:  /* HC and newer */
	// FREQ2 = aux/2 = 48 MHz
	sys_freqctrl |= ((0<<22) | (1<<21) | (1<<20));
	outl(sys_freqctrl, SYS_FREQCTRL0);
		break;
	}

	/*
	 * Route 48MHz FREQ2 into USB Host and/or Device
	 */
#ifdef CONFIG_USB_OHCI
	sys_clksrc |= ((4<<12) | (0<<11) | (0<<10));
#endif
#ifdef CONFIG_AU1000_USB_DEVICE
	sys_clksrc |= ((4<<7) | (0<<6) | (0<<5));
#endif
	outl(sys_clksrc, SYS_CLKSRC);

#ifdef CONFIG_USB_OHCI
	// enable host controller and wait for reset done
	outl(0x08, USB_HOST_CONFIG);
	udelay(1000);
	outl(0x0E, USB_HOST_CONFIG);
	udelay(1000);
	inl(USB_HOST_CONFIG); // throw away first read
	while (!(inl(USB_HOST_CONFIG) & 0x10))
		inl(USB_HOST_CONFIG);
#endif
	
	// configure pins GPIO[14:9] as GPIO
	pin_func = inl(SYS_PINFUNC) & (u32)(~0x8080);

#ifndef CONFIG_AU1000_USB_DEVICE
	// 2nd USB port is USB host
	pin_func |= 0x8000;
#endif
	outl(pin_func, SYS_PINFUNC);
	outl(0x2800, SYS_TRIOUTCLR);
	outl(0x0030, SYS_OUTPUTCLR);
#endif // defined (CONFIG_USB_OHCI) || defined (CONFIG_AU1000_USB_DEVICE)

	// make gpio 15 an input (for interrupt line) 
	pin_func = inl(SYS_PINFUNC) & (u32)(~0x100);
	// we don't need I2S, so make it available for GPIO[31:29] 
	pin_func |= (1<<5);
	outl(pin_func, SYS_PINFUNC);

	outl(0x8000, SYS_TRIOUTCLR);
	
#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

	static_cfg0 = inl(MEM_STCFG0) & (u32)(~0xc00);
	outl(static_cfg0, MEM_STCFG0);

	// configure RCE2* for LCD
	outl(0x00000004, MEM_STCFG2);

	// MEM_STTIME2
	outl(0x09000000, MEM_STTIME2);

	// Set 32-bit base address decoding for RCE2*
	outl(0x10003ff0, MEM_STADDR2);

	// PCI CPLD setup
	// expand CE0 to cover PCI
	outl(0x11803e40, MEM_STADDR1);

	// burst visibility on 
	outl(inl(MEM_STCFG0) | 0x1000, MEM_STCFG0);

	outl(0x83, MEM_STCFG1);         // ewait enabled, flash timing
	outl(0x33030a10, MEM_STTIME1);   // slower timing for FPGA

#ifdef CONFIG_FB_E1356
	if ((argptr = strstr(argptr, "video=")) == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " video=e1356fb:system:pb1000,mmunalign:1");
	}
#endif // CONFIG_FB_E1356


#ifdef CONFIG_PCI
	outl(0, PCI_BRIDGE_CONFIG); // set extend byte to 0
	outl(0, SDRAM_MBAR);        // set mbar to 0
	outl(0x2, SDRAM_CMD);       // enable memory accesses
	au_sync_delay(1);
#endif

#ifndef CONFIG_SERIAL_NONSTANDARD
	/* don't touch the default serial console */
	outl(0, UART0_ADDR + UART_CLK);
#endif
	outl(0, UART1_ADDR + UART_CLK);
	outl(0, UART2_ADDR + UART_CLK);
	outl(0, UART3_ADDR + UART_CLK);

#ifdef CONFIG_BLK_DEV_IDE
	{
		argptr = prom_getcmdline();
		strcat(argptr, " ide0=noprobe");
	}
	ide_ops = &std_ide_ops;
#endif

	// setup irda clocks
	// aux clock, divide by 2, clock from 2/4 divider
	writel(readl(SYS_CLKSRC) | 0x7, SYS_CLKSRC);
	pin_func = inl(SYS_PINFUNC) & (u32)(~(1<<2)); // clear IRTXD
	outl(pin_func, SYS_PINFUNC);

	while (inl(SYS_COUNTER_CNTRL) & SYS_CNTRL_E0S);
	outl(SYS_CNTRL_E0 | SYS_CNTRL_EN0, SYS_COUNTER_CNTRL);
	au_sync();
	while (inl(SYS_COUNTER_CNTRL) & SYS_CNTRL_T0S);
	outl(0, SYS_TOYTRIM);

	/* Enable Au1000 BCLK switching - note: sed1356 must not use
	 * its BCLK (Au1000 LCLK) for any timings */
	switch (prid & 0x000000FF)
	{
	case 0x00: /* DA */
	case 0x01: /* HA */
	case 0x02: /* HB */
		break;
	default:  /* HC and newer */
		outl(0x00000060, 0xb190003c);
		break;
	}
}
