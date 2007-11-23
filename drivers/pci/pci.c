/*
 *	$Id: pci.c,v 1.79 1998/04/17 16:25:24 mj Exp $
 *
 *	PCI Bus Services
 *
 *	Copyright 1993 -- 1998 Drew Eckhardt, Frederic Potter,
 *	David Mosberger-Tang, Martin Mares
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/malloc.h>

#include <asm/page.h>

#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

struct pci_bus pci_root;
struct pci_dev *pci_devices = NULL;
static struct pci_dev **pci_last_dev_p = &pci_devices;
static int pci_reverse __initdata = 0;

struct pci_dev *
pci_find_slot(unsigned int bus, unsigned int devfn)
{
	struct pci_dev *dev;

	for(dev=pci_devices; dev; dev=dev->next)
		if (dev->bus->number == bus && dev->devfn == devfn)
			break;
	return dev;
}


struct pci_dev *
pci_find_device(unsigned int vendor, unsigned int device, struct pci_dev *from)
{
	if (!from)
		from = pci_devices;
	else
		from = from->next;
	while (from && (from->vendor != vendor || from->device != device))
		from = from->next;
	return from;
}


struct pci_dev *
pci_find_class(unsigned int class, struct pci_dev *from)
{
	if (!from)
		from = pci_devices;
	else
		from = from->next;
	while (from && from->class != class)
		from = from->next;
	return from;
}


void
pci_set_master(struct pci_dev *dev)
{
	unsigned short cmd;
	unsigned char lat;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (! (cmd & PCI_COMMAND_MASTER)) {
		printk("PCI: Enabling bus mastering for device %02x:%02x\n",
			dev->bus->number, dev->devfn);
		cmd |= PCI_COMMAND_MASTER;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
	if (lat < 16) {
		printk("PCI: Increasing latency timer of device %02x:%02x to 64\n",
			dev->bus->number, dev->devfn);
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);
	}
}


__initfunc(unsigned int pci_scan_bus(struct pci_bus *bus))
{
	unsigned int devfn, l, max, class;
	unsigned char cmd, irq, tmp, hdr_type, is_multi = 0;
	struct pci_dev *dev;
	struct pci_bus *child;
	int reg;

	DBG("pci_scan_bus for bus %d\n", bus->number);
	max = bus->secondary;
	for (devfn = 0; devfn < 0xff; ++devfn) {
		if (PCI_FUNC(devfn) && !is_multi) {
			/* not a multi-function device */
			continue;
		}
		pcibios_read_config_byte(bus->number, devfn, PCI_HEADER_TYPE, &hdr_type);
		if (!PCI_FUNC(devfn))
			is_multi = hdr_type & 0x80;

		pcibios_read_config_dword(bus->number, devfn, PCI_VENDOR_ID, &l);
		/* some broken boards return 0 if a slot is empty: */
		if (l == 0xffffffff || l == 0x00000000) {
			hdr_type = 0;
			continue;
		}

		dev = kmalloc(sizeof(*dev), GFP_ATOMIC);
		memset(dev, 0, sizeof(*dev));
		dev->bus = bus;
		dev->devfn  = devfn;
		dev->vendor = l & 0xffff;
		dev->device = (l >> 16) & 0xffff;

		/* non-destructively determine if device can be a master: */
		pcibios_read_config_byte(bus->number, devfn, PCI_COMMAND, &cmd);
		pcibios_write_config_byte(bus->number, devfn, PCI_COMMAND, cmd | PCI_COMMAND_MASTER);
		pcibios_read_config_byte(bus->number, devfn, PCI_COMMAND, &tmp);
		dev->master = ((tmp & PCI_COMMAND_MASTER) != 0);
		pcibios_write_config_byte(bus->number, devfn, PCI_COMMAND, cmd);

		pcibios_read_config_dword(bus->number, devfn, PCI_CLASS_REVISION, &class);
		class >>= 8;				    /* upper 3 bytes */
		dev->class = class;
		dev->hdr_type = hdr_type;

		switch (hdr_type & 0x7f) {		    /* header type */
		case PCI_HEADER_TYPE_NORMAL:		    /* standard header */
			if (class >> 8 == PCI_CLASS_BRIDGE_PCI)
				goto bad;
			/*
			 * If the card generates interrupts, read IRQ number
			 * (some architectures change it during pcibios_fixup())
			 */
			pcibios_read_config_byte(bus->number, dev->devfn, PCI_INTERRUPT_PIN, &irq);
			if (irq)
				pcibios_read_config_byte(bus->number, dev->devfn, PCI_INTERRUPT_LINE, &irq);
			dev->irq = irq;
			/*
			 * read base address registers, again pcibios_fixup() can
			 * tweak these
			 */
			for (reg = 0; reg < 6; reg++) {
				pcibios_read_config_dword(bus->number, devfn, PCI_BASE_ADDRESS_0 + (reg << 2), &l);
				dev->base_address[reg] = (l == 0xffffffff) ? 0 : l;
			}
			pcibios_read_config_dword(bus->number, devfn, PCI_ROM_ADDRESS, &l);
			dev->rom_address = (l == 0xffffffff) ? 0 : l;
			break;
		case PCI_HEADER_TYPE_BRIDGE:		    /* bridge header */
			if (class >> 8 != PCI_CLASS_BRIDGE_PCI)
				goto bad;
			for (reg = 0; reg < 2; reg++) {
				pcibios_read_config_dword(bus->number, devfn, PCI_BASE_ADDRESS_0 + (reg << 2), &l);
				dev->base_address[reg] = (l == 0xffffffff) ? 0 : l;
			}
			pcibios_read_config_dword(bus->number, devfn, PCI_ROM_ADDRESS1, &l);
			dev->rom_address = (l == 0xffffffff) ? 0 : l;
			break;
		case PCI_HEADER_TYPE_CARDBUS:		    /* CardBus bridge header */
			if (class >> 16 != PCI_BASE_CLASS_BRIDGE)
				goto bad;
			for (reg = 0; reg < 2; reg++) {
				pcibios_read_config_dword(bus->number, devfn, PCI_CB_MEMORY_BASE_0 + (reg << 3), &l);
				dev->base_address[reg] = (l == 0xffffffff) ? 0 : l;
			}
			break;
		default:				    /* unknown header */
		bad:
			printk(KERN_ERR "PCI: %02x:%02x [%04x/%04x/%06x] has unknown header type %02x, ignoring.\n",
			       bus->number, dev->devfn, dev->vendor, dev->device, class, hdr_type);
			continue;
		}

		DBG("PCI: %02x:%02x [%04x/%04x]\n", bus->number, dev->devfn, dev->vendor, dev->device);

		/*
		 * Put it into the global PCI device chain. It's used to
		 * find devices once everything is set up.
		 */
		if (!pci_reverse) {
			*pci_last_dev_p = dev;
			pci_last_dev_p = &dev->next;
		} else {
			dev->next = pci_devices;
			pci_devices = dev;
		}

		/*
		 * Now insert it into the list of devices held
		 * by the parent bus.
		 */
		dev->sibling = bus->devices;
		bus->devices = dev;

