/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * This file contains the MIPS architecture specific IDE code.
 *
 * Copyright (C) 1994-1996  Linus Torvalds & authors
 */

/*
 *  This file contains the MIPS architecture specific IDE code.
 */

#ifndef __ASM_IDE_H
#define __ASM_IDE_H

#ifdef __KERNEL__

#include <linux/config.h>
#include <asm/io.h>

#ifndef MAX_HWIFS
# ifdef CONFIG_BLK_DEV_IDEPCI
#define MAX_HWIFS	10
# else
#define MAX_HWIFS	6
# endif
#endif

struct ide_ops {
	int (*ide_default_irq)(ide_ioreg_t base);
	ide_ioreg_t (*ide_default_io_base)(int index);
	void (*ide_init_hwif_ports)(hw_regs_t *hw, ide_ioreg_t data_port,
	                            ide_ioreg_t ctrl_port, int *irq);
};

extern struct ide_ops *ide_ops;

static __inline__ int ide_default_irq(ide_ioreg_t base)
{
	return ide_ops->ide_default_irq(base);
}

static __inline__ ide_ioreg_t ide_default_io_base(int index)
{
	return ide_ops->ide_default_io_base(index);
}

static inline void ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port,
                                       ide_ioreg_t ctrl_port, int *irq)
{
	ide_ops->ide_init_hwif_ports(hw, data_port, ctrl_port, irq);
}

static __inline__ void ide_init_default_hwifs(void)
{
#ifndef CONFIG_BLK_DEV_IDEPCI
	hw_regs_t hw;
	int index;

	for(index = 0; index < MAX_HWIFS; index++) {
		ide_init_hwif_ports(&hw, ide_default_io_base(index), 0, NULL);
		hw.irq = ide_default_irq(ide_default_io_base(index));
		ide_register_hw(&hw, NULL);
	}
#endif /* CONFIG_BLK_DEV_IDEPCI */
}

#if defined(CONFIG_SWAP_IO_SPACE) && defined(__MIPSEB__)

#ifdef insl
#undef insl
#endif
#ifdef outsl
#undef outsl
#endif
#ifdef insw
#undef insw
#endif
#ifdef outsw
#undef outsw
#endif

#define insw(p,a,c)							\
do {									\
	unsigned short *ptr = (unsigned short *)(a);			\
	unsigned int i = (c);						\
	while (i--)							\
		*ptr++ = inw(p);					\
} while (0)
#define insl(p,a,c)							\
do {									\
	unsigned long *ptr = (unsigned long *)(a);			\
	unsigned int i = (c);						\
	while (i--)							\
		*ptr++ = inl(p);					\
} while (0)
#define outsw(p,a,c)							\
do {									\
	unsigned short *ptr = (unsigned short *)(a);			\
	unsigned int i = (c);						\
	while (i--)							\
		outw(*ptr++, (p));					\
} while (0)
#define outsl(p,a,c) {							\
	unsigned long *ptr = (unsigned long *)(a);			\
	unsigned int i = (c);						\
	while (i--)							\
		outl(*ptr++, (p));					\
} while (0)

#endif /* defined(CONFIG_SWAP_IO_SPACE) && defined(__MIPSEB__)  */

#endif /* __KERNEL__ */

#endif /* __ASM_IDE_H */
