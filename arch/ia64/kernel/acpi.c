/*
 *  acpi.c - Architecture-Specific Low-Level ACPI Support
 *
 *  Copyright (C) 1999 VA Linux Systems
 *  Copyright (C) 1999,2000 Walt Drummond <drummond@valinux.com>
 *  Copyright (C) 2000, 2002-2003 Hewlett-Packard Co.
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *  Copyright (C) 2000 Intel Corp.
 *  Copyright (C) 2000,2001 J.I. Lee <jung-ik.lee@intel.com>
 *  Copyright (C) 2001 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/irq.h>
#include <linux/acpi.h>
#include <linux/efi.h>
#include <asm/io.h>
#include <asm/iosapic.h>
#include <asm/machvec.h>
#include <asm/page.h>
#include <asm/system.h>


#define PREFIX			"ACPI: "

asm (".weak iosapic_register_intr");
asm (".weak iosapic_override_isa_irq");
asm (".weak iosapic_register_platform_intr");
asm (".weak iosapic_init");
asm (".weak iosapic_system_init");
asm (".weak iosapic_version");

void (*pm_idle) (void);
void (*pm_power_off) (void);

unsigned char acpi_kbd_controller_present = 1;

const char *
acpi_get_sysname (void)
{
#ifdef CONFIG_IA64_GENERIC
	unsigned long rsdp_phys;
	struct acpi20_table_rsdp *rsdp;
	struct acpi_table_xsdt *xsdt;
	struct acpi_table_header *hdr;

	rsdp_phys = acpi_find_rsdp();
	if (!rsdp_phys) {
		printk(KERN_ERR "ACPI 2.0 RSDP not found, default to \"dig\"\n");
		return "dig";
	}

	rsdp = (struct acpi20_table_rsdp *) __va(rsdp_phys);
	if (strncmp(rsdp->signature, RSDP_SIG, sizeof(RSDP_SIG) - 1)) {
		printk(KERN_ERR "ACPI 2.0 RSDP signature incorrect, default to \"dig\"\n");
		return "dig";
	}

	xsdt = (struct acpi_table_xsdt *) __va(rsdp->xsdt_address);
	hdr = &xsdt->header;
	if (strncmp(hdr->signature, XSDT_SIG, sizeof(XSDT_SIG) - 1)) {
		printk(KERN_ERR "ACPI 2.0 XSDT signature incorrect, default to \"dig\"\n");
		return "dig";
	}

	if (!strcmp(hdr->oem_id, "HP")) {
		return "hpzx1";
	}

	return "dig";
#else
# if defined (CONFIG_IA64_HP_SIM)
	return "hpsim";
# elif defined (CONFIG_IA64_HP_ZX1)
	return "hpzx1";
# elif defined (CONFIG_IA64_SGI_SN1)
	return "sn1";
# elif defined (CONFIG_IA64_SGI_SN2)
	return "sn2";
# elif defined (CONFIG_IA64_DIG)
	return "dig";
# else
#	error Unknown platform.  Fix acpi.c.
# endif
#endif
}

#ifdef CONFIG_ACPI

struct acpi_vendor_descriptor {
	u8				guid_id;
	efi_guid_t			guid;
};

struct acpi_vendor_info {
	struct acpi_vendor_descriptor	*descriptor;
	u8				*data;
	u32				length;
};

acpi_status
acpi_vendor_resource_match(acpi_resource *resource, void *context)
{
	struct acpi_vendor_info *info = (struct acpi_vendor_info *) context;
	acpi_resource_vendor *vendor;
	struct acpi_vendor_descriptor *descriptor;
	u32 length;

	if (resource->id != ACPI_RSTYPE_VENDOR)
		return AE_OK;

	vendor = (acpi_resource_vendor *) &resource->data;
	descriptor = (struct acpi_vendor_descriptor *) vendor->reserved;
	if (vendor->length <= sizeof(*info->descriptor) ||
	    descriptor->guid_id != info->descriptor->guid_id ||
	    efi_guidcmp(descriptor->guid, info->descriptor->guid))
		return AE_OK;

	length = vendor->length - sizeof(struct acpi_vendor_descriptor);
	info->data = acpi_os_allocate(length);
	if (!info->data)
		return AE_NO_MEMORY;

	memcpy(info->data, vendor->reserved + sizeof(struct acpi_vendor_descriptor), length);
	info->length = length;
	return AE_CTRL_TERMINATE;
}

acpi_status
acpi_find_vendor_resource(acpi_handle obj, struct acpi_vendor_descriptor *id,
		u8 **data, u32 *length)
{
	struct acpi_vendor_info info;

	info.descriptor = id;
	info.data = 0;

	acpi_walk_resources(obj, METHOD_NAME__CRS, acpi_vendor_resource_match, &info);
	if (!info.data)
		return AE_NOT_FOUND;

	*data = info.data;
	*length = info.length;
	return AE_OK;
}

struct acpi_vendor_descriptor hp_ccsr_descriptor = {
	.guid_id = 2,
	.guid    = EFI_GUID(0x69e9adf9, 0x924f, 0xab5f, 0xf6, 0x4a, 0x24, 0xd2, 0x01, 0x37, 0x0e, 0xad)
};

acpi_status
acpi_hp_csr_space(acpi_handle obj, u64 *csr_base, u64 *csr_length)
{
	acpi_status status;
	u8 *data;
	u32 length;
	int i;

	status = acpi_find_vendor_resource(obj, &hp_ccsr_descriptor, &data, &length);

	if (ACPI_FAILURE(status) || length != 16)
		return AE_NOT_FOUND;

	memcpy(csr_base, data, sizeof(*csr_base));
	memcpy(csr_length, data + 8, sizeof(*csr_length));
	acpi_os_free(data);

	return AE_OK;
}

#endif /* CONFIG_ACPI */

