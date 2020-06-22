/*
 * linux/arch/sh/kernel/io_shmse.c
 *
 * Copyright (C) 2003 Takashi Kusuda <kusuda-takashi@hitachi-ul.co.jp>
 * Based largely on io_se.c
 *
 * I/O routine for SH-Mobile SolutionEngine.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/hitachi_shmse.h>
#include <asm/addrspace.h>

static inline void delay(void)
{
	ctrl_inw(0xac000000);
	ctrl_inw(0xac000000);
}

#define maybebadio(name,port) \
  printk("bad PC-like io %s for port 0x%lx at 0x%08lx\n", \
	#name, (port), (volatile unsigned long) __builtin_return_address(0))

unsigned char shmse_inb(unsigned long port)
{
	if (PXSEG(port)){
		return *(volatile unsigned char *) port; 
	}else{
		maybebadio(inb, port);
		return 0;
	}
}

unsigned char shmse_inb_p(unsigned long port)
{
	unsigned char v;

	if (PXSEG(port)){
		v = *(volatile unsigned char *)port;
	}else{
		maybebadio(inb_p, port);
		return 0;
	}
	delay();
	return v;
}

unsigned short shmse_inw(unsigned long port)
{
	if (PXSEG(port)){
		return *(volatile unsigned short *) port; 
	}else{
		maybebadio(inw, port);
		return 0;
	}
}

unsigned int shmse_inl(unsigned long port)
{
	if (PXSEG(port)){
		return *(volatile unsigned short *) port; 
	}else{
		maybebadio(inl, port);
		return 0;
	}
}

void shmse_outb(unsigned char value, unsigned long port)
{
	if (PXSEG(port)){
		*(volatile unsigned char *)port = value; 
	}else{
		maybebadio(outb, port);
	}
}

void shmse_outb_p(unsigned char value, unsigned long port)
{
	if (PXSEG(port)){
		*(volatile unsigned char *)port = value; 
	}else{
		maybebadio(outb_p, port);
	}
	delay();
}

void shmse_outw(unsigned short value, unsigned long port)
{
	if (PXSEG(port)){
		*(volatile unsigned short *)port = value; 
	}else{
		maybebadio(outw, port);
	}
}

void shmse_outl(unsigned int value, unsigned long port)
{
	if (PXSEG(port)){
		*(volatile unsigned long *)port = value; 
	}else{
		maybebadio(outl, port);
	}
}

void shmse_insb(unsigned long port, void *addr, unsigned long count)
{
	unsigned char *p = addr;
	while (count--) *p++ = shmse_inb(port);
}

void shmse_insw(unsigned long port, void *addr, unsigned long count)
{
	unsigned short *p = addr;
	while (count--) *p++ = shmse_inw(port);
}

void shmse_insl(unsigned long port, void *addr, unsigned long count)
{
	maybebadio(insl, port);
}

void shmse_outsb(unsigned long port, const void *addr, unsigned long count)
{
	unsigned char *p = (unsigned char *)addr;
	while (count--) shmse_outb(*p++, port);
}

void shmse_outsw(unsigned long port, const void *addr, unsigned long count)
{
	unsigned short *p = (unsigned short *)addr;
	while (count--) shmse_outw(*p++, port);
}

void shmse_outsl(unsigned long port, const void *addr, unsigned long count)
{
	maybebadio(outsw, port);
}

unsigned char shmse_readb(unsigned long addr)
{
	return *(volatile unsigned char*)addr;
}

unsigned short shmse_readw(unsigned long addr)
{
	return *(volatile unsigned short*)addr;
}

unsigned int shmse_readl(unsigned long addr)
{
	return *(volatile unsigned long*)addr;
}

void shmse_writeb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char*)addr = b;
}

void shmse_writew(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short*)addr = b;
}

void shmse_writel(unsigned int b, unsigned long addr)
{
        *(volatile unsigned long*)addr = b;
}
