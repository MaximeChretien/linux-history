#ifndef _ASM_M32R_IO_H
#define _ASM_M32R_IO_H

/* $Id$ */

#include <asm/page.h>  /* __va */

#ifdef __KERNEL__

#define IO_SPACE_LIMIT  0xFFFFFFFF

/**
 *	virt_to_phys	-	map virtual addresses to physical
 *	@address: address to remap
 *
 *	The returned physical address is the physical (CPU) mapping for
 *	the memory address given. It is only valid to use this function on
 *	addresses directly mapped or allocated via kmalloc.
 *
 *	This function does not give bus mappings for DMA transfers. In
 *	almost all conceivable cases a device driver should not be using
 *	this function
 */

static __inline__ unsigned long virt_to_phys(volatile void * address)
{
	return __pa(address);
}

/**
 *	phys_to_virt	-	map physical address to virtual
 *	@address: address to remap
 *
 *	The returned virtual address is a current CPU mapping for
 *	the memory address given. It is only valid to use this function on
 *	addresses that have a kernel mapping
 *
 *	This function does not handle bus mappings for DMA transfers. In
 *	almost all conceivable cases a device driver should not be using
 *	this function
 */

static __inline__ void *phys_to_virt(unsigned long address)
{
	return __va(address);
}

extern void * __ioremap(unsigned long offset, unsigned long size, unsigned long flags);

/**
 *	ioremap		-	map bus memory into CPU space
 *	@offset:	bus address of the memory
 *	@size:		size of the resource to map
 *
 *	ioremap performs a platform specific sequence of operations to
 *	make bus memory CPU accessible via the readb/readw/readl/writeb/
 *	writew/writel functions and the other mmio helpers. The returned
 *	address is not guaranteed to be usable directly as a virtual
 *	address.
 */

static __inline__ void * ioremap(unsigned long offset, unsigned long size)
{
	return __ioremap(offset, size, 0);
}

extern void iounmap(void *addr);
#define ioremap_nocache(off,size) ioremap(off,size)

/*
 * IO bus memory addresses are also 1:1 with the physical address
 */
#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)
#define page_to_bus	page_to_phys
#define virt_to_bus	virt_to_phys

extern unsigned char _inb(unsigned long);
extern unsigned short _inw(unsigned long);
extern unsigned long _inl(unsigned long);
extern unsigned char _inb_p(unsigned long);
extern unsigned short _inw_p(unsigned long);
extern unsigned long _inl_p(unsigned long);
extern void _outb(unsigned char, unsigned long);
extern void _outw(unsigned short, unsigned long);
extern void _outl(unsigned long, unsigned long);
extern void _outb_p(unsigned char, unsigned long);
extern void _outw_p(unsigned short, unsigned long);
extern void _outl_p(unsigned long, unsigned long);
extern void _insb(unsigned int, void *, unsigned long);
extern void _insw(unsigned int, void *, unsigned long);
extern void _insl(unsigned int, void *, unsigned long);
extern void _outsb(unsigned int, const void *, unsigned long);
extern void _outsw(unsigned int, const void *, unsigned long);
extern void _outsl(unsigned int, const void *, unsigned long);

static inline unsigned char _readb(unsigned long addr)
{
	return *(volatile unsigned char *)addr;
}

static inline unsigned short _readw(unsigned long addr)
{
	return *(volatile unsigned short *)addr;
}

static inline unsigned long _readl(unsigned long addr)
{
	return *(volatile unsigned long *)addr;
}

static inline void _writeb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char *)addr = b;
}

static inline void _writew(unsigned short w, unsigned long addr)
{
	*(volatile unsigned short *)addr = w;
}

static inline void _writel(unsigned long l, unsigned long addr)
{
	*(volatile unsigned long *)addr = l;
}

#define inb     _inb
#define inw     _inw
#define inl     _inl
#define outb    _outb
#define outw    _outw
#define outl    _outl

#define inb_p   _inb_p
#define inw_p   _inw_p
#define inl_p   _inl_p
#define outb_p  _outb_p
#define outw_p  _outw_p
#define outl_p  _outl_p

#define insb    _insb
#define insw    _insw
#define insl    _insl
#define outsb   _outsb
#define outsw   _outsw
#define outsl   _outsl

#define readb(addr)   _readb((unsigned long)(addr))
#define readw(addr)   _readw((unsigned long)(addr))
#define readl(addr)   _readl((unsigned long)(addr))
#define __raw_readb readb
#define __raw_readw readw
#define __raw_readl readl

#define writeb(val, addr)  _writeb((val), (unsigned long)(addr))
#define writew(val, addr)  _writew((val), (unsigned long)(addr))
#define writel(val, addr)  _writel((val), (unsigned long)(addr))
#define __raw_writeb writeb
#define __raw_writew writew
#define __raw_writel writel

#define flush_write_buffers() do { } while (0)  /* M32R_FIXME */

/**
 *	isa_check_signature		-	find BIOS signatures
 *	@io_addr: mmio address to check
 *	@signature:  signature block
 *	@length: length of signature
 *
 *	Perform a signature comparison with the ISA mmio address io_addr.
 *	Returns 1 on a match.
 *
 *	This function is deprecated. New drivers should use ioremap and
 *	check_signature.
 */

static inline int isa_check_signature(unsigned long io_addr,
        const unsigned char *signature, int length)
{
        int retval = 0;
#if 0
printk("isa_check_signature\n");
        do {
                if (isa_readb(io_addr) != *signature)
                        goto out;
                io_addr++;
                signature++;
                length--;
        } while (length);
        retval = 1;
out:
#endif
        return retval;
}

#define memset_io(a, b, c)	memset((void *)(a), (b), (c))
#define memcpy_fromio(a, b, c)	memcpy((a), (void *)(b), (c))
#define memcpy_toio(a, b, c)	memcpy((void *)(a), (b), (c))

#endif  /* __KERNEL__ */

#endif  /* _ASM_M32R_IO_H */