#if 0
		/*
		 * Setting of latency timer in case it was less than 32 was
		 * a great idea, but it confused several broken devices. Grrr.
		 */
		pcibios_read_config_byte(bus->number, dev->devfn, PCI_LATENCY_TIMER, &tmp);
		if (tmp < 32)
			pcibios_write_config_byte(bus->number, dev->devfn, PCI_LATENCY_TIMER, 32);
#endif

		/*
		 * If it's a bridge, scan the bus behind it.
		 */
		if (class >> 8 == PCI_CLASS_BRIDGE_PCI) {
			unsigned int buses;
			unsigned short cr;

			/*
			 * Insert it into the tree of buses.
			 */
			child = kmalloc(sizeof(*child), GFP_ATOMIC);
			memset(child, 0, sizeof(*child));
			child->next = bus->children;
			bus->children = child;
			child->self = dev;
			child->parent = bus;

			/*
			 * Set up the primary, secondary and subordinate
			 * bus numbers.
			 */
			child->number = child->secondary = ++max;
			child->primary = bus->secondary;
			child->subordinate = 0xff;
			/*
			 * Clear all status bits and turn off memory,
			 * I/O and master enables.
			 */
			pcibios_read_config_word(bus->number, devfn, PCI_COMMAND, &cr);
			pcibios_write_config_word(bus->number, devfn, PCI_COMMAND, 0x0000);
			pcibios_write_config_word(bus->number, devfn, PCI_STATUS, 0xffff);
			/*
			 * Read the existing primary/secondary/subordinate bus
			 * number configuration to determine if the PCI bridge
			 * has already been configured by the system.  If so,
			 * do not modify the configuration, merely note it.
			 */
			pcibios_read_config_dword(bus->number, devfn, PCI_PRIMARY_BUS, &buses);
			if ((buses & 0xFFFFFF) != 0)
			  {
			    unsigned int cmax;

			    child->primary = buses & 0xFF;
			    child->secondary = (buses >> 8) & 0xFF;
			    child->subordinate = (buses >> 16) & 0xFF;
			    child->number = child->secondary;
			    cmax = pci_scan_bus(child);
			    if (cmax > max) max = cmax;
			  }
			else
			  {
			    /*
			     * Configure the bus numbers for this bridge:
			     */
			    buses &= 0xff000000;
			    buses |=
			      (((unsigned int)(child->primary)     <<  0) |
			       ((unsigned int)(child->secondary)   <<  8) |
			       ((unsigned int)(child->subordinate) << 16));
			    pcibios_write_config_dword(bus->number, devfn, PCI_PRIMARY_BUS, buses);
			    /*
			     * Now we can scan all subordinate buses:
			     */
			    max = pci_scan_bus(child);
			    /*
			     * Set the subordinate bus number to its real
			     * value:
			     */
			    child->subordinate = max;
			    buses = (buses & 0xff00ffff)
			      | ((unsigned int)(child->subordinate) << 16);
			    pcibios_write_config_dword(bus->number, devfn, PCI_PRIMARY_BUS, buses);
			  }
			pcibios_write_config_word(bus->number, devfn, PCI_COMMAND, cr);
		}
	}
	/*
	 * We've scanned the bus and so we know all about what's on
	 * the other side of any bridges that may be on this bus plus
	 * any devices.
	 *
	 * Return how far we've got finding sub-buses.
	 */
	DBG("PCI: pci_scan_bus returning with max=%02x\n", max);
	return max;
}


__initfunc(void pci_init(void))
{
	pcibios_init();

	if (!pci_present()) {
		printk("PCI: No PCI bus detected\n");
		return;
	}

	printk("PCI: Probing PCI hardware.\n");

	memset(&pci_root, 0, sizeof(pci_root));
	pci_root.subordinate = pci_scan_bus(&pci_root);

	/* give BIOS a chance to apply platform specific fixes: */
	pcibios_fixup();

#ifdef CONFIG_PCI_OPTIMIZE
	pci_quirks_init();
#endif

#ifdef CONFIG_PROC_FS
	pci_proc_init();
#endif
}


__initfunc(void pci_setup (char *str, int *ints))
{
	while (str) {
		char *k = strchr(str, ',');
		if (k)
			*k++ = 0;
		if (*str && (str = pcibios_setup(str)) && *str) {
			if (!strcmp(str, "reverse"))
				pci_reverse = 1;
			else printk(KERN_ERR "PCI: Unknown option `%s'\n", str);
		}
		str = k;
	}
}
