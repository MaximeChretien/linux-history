/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * MIPS boards specific PCI support.
 *
 */
#include <linux/config.h>

#ifdef CONFIG_PCI

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/mips-boards/generic.h>
#include <asm/gt64120.h>
#include <asm/mips-boards/bonito64.h>
#ifdef CONFIG_MIPS_MALTA
#include <asm/mips-boards/malta.h>
#endif
#include <asm/mips-boards/msc01_pci.h>

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1

/*
 *  PCI configuration cycle AD bus definition
 */
/* Type 0 */
#define PCI_CFG_TYPE0_REG_SHF           0
#define PCI_CFG_TYPE0_FUNC_SHF          8

/* Type 1 */
#define PCI_CFG_TYPE1_REG_SHF           0
#define PCI_CFG_TYPE1_FUNC_SHF          8
#define PCI_CFG_TYPE1_DEV_SHF           11
#define PCI_CFG_TYPE1_BUS_SHF           16

static int
mips_pcibios_config_access(unsigned char access_type, struct pci_dev *dev,
                           unsigned char where, u32 *data)
{
	unsigned char bus = dev->bus->number;
	unsigned char dev_fn = dev->devfn;
	unsigned char type;
	u32 intr, dummy;
	u64 pci_addr;

	switch(mips_revision_corid) {
	case MIPS_REVISION_CORID_QED_RM5261:
	case MIPS_REVISION_CORID_CORE_LV:
	case MIPS_REVISION_CORID_CORE_FPGA:
	        /* Galileo GT64120 system controller. */

		if ((bus == 0) && (dev_fn >= PCI_DEVFN(31,0)))
			return -1; /* Because of a bug in the galileo (for slot 31). */

		/* Clear cause register bits */
		GT_READ(GT_INTRCAUSE_OFS, intr);
		GT_WRITE(GT_INTRCAUSE_OFS, intr &
			 ~(GT_INTRCAUSE_MASABORT0_BIT |
			   GT_INTRCAUSE_TARABORT0_BIT));

		/* Setup address */
		GT_WRITE(GT_PCI0_CFGADDR_OFS,
			 (bus         << GT_PCI0_CFGADDR_BUSNUM_SHF)   |
			 (dev_fn      << GT_PCI0_CFGADDR_FUNCTNUM_SHF) |
			 ((where / 4) << GT_PCI0_CFGADDR_REGNUM_SHF)   |
			 GT_PCI0_CFGADDR_CONFIGEN_BIT);

		if (access_type == PCI_ACCESS_WRITE) {
			if (bus == 0 && dev_fn == 0) {
				/*
				 * The Galileo system controller is acting
				 * differently than other devices.
				 */
				GT_WRITE(GT_PCI0_CFGDATA_OFS, *data);
			} else {
				GT_PCI_WRITE(GT_PCI0_CFGDATA_OFS, *data);
			}
		} else {
			if (bus == 0 && dev_fn == 0) {
				/*
				 * The Galileo system controller is acting
				 * differently than other devices.
				 */
				GT_READ(GT_PCI0_CFGDATA_OFS, *data);
			} else {
				GT_PCI_READ(GT_PCI0_CFGDATA_OFS, *data);
			}
		}

		/* Check for master or target abort */
		GT_READ(GT_INTRCAUSE_OFS, intr);

		if (intr & (GT_INTRCAUSE_MASABORT0_BIT |
			    GT_INTRCAUSE_TARABORT0_BIT))
		{
			/* Error occured */

			/* Clear bits */
			GT_READ(GT_INTRCAUSE_OFS, intr);
			GT_WRITE(GT_INTRCAUSE_OFS, intr &
				 ~(GT_INTRCAUSE_MASABORT0_BIT |
				   GT_INTRCAUSE_TARABORT0_BIT));

			return -1;
		}

		break;

	case MIPS_REVISION_CORID_BONITO64:
	case MIPS_REVISION_CORID_CORE_20K:
	        /* Algorithmics Bonito64 system controller. */

	        if ((bus == 0) && (PCI_SLOT(dev_fn) == 0)) {
		        return -1;
		}

		/* Clear cause register bits */
		BONITO_PCICMD |= (BONITO_PCICMD_MABORT_CLR |
				  BONITO_PCICMD_MTABORT_CLR);

		/*
		 * Setup pattern to be used as PCI "address" for
		 * Type 0 cycle
		 */
		if (bus == 0) {
		        /* IDSEL */
		        pci_addr = (u64)1 << (PCI_SLOT(dev_fn) + 10);
		} else {
		        /* Bus number */
		        pci_addr = bus << PCI_CFG_TYPE1_BUS_SHF;

			/* Device number */
			pci_addr |= PCI_SLOT(dev_fn) << PCI_CFG_TYPE1_DEV_SHF;
		}

		/* Function (same for Type 0/1) */
		pci_addr |= PCI_FUNC(dev_fn) << PCI_CFG_TYPE0_FUNC_SHF;

		/* Register number (same for Type 0/1) */
		pci_addr |= (where & ~0x3) << PCI_CFG_TYPE0_REG_SHF;

		if (bus == 0) {
		        /* Type 0 */
		        BONITO_PCIMAP_CFG = pci_addr >> 16;
		} else {
		        /* Type 1 */
		        BONITO_PCIMAP_CFG = (pci_addr >> 16) | 0x10000;
		}

		/* Flush Bonito register block */
		dummy = BONITO_PCIMAP_CFG;
		__asm__ __volatile__(
				     ".set\tnoreorder\n\t"
				     ".set\tnoat\n\t"
				     "sync\n\t"
				     ".set\tat\n\t"
				     ".set\treorder");

		/* Perform access */
		if (access_type == PCI_ACCESS_WRITE) {
		        *(volatile u32 *)(KSEG1ADDR(BONITO_PCICFG_BASE +
					  (pci_addr & 0xffff))) = *(u32 *)data;

			/* Wait till done */
			while (BONITO_PCIMSTAT & 0xF)
			        ;
		} else {
		        *(u32 *)data =
			  *(volatile u32 *)(KSEG1ADDR(BONITO_PCICFG_BASE +
					    (pci_addr & 0xffff)));
		}

		/* Detect Master/Target abort */
		if (BONITO_PCICMD & (BONITO_PCICMD_MABORT_CLR |
				     BONITO_PCICMD_MTABORT_CLR) )
		{
		        /* Error occurred */

		        /* Clear bits */
		        BONITO_PCICMD |= (BONITO_PCICMD_MABORT_CLR |
					  BONITO_PCICMD_MTABORT_CLR);

			return -1;
		}
	        break;

	case MIPS_REVISION_CORID_CORE_MSC:
	        /* MIPS system controller. */

	        if ((bus == 0) && (PCI_SLOT(dev_fn) == 0)) {
		        return -1;
		}

		/* Clear status register bits. */
		MSC_WRITE(MSC01_PCI_INTSTAT,
			  (MSC01_PCI_INTCFG_MA_BIT |
			   MSC01_PCI_INTCFG_TA_BIT));

		/* Setup address */
		if (bus == 0)
			type = 0;  /* Type 0 */
		else
			type = 1;  /* Type 1 */

		MSC_WRITE(MSC01_PCI_CFGADDR,
			  ((bus              << MSC01_PCI_CFGADDR_BNUM_SHF) |
			   (PCI_SLOT(dev_fn) << MSC01_PCI_CFGADDR_DNUM_SHF) |
			   (PCI_FUNC(dev_fn) << MSC01_PCI_CFGADDR_FNUM_SHF) |
			   ((where /4 )      << MSC01_PCI_CFGADDR_RNUM_SHF) |
			   (type)));

		/* Perform access */
		if (access_type == PCI_ACCESS_WRITE) {
		        MSC_WRITE(MSC01_PCI_CFGDATA, *data);
		} else {
			MSC_READ(MSC01_PCI_CFGDATA, *data);
		}

		/* Detect Master/Target abort */
		MSC_READ(MSC01_PCI_INTSTAT, intr);
		if (intr & (MSC01_PCI_INTCFG_MA_BIT |
			    MSC01_PCI_INTCFG_TA_BIT))
		{
		        /* Error occurred */

		        /* Clear bits */
			MSC_READ(MSC01_PCI_INTSTAT, intr);
			MSC_WRITE(MSC01_PCI_INTSTAT,
				  (MSC01_PCI_INTCFG_MA_BIT |
				   MSC01_PCI_INTCFG_TA_BIT));

			return -1;
		}
	        break;
	default:
	        printk("Unknown Core card, don't know the system controller.\n");
		return -1;
	}

	return 0;
}


