/*
 * arch/ppc/common/misc-simple.c
 *
 * Misc. bootloader code for many machines.  This assumes you have are using
 * a 6xx/7xx/74xx CPU in your machine.  This assumes the chunk of memory
 * below 8MB is free.  Finally, it assumes you have a NS16550-style uart for 
 * your serial console.  If a machine meets these requirements, it can quite
 * likely use this code during boot.
 * 
 * Author: Matt Porter <mporter@mvista.com>
 * Derived from arch/ppc/boot/prep/misc.c
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/types.h>
#include <linux/elf.h>
#include <linux/config.h>

#include <asm/page.h>
#include <asm/processor.h>
#include <asm/mmu.h>

#include "nonstdio.h"
#include "zlib.h"

unsigned long com_port;

char *avail_ram;
char *end_avail;
extern char _end[];

#ifdef CONFIG_CMDLINE
#define CMDLINE CONFIG_CMDLINE
#else
#define CMDLINE ""
#endif
char cmd_preset[] = CMDLINE;
char cmd_buf[256];
char *cmd_line = cmd_buf;

unsigned long initrd_start = 0, initrd_end = 0;

/* These values must be variables.  If not, the compiler optimizer
 * will remove some code, causing the size of the code to vary
 * when these values are zero.  This is bad because we first
 * compile with these zero to determine the size and offsets
 * in an image, than compile again with these set to the proper
 * discovered value.
 */
unsigned int initrd_offset, initrd_size;
char *zimage_start;
int zimage_size;

extern void gunzip(void *, int, unsigned char *, int *);
extern unsigned long serial_init(int chan);

void
decompress_kernel(unsigned long load_addr, int num_words, unsigned long cksum)
{

	int timer = 0;
	extern unsigned long start;
	char *cp, ch;

	com_port = serial_init(0);

	/* assume the chunk below 8M is free */
	end_avail = (char *)0x00800000;

	/*
	 * Reveal where we were loaded at and where we
	 * were relocated to.
	 */
	puts("loaded at:     "); puthex(load_addr);
	puts(" "); puthex((unsigned long)(load_addr + (4*num_words)));
	puts("\n");
	if ( (unsigned long)load_addr != (unsigned long)&start )
	{
		puts("relocated to:  "); puthex((unsigned long)&start);
		puts(" ");
		puthex((unsigned long)((unsigned long)&start + (4*num_words)));
		puts("\n");
	}

	/* we have to subtract 0x10000 here to correct for objdump including
	   the size of the elf header which we strip -- Cort */
	zimage_start = (char *)(load_addr - 0x10000 + ZIMAGE_OFFSET);
	zimage_size = ZIMAGE_SIZE;
	initrd_offset = INITRD_OFFSET;
	initrd_size = INITRD_SIZE;

	if ( initrd_offset )
		initrd_start = load_addr - 0x10000 + initrd_offset;
	else
		initrd_start = 0;
	initrd_end = initrd_size + initrd_start;

	/* Relocate the zImage */
	avail_ram = (char *)PAGE_ALIGN((unsigned long)_end);
	puts("zimage at:     "); puthex((unsigned long)zimage_start);
	puts(" "); puthex((unsigned long)(zimage_size+zimage_start));
	puts("\n");
	memcpy( (void *)avail_ram, (void *)zimage_start, zimage_size );
	zimage_start = (char *)avail_ram;
	puts("relocated to:  "); puthex((unsigned long)zimage_start);
	puts(" ");
	puthex((unsigned long)zimage_size+(unsigned long)zimage_start);
	puts("\n");

	if ( initrd_start ) {
		puts("initrd at:     "); puthex(initrd_start);
		puts(" "); puthex(initrd_end); puts("\n");
		/* relocate initrd */
		avail_ram = (char *)PAGE_ALIGN((unsigned long)zimage_size + 
				(unsigned long)zimage_start);
		memcpy( (void *)avail_ram, (void *)initrd_start, initrd_size );
		initrd_start = (unsigned long)avail_ram;
		initrd_end = initrd_start + initrd_size;
		puts("relocated to:  "); puthex(initrd_start);
		puts(" "); puthex(initrd_end); puts("\n");
	}

	avail_ram = (char *)0x00400000;
	end_avail = (char *)0x00800000;
	puts("avail ram:     "); puthex((unsigned long)avail_ram); puts(" ");
	puthex((unsigned long)end_avail); puts("\n");

	/* Display standard Linux/PPC boot prompt for kernel args */
	puts("\nLinux/PPC load: ");
	cp = cmd_line;
	memcpy (cmd_line, cmd_preset, sizeof(cmd_preset));
	while ( *cp ) putc(*cp++);
	while (timer++ < 5*1000) {
		if (tstc()) {
			while ((ch = getc()) != '\n' && ch != '\r') {
				/* Test for backspace/delete */
				if (ch == '\b' || ch == '\177') {
					if (cp != cmd_line) {
						cp--;
						puts("\b \b");
					}
				/* Test for ^x/^u (and wipe the line) */
				} else if (ch == '\030' || ch == '\025') {
					while (cp != cmd_line) {
						cp--;
						puts("\b \b");
					}
				} else {
					*cp++ = ch;
					putc(ch);
				}
			}
			break;  /* Exit 'timer' loop */
		}
		udelay(1000);  /* 1 msec */
	}
	*cp = 0;
	puts("\n");

	/* mappings on early boot can only handle 16M */
	if ( (u32)(cmd_line) > (16<<20))
		puts("cmd_line located > 16M\n");

	puts("Uncompressing Linux...");

	gunzip(0, 0x400000, zimage_start, &zimage_size);
	puts("done.\n");

	puts("Now booting the kernel\n");
}
