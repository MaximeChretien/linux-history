#ifndef	PCI_H
#define PCI_H

/*
** Support for NE2000 PCI clones added David Monro June 1997
** Generalised for other PCI NICs by Ken Yap July 1997
**
** Most of this is taken from:
**
** /usr/src/linux/drivers/pci/pci.c
** /usr/src/linux/include/linux/pci.h
** /usr/src/linux/arch/i386/bios32.c
** /usr/src/linux/include/linux/bios32.h
** /usr/src/linux/drivers/net/ne.c
*/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#define PCI_COMMAND_IO			0x1	/* Enable response in I/O space */
#define PCI_COMMAND_MASTER		0x4	/* Enable bus mastering */
#define PCI_LATENCY_TIMER		0x0d	/* 8 bits */


#define PCI_VENDOR_ID           0x00	/* 16 bits */
#define PCI_DEVICE_ID           0x02	/* 16 bits */
#define PCI_COMMAND             0x04	/* 16 bits */

#define PCI_CLASS_CODE          0x0b	/* 8 bits */
#define PCI_SUBCLASS_CODE       0x0a	/* 8 bits */
#define PCI_HEADER_TYPE         0x0e	/* 8 bits */

#define PCI_BASE_ADDRESS_0      0x10	/* 32 bits */
#define PCI_BASE_ADDRESS_1      0x14	/* 32 bits */
#define PCI_BASE_ADDRESS_2      0x18	/* 32 bits */
#define PCI_BASE_ADDRESS_3      0x1c	/* 32 bits */
#define PCI_BASE_ADDRESS_4      0x20	/* 32 bits */
#define PCI_BASE_ADDRESS_5      0x24	/* 32 bits */

#ifndef	PCI_BASE_ADDRESS_IO_MASK
#define	PCI_BASE_ADDRESS_IO_MASK       (~0x03)
#endif
#define	PCI_BASE_ADDRESS_SPACE_IO	0x01
#define	PCI_ROM_ADDRESS		0x30	/* 32 bits */
#define	PCI_ROM_ADDRESS_ENABLE	0x01	/* Write 1 to enable ROM,
					   bits 31..11 are address,
					   10..2 are reserved */

#define PCI_FUNC(devfn)           ((devfn) & 0x07)

#define BIOS32_SIGNATURE        (('_' << 0) + ('3' << 8) + ('2' << 16) + ('_' << 24))

/* PCI signature: "PCI " */
#define PCI_SIGNATURE           (('P' << 0) + ('C' << 8) + ('I' << 16) + (' ' << 24))

/* PCI service signature: "$PCI" */
#define PCI_SERVICE             (('$' << 0) + ('P' << 8) + ('C' << 16) + ('I' << 24))

#define KERN_CODE_SEG	0x8	/* This _MUST_ match start.S */

#define PCI_VENDOR_ID_REALTEK           0x10ec
#define PCI_DEVICE_ID_REALTEK_8029      0x8029
#define PCI_DEVICE_ID_REALTEK_8139      0x8139
#define PCI_VENDOR_ID_WINBOND2          0x1050
#define PCI_DEVICE_ID_WINBOND2_89C940   0x0940
#define PCI_VENDOR_ID_COMPEX            0x11f6
#define PCI_DEVICE_ID_COMPEX_RL2000     0x1401
#define PCI_VENDOR_ID_KTI               0x8e2e
#define PCI_DEVICE_ID_KTI_ET32P2        0x3000
#define PCI_VENDOR_ID_NETVIN            0x4a14
#define PCI_DEVICE_ID_NETVIN_NV5000SC   0x5000
#define PCI_VENDOR_ID_3COM		0x10b7
#define PCI_DEVICE_ID_3COM_3C900TPO	0x9000
#define PCI_DEVICE_ID_3COM_3C900COMBO	0x9001
#define PCI_DEVICE_ID_3COM_3C905TX	0x9050
#define PCI_DEVICE_ID_3COM_3C905T4	0x9051
#define PCI_DEVICE_ID_3COM_3C905B_TX	0x9055
#define PCI_DEVICE_ID_3COM_3C905C_TXM	0x9200
#define PCI_VENDOR_ID_INTEL		0x8086
#define PCI_DEVICE_ID_INTEL_82557	0x1229
#define PCI_VENDOR_ID_AMD		0x1022
#define PCI_DEVICE_ID_AMD_LANCE		0x2000
#define PCI_VENDOR_ID_SMC_1211          0x1113
#define PCI_DEVICE_ID_SMC_1211          0x1211
#define PCI_VENDOR_ID_DEC		0x1011
#define PCI_DEVICE_ID_DEC_TULIP		0x0002
#define PCI_DEVICE_ID_DEC_TULIP_FAST	0x0009
#define PCI_DEVICE_ID_DEC_TULIP_PLUS	0x0014
#define PCI_DEVICE_ID_DEC_21142		0x0019
#define PCI_VENDOR_ID_SMC		0x10B8
#ifndef	PCI_DEVICE_ID_SMC_EPIC100
# define PCI_DEVICE_ID_SMC_EPIC100	0x0005
#endif
#define PCI_VENDOR_ID_MACRONIX		0x10d9
#define PCI_DEVICE_ID_MX987x5		0x0531
#define PCI_VENDOR_ID_LINKSYS		0x11AD
#define PCI_DEVICE_ID_LC82C115		0xC115
#define PCI_VENDOR_ID_VIATEC		0x1106
#define PCI_DEVICE_ID_VIA_RHINE_I	0x3043
#define PCI_DEVICE_ID_VIA_86C100A	0x6100
#define PCI_VENDOR_ID_DAVICOM		0x1282
#define PCI_DEVICE_ID_DM9102		0x9102

struct pci_device {
	unsigned short vendor, dev_id;
	const char *name;
	unsigned int membase;
	unsigned int ioaddr;
	unsigned short devfn;
	unsigned short bus;
};

extern void eth_pci_init(struct pci_device *);
int pcibios_read_config_byte(unsigned char bus, unsigned char dev_fn,
			     unsigned char where, unsigned char *val);
int pcibios_read_config_word(unsigned char bus, unsigned char dev_fn,
			     unsigned char where, unsigned short *val);
int pcibios_read_config_dword(unsigned char bus, unsigned char dev_fn,
			      unsigned char where, unsigned int *val);
int pcibios_write_config_byte(unsigned char bus, unsigned char dev_fn,
			      unsigned char where, unsigned char val);
int pcibios_write_config_word(unsigned char bus, unsigned char dev_fn,
			      unsigned char where, unsigned short val);
int pcibios_write_config_dword(unsigned char bus, unsigned char dev_fn,
			       unsigned char where, unsigned int val);

/*
 * Error values that may be returned by the PCI bios.
 */
#define PCIBIOS_SUCCESSFUL              0x00
#define PCIBIOS_FUNC_NOT_SUPPORTED      0x81
#define PCIBIOS_BAD_VENDOR_ID           0x83
#define PCIBIOS_DEVICE_NOT_FOUND        0x86
#define PCIBIOS_BAD_REGISTER_NUMBER     0x87
#define PCIBIOS_SET_FAILED              0x88
#define PCIBIOS_BUFFER_TOO_SMALL        0x89



#endif				/* PCI_H */