#ifdef CONFIG_ACPI_BOOT

#define ACPI_MAX_PLATFORM_INTERRUPTS	256

/* Array to record platform interrupt vectors for generic interrupt routing. */
int platform_intr_list[ACPI_MAX_PLATFORM_INTERRUPTS] = { [0 ... ACPI_MAX_PLATFORM_INTERRUPTS - 1] = -1 };

enum acpi_irq_model_id acpi_irq_model = ACPI_IRQ_MODEL_IOSAPIC;

/*
 * Interrupt routing API for device drivers.  Provides interrupt vector for
 * a generic platform event.  Currently only CPEI is implemented.
 */
int
acpi_request_vector (u32 int_type)
{
	int vector = -1;

	if (int_type < ACPI_MAX_PLATFORM_INTERRUPTS) {
		/* correctable platform error interrupt */
		vector = platform_intr_list[int_type];
	} else
		printk(KERN_ERR "acpi_request_vector(): invalid interrupt type\n");
	return vector;
}

char *
__acpi_map_table (unsigned long phys_addr, unsigned long size)
{
	return __va(phys_addr);
}

/* --------------------------------------------------------------------------
                            Boot-time Table Parsing
   -------------------------------------------------------------------------- */

static int			total_cpus __initdata;
static int			available_cpus __initdata;
struct acpi_table_madt *	acpi_madt __initdata;
static u8			has_8259;


static int __init
acpi_parse_lapic_addr_ovr (acpi_table_entry_header *header)
{
	struct acpi_table_lapic_addr_ovr *lapic;

	lapic = (struct acpi_table_lapic_addr_ovr *) header;
	if (!lapic)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	if (lapic->address) {
		iounmap((void *) ipi_base_addr);
		ipi_base_addr = (unsigned long) ioremap(lapic->address, 0);
	}

	return 0;
}


static int __init
acpi_parse_lsapic (acpi_table_entry_header *header)
{
	struct acpi_table_lsapic *lsapic;
	int phys_id;

	lsapic = (struct acpi_table_lsapic *) header;
	if (!lsapic)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	phys_id = (lsapic->id << 8) | lsapic->eid;

	if (total_cpus == NR_CPUS) {
		printk(KERN_ERR PREFIX "Ignoring CPU (0x%04x) (NR_CPUS == %d)\n",
			phys_id, NR_CPUS);
		return 0;
	}

	printk(KERN_INFO "CPU %d (0x%04x)", total_cpus, phys_id);

	if (lsapic->flags.enabled) {
		available_cpus++;
		printk(" enabled");
#ifdef CONFIG_SMP
		smp_boot_data.cpu_phys_id[total_cpus] = phys_id;
		if (hard_smp_processor_id() == smp_boot_data.cpu_phys_id[total_cpus])
			printk(" (BSP)");
#endif
	}
	else {
		printk(" disabled");
#ifdef CONFIG_SMP
		smp_boot_data.cpu_phys_id[total_cpus] = -1;
#endif
	}

	printk("\n");

	total_cpus++;
	return 0;
}


static int __init
acpi_parse_lapic_nmi (acpi_table_entry_header *header)
{
	struct acpi_table_lapic_nmi *lacpi_nmi;

	lacpi_nmi = (struct acpi_table_lapic_nmi*) header;
	if (!lacpi_nmi)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	/* TBD: Support lapic_nmi entries */

	return 0;
}


static int __init
acpi_parse_iosapic (acpi_table_entry_header *header)
{
	struct acpi_table_iosapic *iosapic;

	iosapic = (struct acpi_table_iosapic *) header;
	if (!iosapic)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	if (iosapic_init)
		iosapic_init(iosapic->address, iosapic->global_irq_base);

	return 0;
}


