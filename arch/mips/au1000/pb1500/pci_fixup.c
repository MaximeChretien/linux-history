/*
 * BRIEF MODULE DESCRIPTION
 *	Board specific pci fixups.
 *
 * Copyright 2001,2002 MontaVista Software Inc.
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

#ifdef CONFIG_PCI

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/au1000.h>
#include <asm/pb1500.h>

#undef	DEBUG
#ifdef 	DEBUG
#define	DBG(x...)	printk(x)
#else
#define	DBG(x...)	
#endif

static void fixup_resource(int r_num, struct pci_dev *dev) ;
static unsigned long virt_io_addr;

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
	/* will need to fixup IO resources */
}

void __init pcibios_fixup(void)
{
	int i;
	struct pci_dev *dev;
	
	virt_io_addr = (unsigned long)ioremap(Au1500_PCI_IO_START, 
			Au1500_PCI_IO_END - Au1500_PCI_IO_START + 1);

	if (!virt_io_addr) {
		printk(KERN_ERR "Unable to ioremap pci space\n");
		return;
	}

	pci_for_each_dev(dev) {
		for (i=0; i < DEVICE_COUNT_RESOURCE; i++) {
			if (dev->resource[i].start) {
				fixup_resource(i, dev);
			}
		}
	}
}

void __init pcibios_fixup_irqs(void)
{
	unsigned int slot, func;
	unsigned char pin;
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
		if (dev->bus->number != 0)
			return;

		dev->irq = 0xff;
		slot = PCI_SLOT(dev->devfn);
		switch (slot) {
			case 12:
			case 13:
				dev->irq = AU1000_PCI_INTA;
				break;

		}
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
		DBG("slot %d irq %d\n", slot, dev->irq);
	}
}
unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}

static void fixup_resource(int r_num, struct pci_dev *dev) 
{
	unsigned long start, size, new_start;

	if (dev->resource[r_num].flags & IORESOURCE_IO) {
		start = dev->resource[r_num].start;
		size = dev->resource[r_num].end - start;
		new_start = virt_io_addr + (start - Au1500_PCI_IO_START);
		dev->resource[r_num].start = new_start;
		dev->resource[r_num].end = new_start + size;
	}
}

#endif