/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int
mips_pcibios_read_config_byte (struct pci_dev *dev, int where, u8 *val)
{
	u32 data = 0;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = (data >> ((where & 3) << 3)) & 0xff;

	return PCIBIOS_SUCCESSFUL;
}


static int
mips_pcibios_read_config_word (struct pci_dev *dev, int where, u16 *val)
{
	u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return -1;

	*val = (data >> ((where & 3) << 3)) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int
mips_pcibios_read_config_dword (struct pci_dev *dev, int where, u32 *val)
{
	u32 data = 0;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = data;

	return PCIBIOS_SUCCESSFUL;
}


static int
mips_pcibios_write_config_byte (struct pci_dev *dev, int where, u8 val)
{
	u32 data = 0;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	data = (data & ~(0xff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (mips_pcibios_config_access(PCI_ACCESS_WRITE, dev, where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

static int
mips_pcibios_write_config_word (struct pci_dev *dev, int where, u16 val)
{
        u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

        if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return -1;

	data = (data & ~(0xffff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (mips_pcibios_config_access(PCI_ACCESS_WRITE, dev, where, &data))
	       return -1;


	return PCIBIOS_SUCCESSFUL;
}

static int
mips_pcibios_write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (mips_pcibios_config_access(PCI_ACCESS_WRITE, dev, where, &val))
	       return -1;

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops mips_pci_ops = {
	mips_pcibios_read_config_byte,
        mips_pcibios_read_config_word,
	mips_pcibios_read_config_dword,
	mips_pcibios_write_config_byte,
	mips_pcibios_write_config_word,
	mips_pcibios_write_config_dword
};

int mips_pcibios_iack(void)
{
	int irq;
        u32 dummy;

	/*
	 * Determine highest priority pending interrupt by performing
	 * a PCI Interrupt Acknowledge cycle.
	 */
	switch(mips_revision_corid) {
	case MIPS_REVISION_CORID_QED_RM5261:
	case MIPS_REVISION_CORID_CORE_LV:
	case MIPS_REVISION_CORID_CORE_FPGA:
	case MIPS_REVISION_CORID_CORE_MSC:
		if (mips_revision_corid == MIPS_REVISION_CORID_CORE_MSC)
			MSC_READ(MSC01_PCI_IACK, irq);
		else
			GT_READ(GT_PCI0_IACK_OFS, irq);
		irq &= 0xff;
		break;
	case MIPS_REVISION_CORID_BONITO64:
	case MIPS_REVISION_CORID_CORE_20K:
		/* The following will generate a PCI IACK cycle on the
		 * Bonito controller. It's a little bit kludgy, but it
		 * was the easiest way to implement it in hardware at
		 * the given time.
		 */
		BONITO_PCIMAP_CFG = 0x20000;

		/* Flush Bonito register block */
		dummy = BONITO_PCIMAP_CFG;
		__asm__ __volatile__(
			".set\tnoreorder\n\t"
			".set\tnoat\n\t"
			"sync\n\t"
			".set\tat\n\t"
			".set\treorder");

		irq = *(volatile u32 *)(KSEG1ADDR(BONITO_PCICFG_BASE));
		irq &= 0xff;
		BONITO_PCIMAP_CFG = 0;
		break;
	default:
	        printk("Unknown Core card, don't know the system controller.\n");
		return -1;
	}
	return irq;
}

void __init pcibios_init(void)
{
#ifdef CONFIG_MIPS_MALTA
	struct pci_dev *pdev;
	unsigned char reg_val;
#endif

	printk("PCI: Probing PCI hardware on host bus 0.\n");
	pci_scan_bus(0, &mips_pci_ops, NULL);

	switch(mips_revision_corid) {
	case MIPS_REVISION_CORID_QED_RM5261:
	case MIPS_REVISION_CORID_CORE_LV:
	case MIPS_REVISION_CORID_CORE_FPGA:
		/*
		 * Due to a bug in the Galileo system controller, we need
		 * to setup the PCI BAR for the Galileo internal registers.
		 * This should be done in the bios/bootprom and will be
		 * fixed in a later revision of YAMON (the MIPS boards
		 * boot prom).
		 */
		GT_WRITE(GT_PCI0_CFGADDR_OFS,
			 (0 << GT_PCI0_CFGADDR_BUSNUM_SHF) | /* Local bus */
			 (0 << GT_PCI0_CFGADDR_DEVNUM_SHF) | /* GT64120 dev */
			 (0 << GT_PCI0_CFGADDR_FUNCTNUM_SHF) | /* Function 0*/
			 ((0x20/4) << GT_PCI0_CFGADDR_REGNUM_SHF) | /* BAR 4*/
			 GT_PCI0_CFGADDR_CONFIGEN_BIT );

		/* Perform the write */
		GT_WRITE( GT_PCI0_CFGDATA_OFS, PHYSADDR(MIPS_GT_BASE));
		break;
	}

#ifdef CONFIG_MIPS_MALTA
	pci_for_each_dev(pdev) {
		if ((pdev->vendor == PCI_VENDOR_ID_INTEL)
		    && (pdev->device == PCI_DEVICE_ID_INTEL_82371AB)
		    && (PCI_SLOT(pdev->devfn) == 0x0a)) {
			/*
			 * IDE Decode enable.
			 */
			pci_read_config_byte(pdev, 0x41, &reg_val);
        		pci_write_config_byte(pdev, 0x41, reg_val | 0x80);
			pci_read_config_byte(pdev, 0x43, &reg_val);
        		pci_write_config_byte(pdev, 0x43, reg_val | 0x80);
		}

		if ((pdev->vendor == PCI_VENDOR_ID_INTEL)
		    && (pdev->device == PCI_DEVICE_ID_INTEL_82371AB_0)
		    && (PCI_SLOT(pdev->devfn) == 0x0a)) {
			/*
			 * Set top of main memory accessible by ISA or DMA
			 * devices to 16 Mb.
			 */
			pci_read_config_byte(pdev, 0x69, &reg_val);
			pci_write_config_byte(pdev, 0x69, reg_val | 0xf0);
		}
	}

	/*
	 * Activate Floppy Controller in the SMSC FDC37M817 Super I/O
	 * Controller.
	 * This should be done in the bios/bootprom and will be fixed in
         * a later revision of YAMON (the MIPS boards boot prom).
	 */
	/* Entering config state. */
	SMSC_WRITE(SMSC_CONFIG_ENTER, SMSC_CONFIG_REG);

	/* Activate floppy controller. */
	SMSC_WRITE(SMSC_CONFIG_DEVNUM, SMSC_CONFIG_REG);
	SMSC_WRITE(SMSC_CONFIG_DEVNUM_FLOPPY, SMSC_DATA_REG);
	SMSC_WRITE(SMSC_CONFIG_ACTIVATE, SMSC_CONFIG_REG);
	SMSC_WRITE(SMSC_CONFIG_ACTIVATE_ENABLE, SMSC_DATA_REG);

	/* Exit config state. */
	SMSC_WRITE(SMSC_CONFIG_EXIT, SMSC_CONFIG_REG);
#endif
}

int __init
pcibios_enable_device(struct pci_dev *dev)
{
	/* Not needed, since we enable all devices at startup.  */
	return 0;
}

void __init
pcibios_align_resource(void *data, struct resource *res, unsigned long size,
		       unsigned long align)
{
}

char * __init
pcibios_setup(char *str)
{
	/* Nothing to do for now.  */

	return str;
}

struct pci_fixup pcibios_fixups[] = {
	{ 0 }
};

void __init
pcibios_update_resource(struct pci_dev *dev, struct resource *root,
                        struct resource *res, int resource)
{
	unsigned long where, size;
	u32 reg;

	where = PCI_BASE_ADDRESS_0 + (resource * 4);
	size = res->end - res->start;
	pci_read_config_dword(dev, where, &reg);
	reg = (reg & size) | (((u32)(res->start - root->start)) & ~size);
	pci_write_config_dword(dev, where, reg);
}

/*
 *  Called after each bus is probed, but before its children
 *  are examined.
 */
void __init pcibios_fixup_bus(struct pci_bus *b)
{
	pci_read_bridge_bases(b);
}

unsigned __init int pcibios_assign_all_busses(void)
{
	return 1;
}

#endif /* CONFIG_PCI */