static int __init
acpi_parse_plat_int_src (acpi_table_entry_header *header)
{
	struct acpi_table_plat_int_src *plintsrc;
	int vector;

	plintsrc = (struct acpi_table_plat_int_src *) header;
	if (!plintsrc)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	if (!iosapic_register_platform_intr) {
		printk(KERN_WARNING PREFIX "No ACPI platform interrupt support\n");
		return -ENODEV;
	}

	/*
	 * Get vector assignment for this interrupt, set attributes,
	 * and program the IOSAPIC routing table.
	 */
	vector = iosapic_register_platform_intr(plintsrc->type,
						plintsrc->global_irq,
						plintsrc->iosapic_vector,
						plintsrc->eid,
						plintsrc->id,
						(plintsrc->flags.polarity == 1) ? IOSAPIC_POL_HIGH : IOSAPIC_POL_LOW,
						(plintsrc->flags.trigger == 1) ? IOSAPIC_EDGE : IOSAPIC_LEVEL);

	platform_intr_list[plintsrc->type] = vector;
	return 0;
}


static int __init
acpi_parse_int_src_ovr (acpi_table_entry_header *header)
{
	struct acpi_table_int_src_ovr *p;

	p = (struct acpi_table_int_src_ovr *) header;
	if (!p)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	/* Ignore if the platform doesn't support overrides */
	if (!iosapic_override_isa_irq)
		return 0;

	iosapic_override_isa_irq(p->bus_irq, p->global_irq,
				 (p->flags.polarity == 1) ? IOSAPIC_POL_HIGH : IOSAPIC_POL_LOW,
				 (p->flags.trigger == 1) ? IOSAPIC_EDGE : IOSAPIC_LEVEL);
	return 0;
}


static int __init
acpi_parse_nmi_src (acpi_table_entry_header *header)
{
	struct acpi_table_nmi_src *nmi_src;

	nmi_src = (struct acpi_table_nmi_src*) header;
	if (!nmi_src)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	/* TBD: Support nimsrc entries */

	return 0;
}


static int __init
acpi_parse_madt (unsigned long phys_addr, unsigned long size)
{
	if (!phys_addr || !size)
		return -EINVAL;

	acpi_madt = (struct acpi_table_madt *) __va(phys_addr);

	/* remember the value for reference after free_initmem() */
#ifdef CONFIG_ITANIUM
	has_8259 = 1; /* Firmware on old Itanium systems is broken */
#else
	has_8259 = acpi_madt->flags.pcat_compat;
#endif
	if (iosapic_system_init)
		iosapic_system_init(has_8259);

	/* Get base address of IPI Message Block */

	if (acpi_madt->lapic_address)
		ipi_base_addr = (unsigned long) ioremap(acpi_madt->lapic_address, 0);

	printk(KERN_INFO PREFIX "Local APIC address 0x%lx\n", ipi_base_addr);
	return 0;
}


static int __init
acpi_parse_fadt (unsigned long phys_addr, unsigned long size)
{
	struct acpi_table_header *fadt_header;
	fadt_descriptor_rev2 *fadt;
	u32 sci_irq;

	if (!phys_addr || !size)
		return -EINVAL;

	fadt_header = (struct acpi_table_header *) __va(phys_addr);

	if (fadt_header->revision != 3)
		return -ENODEV;		/* Only deal with ACPI 2.0 FADT */

	fadt = (fadt_descriptor_rev2 *) fadt_header;

	if (!(fadt->iapc_boot_arch & BAF_8042_KEYBOARD_CONTROLLER))
		acpi_kbd_controller_present = 0;

	sci_irq = fadt->sci_int;

	if (has_8259 && sci_irq < 16)
		return 0;	/* legacy, no setup required */

	if (!iosapic_register_intr)
		return -ENODEV;

	iosapic_register_intr(sci_irq, IOSAPIC_POL_LOW, IOSAPIC_LEVEL);
	return 0;
}


unsigned long __init
acpi_find_rsdp (void)
{
	unsigned long rsdp_phys = 0;

	if (efi.acpi20)
		rsdp_phys = __pa(efi.acpi20);
	else if (efi.acpi)
		printk(KERN_WARNING PREFIX "v1.0/r0.71 tables no longer supported\n");

	return rsdp_phys;
}


