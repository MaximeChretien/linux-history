#ifndef __PPC_PCI_H
#define __PPC_PCI_H

#include <linux/config.h>
#include <linux/pci.h>

/* Values for the `which' argument to sys_pciconfig_iobase.  */
#define IOBASE_BRIDGE_NUMBER	0
#define IOBASE_MEMORY		1
#define IOBASE_IO		2

/* Return the index of the PCI controller for device PDEV. */
#define pci_controller_num(PDEV)	(0)

#endif
