/*
 *  setup.c, Setup for the TANBAC TB0226.
 *
 *  Copyright (C) 2002-2004  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/config.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/ioport.h>

#include <asm/pci_channel.h>
#include <asm/time.h>
#include <asm/vr41xx/tb0226.h>

#ifdef CONFIG_PCI
static struct resource vr41xx_pci_io_resource = {
	"PCI I/O space",
	VR41XX_PCI_IO_START,
	VR41XX_PCI_IO_END,
	IORESOURCE_IO
};

static struct resource vr41xx_pci_mem_resource = {
	"PCI memory space",
	VR41XX_PCI_MEM_START,
	VR41XX_PCI_MEM_END,
	IORESOURCE_MEM
};

struct pci_channel mips_pci_channels[] = {
	{&vr41xx_pci_ops, &vr41xx_pci_io_resource, &vr41xx_pci_mem_resource, 0, 256},
	{NULL, NULL, NULL, 0, 0}
};

struct vr41xx_pci_address_space vr41xx_pci_mem1 = {
	VR41XX_PCI_MEM1_BASE,
	VR41XX_PCI_MEM1_MASK,
	IO_MEM1_RESOURCE_START
};

struct vr41xx_pci_address_space vr41xx_pci_mem2 = {
	VR41XX_PCI_MEM2_BASE,
	VR41XX_PCI_MEM2_MASK,
	IO_MEM2_RESOURCE_START
};

struct vr41xx_pci_address_space vr41xx_pci_io = {
	VR41XX_PCI_IO_BASE,
	VR41XX_PCI_IO_MASK,
	IO_PORT_RESOURCE_START
};

static struct vr41xx_pci_address_map pci_address_map = {
	&vr41xx_pci_mem1,
	&vr41xx_pci_mem2,
	&vr41xx_pci_io
};
#endif

void __init tanbac_tb0226_setup(void)
{
	set_io_port_base(IO_PORT_BASE);
	ioport_resource.start = IO_PORT_RESOURCE_START;
	ioport_resource.end = IO_PORT_RESOURCE_END;
	iomem_resource.start = IO_MEM1_RESOURCE_START;
	iomem_resource.end = IO_MEM2_RESOURCE_END;

	board_time_init = vr41xx_time_init;
	board_timer_setup = vr41xx_timer_setup;

#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

	vr41xx_bcu_init();
	vr41xx_cmu_init();
	vr41xx_pmu_init();

#ifdef CONFIG_SERIAL
	vr41xx_siu_init(SIU_RS232C, 0);
#endif

#ifdef CONFIG_PCI
	vr41xx_pciu_init(&pci_address_map);
#endif
}