int __init
acpi_boot_init (char *cmdline)
{
	int result;

	/* Initialize the ACPI boot-time table parser */
	result = acpi_table_init(cmdline);
	if (result)
		return result;

	/*
	 * MADT
	 * ----
	 * Parse the Multiple APIC Description Table (MADT), if exists.
	 * Note that this table provides platform SMP configuration
	 * information -- the successor to MPS tables.
	 */

	if (acpi_table_parse(ACPI_APIC, acpi_parse_madt) < 1) {
		printk(KERN_ERR PREFIX "Can't find MADT\n");
		goto skip_madt;
	}

	/* Local APIC */

	if (acpi_table_parse_madt(ACPI_MADT_LAPIC_ADDR_OVR,
				  acpi_parse_lapic_addr_ovr) < 0)
		printk(KERN_ERR PREFIX "Error parsing LAPIC address override entry\n");

	if (acpi_table_parse_madt(ACPI_MADT_LSAPIC,
				  acpi_parse_lsapic) < 1)
		printk(KERN_ERR PREFIX "Error parsing MADT - no LAPIC entries\n");

	if (acpi_table_parse_madt(ACPI_MADT_LAPIC_NMI,
				  acpi_parse_lapic_nmi) < 0)
		printk(KERN_ERR PREFIX "Error parsing LAPIC NMI entry\n");

	/* I/O APIC */

	if (acpi_table_parse_madt(ACPI_MADT_IOSAPIC,
				  acpi_parse_iosapic) < 1)
		printk(KERN_ERR PREFIX "Error parsing MADT - no IOAPIC entries\n");

	/* System-Level Interrupt Routing */

	if (acpi_table_parse_madt(ACPI_MADT_PLAT_INT_SRC,
				  acpi_parse_plat_int_src) < 0)
		printk(KERN_ERR PREFIX "Error parsing platform interrupt source entry\n");

	if (acpi_table_parse_madt(ACPI_MADT_INT_SRC_OVR,
				  acpi_parse_int_src_ovr) < 0)
		printk(KERN_ERR PREFIX "Error parsing interrupt source overrides entry\n");

	if (acpi_table_parse_madt(ACPI_MADT_NMI_SRC,
				  acpi_parse_nmi_src) < 0)
		printk(KERN_ERR PREFIX "Error parsing NMI SRC entry\n");
skip_madt:

	/*
	 * The FADT table contains an SCI_INT line, by which the system
	 * gets interrupts such as power and sleep buttons.  If it's not
	 * on a Legacy interrupt, it needs to be setup.
	 */
	if (acpi_table_parse(ACPI_FACP, acpi_parse_fadt) < 1)
		printk(KERN_ERR PREFIX "Can't find FADT\n");

#ifdef CONFIG_SMP
	if (available_cpus == 0) {
		printk(KERN_INFO "ACPI: Found 0 CPUS; assuming 1\n");
		available_cpus = 1; /* We've got at least one of these, no? */
	}
	smp_boot_data.cpu_count = total_cpus;
#endif
	/* Make boot-up look pretty */
	printk(KERN_INFO "%d CPUs available, %d CPUs total\n", available_cpus, total_cpus);
	return 0;
}


/* --------------------------------------------------------------------------
                             PCI Interrupt Routing
   -------------------------------------------------------------------------- */
#ifdef CONFIG_PCI

int __init
acpi_get_prt (struct pci_vector_struct **vectors, int *count)
{
	struct pci_vector_struct *vector;
	struct list_head *node;
	struct acpi_prt_entry *entry;
	int i = 0;

	if (!vectors || !count)
		return -EINVAL;

	*vectors = NULL;
	*count = 0;

	if (acpi_prt.count < 0) {
		printk(KERN_ERR PREFIX "No PCI interrupt routing entries\n");
		return -ENODEV;
	}

	/* Allocate vectors */

	*vectors = kmalloc(sizeof(struct pci_vector_struct) * acpi_prt.count, GFP_KERNEL);
	if (!(*vectors))
		return -ENOMEM;

	/* Convert PRT entries to IOSAPIC PCI vectors */

	vector = *vectors;

	list_for_each(node, &acpi_prt.entries) {
		entry = (struct acpi_prt_entry *)node;
		vector[i].segment = entry->id.segment;
		vector[i].bus    = entry->id.bus;
		vector[i].pci_id = ((u32) entry->id.device << 16) | 0xffff;
		vector[i].pin    = entry->pin;
		vector[i].irq    = entry->link.index;
		i++;
	}
	*count = acpi_prt.count;
	return 0;
}
#endif
/* Assume IA64 always use I/O SAPIC */

int __init
acpi_get_interrupt_model (int *type)
{
        if (!type)
                return -EINVAL;

	*type = ACPI_IRQ_MODEL_IOSAPIC;
        return 0;
}

int
acpi_irq_to_vector (u32 irq)
{
	if (has_8259 && irq < 16)
		return isa_irq_to_vector(irq);

	return gsi_to_vector(irq);
}

#endif /* CONFIG_ACPI_BOOT */
