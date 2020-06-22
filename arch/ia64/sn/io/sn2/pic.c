/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001-2002 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <asm/sn/sgi.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/pci/pci_defs.h>
#include <asm/sn/prio.h>
#include <asm/sn/xtalk/xbow.h>
#include <asm/sn/ioc3.h>
#include <asm/sn/eeprom.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_private.h>


#define PCI_BUS_NO_1 1

int pic_devflag = D_MP;

extern int pcibr_attach2(devfs_handle_t, bridge_t *, devfs_handle_t, int, pcibr_soft_t *);
extern void pcibr_driver_reg_callback(devfs_handle_t, int, int, int);
extern void pcibr_driver_unreg_callback(devfs_handle_t, int, int, int);


void
pic_init(void)
{
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INIT, NULL, "pic_init()\n"));	

	xwidget_driver_register(PIC_WIDGET_PART_NUM_BUS0,
			    PIC_WIDGET_MFGR_NUM,
			    "pic_",
			    0);
}

int
pic_attach(devfs_handle_t conn_v)
{
	int		rc;
	bridge_t	*bridge0, *bridge1 = (bridge_t *)0;
	devfs_handle_t	pcibr_vhdl0, pcibr_vhdl1 = (devfs_handle_t)0;
	pcibr_soft_t	bus0_soft, bus1_soft = (pcibr_soft_t)0;

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v, "pic_attach()\n"));

	bridge0 = (bridge_t *) xtalk_piotrans_addr(conn_v, NULL,
	                        0, sizeof(bridge_t), 0);
#ifdef	PCI_BUS_NO_1
	bridge1 = (bridge_t *)((char *)bridge0 + PIC_BUS1_OFFSET);
#endif

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v,
		    "pic_attach: bridge0=0x%x, bridge1=0x%x\n", 
		    bridge0, bridge1));

	/*
	 * Create the vertex for the PCI buses, which we
	 * will also use to hold the pcibr_soft and
	 * which will be the "master" vertex for all the
	 * pciio connection points we will hang off it.
	 * This needs to happen before we call nic_bridge_vertex_info
	 * as we are some of the *_vmc functions need access to the edges.
	 *
	 * Opening this vertex will provide access to
	 * the Bridge registers themselves.
	 */
	/* FIXME: what should the hwgraph path look like ? */
	rc = hwgraph_path_add(conn_v, EDGE_LBL_PCIX_0, &pcibr_vhdl0);
	ASSERT(rc == GRAPH_SUCCESS);
#ifdef	PCI_BUS_NO_1
	rc = hwgraph_path_add(conn_v, EDGE_LBL_PCIX_1, &pcibr_vhdl1);
	ASSERT(rc == GRAPH_SUCCESS);
#endif

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v,
		    "pic_attach: pcibr_vhdl0=%v, pcibr_vhdl1=%v\n",
		    pcibr_vhdl0, pcibr_vhdl1));

	/* register pci provider array */
	pciio_provider_register(pcibr_vhdl0, &pci_pic_provider);
#ifdef	PCI_BUS_NO_1
	pciio_provider_register(pcibr_vhdl1, &pci_pic_provider);
#endif

	pciio_provider_startup(pcibr_vhdl0);
#ifdef	PCI_BUS_NO_1
	pciio_provider_startup(pcibr_vhdl1);
#endif

	pcibr_attach2(conn_v, bridge0, pcibr_vhdl0, 0, &bus0_soft);
#ifdef	PCI_BUS_NO_1
	pcibr_attach2(conn_v, bridge1, pcibr_vhdl1, 1, &bus1_soft);
#endif

	/* save a pointer to the PIC's other bus's soft struct */
#ifdef	PCI_BUS_NO_1
        bus0_soft->bs_peers_soft = bus1_soft;
        bus1_soft->bs_peers_soft = bus0_soft;
#else
        bus0_soft->bs_peers_soft = (pcibr_soft_t)0;
#endif

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v,
		    "pic_attach: bus0_soft=0x%x, bus1_soft=0x%x\n",
		    bus0_soft, bus1_soft));

	return 0;
}

/*
 * pci provider functions
 *
 * mostly in pcibr.c but if any are needed here then
 * this might be a way to get them here.
 */
pciio_provider_t        pci_pic_provider =
{
    (pciio_piomap_alloc_f *) pcibr_piomap_alloc,
    (pciio_piomap_free_f *) pcibr_piomap_free,
    (pciio_piomap_addr_f *) pcibr_piomap_addr,
    (pciio_piomap_done_f *) pcibr_piomap_done,
    (pciio_piotrans_addr_f *) pcibr_piotrans_addr,
    (pciio_piospace_alloc_f *) pcibr_piospace_alloc,
    (pciio_piospace_free_f *) pcibr_piospace_free,

    (pciio_dmamap_alloc_f *) pcibr_dmamap_alloc,
    (pciio_dmamap_free_f *) pcibr_dmamap_free,
    (pciio_dmamap_addr_f *) pcibr_dmamap_addr,
    (pciio_dmamap_list_f *) pcibr_dmamap_list,
    (pciio_dmamap_done_f *) pcibr_dmamap_done,
    (pciio_dmatrans_addr_f *) pcibr_dmatrans_addr,
    (pciio_dmatrans_list_f *) pcibr_dmatrans_list,
    (pciio_dmamap_drain_f *) pcibr_dmamap_drain,
    (pciio_dmaaddr_drain_f *) pcibr_dmaaddr_drain,
    (pciio_dmalist_drain_f *) pcibr_dmalist_drain,

    (pciio_intr_alloc_f *) pcibr_intr_alloc,
    (pciio_intr_free_f *) pcibr_intr_free,
    (pciio_intr_connect_f *) pcibr_intr_connect,
    (pciio_intr_disconnect_f *) pcibr_intr_disconnect,
    (pciio_intr_cpu_get_f *) pcibr_intr_cpu_get,

    (pciio_provider_startup_f *) pcibr_provider_startup,
    (pciio_provider_shutdown_f *) pcibr_provider_shutdown,
    (pciio_reset_f *) pcibr_reset,
    (pciio_write_gather_flush_f *) pcibr_write_gather_flush,
    (pciio_endian_set_f *) pcibr_endian_set,
    (pciio_priority_set_f *) pcibr_priority_set,
    (pciio_config_get_f *) pcibr_config_get,
    (pciio_config_set_f *) pcibr_config_set,
    (pciio_error_devenable_f *) 0,
    (pciio_error_extract_f *) 0,
    (pciio_driver_reg_callback_f *) pcibr_driver_reg_callback,
    (pciio_driver_unreg_callback_f *) pcibr_driver_unreg_callback,
    (pciio_device_unregister_f 	*) pcibr_device_unregister,
    (pciio_dma_enabled_f		*) pcibr_dma_enabled,
};
