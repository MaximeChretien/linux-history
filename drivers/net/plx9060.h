/* plx9060
 * -*- linux-c -*-
 * 
 * SBE wanXL device driver
 * Copyright (C) 1999 RG Studio s.c., http://www.rgstudio.com.pl/
 * Written by Krzysztof Halasa <khc@rgstudio.com.pl>
 *
 * Portions (C) SBE Inc., used by permission.
 *
 * Sources:
 *	wanXL technical reference manuals
 *	wanXL UNIXware X.25 driver
 *	Donald Becker's skeleton.c driver
 *	"Linux Kernel Module Programming" by Ori Pomerantz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __PLX9060_H
#define __PLX9060_H


typedef struct {
	/*
	 * Local Configuration Registers
	 */
	volatile u32 range_las;	/* Range for PCI to Local Address Space */
	volatile u32 remap_las;	/* Re-map for PCI to Local Address Spc */
	volatile u32 lcl_arb;	/* 9060ES Local Arbitration Register */
	volatile u32 _res5;	/* Reserved */
	volatile u32 range_rom;	/* Range for PCI to Expansion ROM */
	volatile u32 remap_rom;	/* Re-map for PCI to Expansion ROM */
	volatile u32 region;	/* Bus Region Descriptors for PCI to Local */
	volatile u32 range_dm;	/* Range for Direct Master to PCI */
	volatile u32 lba_pmem;	/* Local Base Address for PCI Memory Space */
	volatile u32 lba_pio;	/* Local Base Address for PCI I/O - Cfg Spc*/
	volatile u32 remap_dm;	/* Re-map for Direct Master to PCI */
	volatile u32 car_cfgio;	/* Configuration Address Reg Cfg | I/O Spc */

	volatile u8 _res6[(0x40 - 0x2C - 0x04)];

	volatile u32 mbox_0;	/* Mailbox #0 - sts */
	volatile u32 mbox_1;	/* Mailbox #1 - cmd */
	volatile u32 mbox_2;	/* Mailbox #2 - user */
	volatile u32 mbox_3;	/* Mailbox #3 - intr */
	volatile u32 mbox_4;	/* Mailbox #4 */
	volatile u32 mbox_5;	/* Mailbox #5 */
	volatile u32 mbox_6;	/* Mailbox #6 */
	volatile u32 mbox_7;	/* Mailbox #7 */

	volatile u32 dbr_in;	/* Doorbell Reg Incoming Interrupts from PCI */
	volatile u32 dbr_out;	/* Doorbell Reg Outgoing Interrupts to PCI */
	volatile u32 intr_cs;	/* Interrupt Control and Status */
	volatile u32 control;	/* EEPROM, Cmd codes, User I/O, Init Done */

	volatile u8 _res8[(0x100 - 0xEC - 0x04)];

	volatile u32 dma_mode_0; /* DMA Chan. #0 Mode Register */
	volatile u32 dma_pci_0;	/* DMA Chan. #0 PCI Address Register */
	volatile u32 dma_lcl_0;	/* DMA Chan. #0 Local Address Register */
	volatile u32 dma_cnt_0;	/* DMA Chan. #0 Transfer Count Register */
	volatile u32 dma_desc_0; /* DMA Chan. #0 Descriptor Pointer */

	volatile u32 dma_mode_1; /* DMA Chan. #1 Mode Register */
	volatile u32 dma_pci_1;	/* DMA Chan. #1 PCI Address Register */
	volatile u32 dma_lcl_1;	/* DMA Chan. #1 Local Address Register */
	volatile u32 dma_cnt_1;	/* DMA Chan. #1 Transfer Count Register */
	volatile u32 dma_desc_1; /* DMA Chan. #1 Descriptor Pointer */

	volatile u32 dma_csr;	/* DMA Command/Status Register */
	volatile u32 dma_arb_0;	/* DMA Arbitration Register 0 */
	volatile u32 dma_arb_1;	/* DMA Arbitration Register 1 */
}plx9060;


/**********************************************************************
**            Register Offsets and Bit Definitions
**
** Note: All offsets zero relative.  IE. Some standard base address
** must be added to the Register Number to properly access the register.
**
**********************************************************************/

#define PLX_LASMAP_REG         0x0000 /* L, Local Addr Space Range Register */
#define  LASMAP_IO         0x00000001 /* Map to: 1=I/O, 0=Mem */
#define  LMAP_ANY32        0x00000000 /* Locate anywhere in 32 bit */
#define  LMAP_LT1MB        0x00000002 /* Locate in 1st meg */
#define  LMAP_ANY64        0x00000004 /* Locate anywhere in 64 bit */

#define PLX_LASRNG_REG         0x0004 /* L, Local Addr Space Range Register */
#define  LRNG_EN           0x00000001 /* Enable slave decode */
#define  LRNG_IO           0xFFFFFFFC /* Decode bits if I/O spc */
#define  LRNG_MEM          0xFFFFFFF0 /* Decode bits if mem spc */


/* Note: The Local Arbitration Register is only present on the 9060ES part.
**       The 9060 part with DMA does not have this register
*/
#define PLX_LCLARB_REG         0x0008 /* L, Local Arbitration Register */
#define  LARB_LLT          0x0000000F /* Local Bus Latency Timer */
#define  LARB_LPT          0x000000F0 /* Local Bus Pause Timer */
#define  LARB_LTEN         0x00000100 /* Latency Timer Enable */
#define  LARB_LPEN         0x00000200 /* Pause Timer Enable */
#define  LARB_BREQ         0x00000400 /* Local Bus BREQ Enable */

/* Note: The Expansion ROM  stuff is only relevant to the PC environment.
**       This expansion ROM code is executed by the host CPU at boot time.
**       For this reason no bit definitions are provided here.
*/
#define PLX_ROMRNG_REG         0x0010 /* L, Expn ROM Space Range Register */
#define PLX_ROMMAP_REG         0x0014 /* L, Local Addr Space Range Register */


#define PLX_REGION_REG         0x0018 /* L, Local Bus Region Descriptor */
#define  RGN_WIDTH         0x00000002 /* Local bus width bits */
#define  RGN_8BITS         0x00000000 /* 08 bit Local Bus */
#define  RGN_16BITS        0x00000001 /* 16 bit Local Bus */
#define  RGN_32BITS        0x00000002 /* 32 bit Local Bus */
#define  RGN_MWS           0x0000003C /* Memory Access Wait States */
#define  RGN_0MWS          0x00000000
#define  RGN_1MWS          0x00000004
#define  RGN_2MWS          0x00000008
#define  RGN_3MWS          0x0000000C
#define  RGN_4MWS          0x00000010
#define  RGN_6MWS          0x00000018
#define  RGN_8MWS          0x00000020
#define  RGN_MRE           0x00000040 /* Memory Space Ready Input Enable */
#define  RGN_MBE           0x00000080 /* Memory Space Bterm Input Enable */
#define  RGN_RWS           0x003C0000 /* Expn ROM Wait States */
#define  RGN_RRE           0x00400000 /* ROM Space Ready Input Enable */
#define  RGN_RBE           0x00800000 /* ROM Space Bterm Input Enable */
#define  RGN_MBEN          0x01000000 /* Memory Space Burst Enable */
#define  RGN_RBEN          0x04000000 /* ROM Space Burst Enable */
#define  RGN_THROT         0x08000000 /* De-assert TRDY when FIFO full */
#define  RGN_TRD           0xF0000000 /* Target Ready Delay /8 */


#define PLX_DMRNG_REG          0x001C /* L, Direct Master Range Register */

#define PLX_LBAPMEM_REG        0x0020 /* L, Lcl Base Addr for PCI mem space */

#define PLX_LBAPIO_REG         0x0024 /* L, Lcl Base Addr for PCI I/O space */

#define PLX_DMMAP_REG          0x0028 /* L, Direct Master Remap Register */
#define  DMM_MAE           0x00000001 /* Direct Mstr Memory Acc Enable */
#define  DMM_IAE           0x00000002 /* Direct Mstr I/O Acc Enable */
#define  DMM_LCK           0x00000004 /* LOCK Input Enable */
#define  DMM_PF4           0x00000008 /* Prefetch 4 Mode Enable */
#define  DMM_THROT         0x00000010 /* Assert IRDY when read FIFO full */
#define  DMM_PAF0          0x00000000 /* Programmable Almost fill level */
#define  DMM_PAF1          0x00000020 /* Programmable Almost fill level */
#define  DMM_PAF2          0x00000040 /* Programmable Almost fill level */
#define  DMM_PAF3          0x00000060 /* Programmable Almost fill level */
#define  DMM_PAF4          0x00000080 /* Programmable Almost fill level */
#define  DMM_PAF5          0x000000A0 /* Programmable Almost fill level */
#define  DMM_PAF6          0x000000C0 /* Programmable Almost fill level */
#define  DMM_PAF7          0x000000D0 /* Programmable Almost fill level */
#define  DMM_MAP           0xFFFF0000 /* Remap Address Bits */

#define PLX_CAR_REG            0x002C /* L, Configuration Address Register */
#define  CAR_CT0           0x00000000 /* Config Type 0 */
#define  CAR_CT1           0x00000001 /* Config Type 1 */
#define  CAR_REG           0x000000FC /* Register Number Bits */
#define  CAR_FUN           0x00000700 /* Function Number Bits */
#define  CAR_DEV           0x0000F800 /* Device Number Bits */
#define  CAR_BUS           0x00FF0000 /* Bus Number Bits */
#define  CAR_CFG           0x80000000 /* Config Spc Access Enable */


#define PLX_DBR_IN_REG         0x0060 /* L, PCI to Local Doorbell Register */

#define PLX_DBR_OUT_REG        0x0064 /* L, Local to PCI Doorbell Register */

#define PLX_INTRCS_REG         0x0068 /* L, Interrupt Control/Status Reg */
#define  ICS_AERR          0x00000001 /* Assert LSERR on ABORT */
#define  ICS_PERR          0x00000002 /* Assert LSERR on Parity Error */
#define  ICS_SERR          0x00000004 /* Generate PCI SERR# */
#define  ICS_PIE           0x00000100 /* PCI Interrupt Enable */
#define  ICS_PDIE          0x00000200 /* PCI Doorbell Interrupt Enable */
#define  ICS_PAIE          0x00000400 /* PCI Abort Interrupt Enable */
#define  ICS_PLIE          0x00000800 /* PCI Local Int Enable */
#define  ICS_RAE           0x00001000 /* Retry Abort Enable */
#define  ICS_PDIA          0x00002000 /* PCI Doorbell Interrupt Active */
#define  ICS_PAIA          0x00004000 /* PCI Abort Interrupt Active */
#define  ICS_LIA           0x00008000 /* Local Interrupt Active */
#define  ICS_LIE           0x00010000 /* Local Interrupt Enable */
#define  ICS_LDIE          0x00020000 /* Local Doorbell Int Enable */
#define  ICS_DMA0_E        0x00040000 /* DMA #0 Interrupt Enable */
#define  ICS_DMA1_E        0x00080000 /* DMA #1 Interrupt Enable */
#define  ICS_LDIA          0x00100000 /* Local Doorbell Int Active */
#define  ICS_DMA0_A        0x00200000 /* DMA #0 Interrupt Active */
#define  ICS_DMA1_A        0x00400000 /* DMA #1 Interrupt Active */
#define  ICS_BIA           0x00800000 /* BIST Interrupt Active */
#define  ICS_TA_DM         0x01000000 /* Target Abort - Direct Master */
#define  ICS_TA_DMA0       0x02000000 /* Target Abort - DMA #0 */
#define  ICS_TA_DMA1       0x04000000 /* Target Abort - DMA #1 */
#define  ICS_TA_RA         0x08000000 /* Target Abort - Retry Timeout */

#define PLX_CONTROL_REG        0x006C /* L, EEPROM Cntl & PCI Cmd Codes */
#define  CTL_RDMA          0x0000000F /* DMA Read Command */
#define  CTL_WDMA          0x000000F0 /* DMA Write Command */
#define  CTL_RMEM          0x00000F00 /* Memory Read Command */
#define  CTL_WMEM          0x0000F000 /* Memory Write Command */
#define  CTL_USER0         0x00010000 /* USER0 pin control bit */
#define  CTL_USER1         0x00020000 /* USER1 pin control bit */
#define  CTL_EE_CLK        0x01000000 /* EEPROM Clock line */
#define  CTL_EE_CS         0x02000000 /* EEPROM Chip Select */
#define  CTL_EE_W          0x04000000 /* EEPROM Write bit */
#define  CTL_EE_R          0x08000000 /* EEPROM Read bit */
#define  CTL_EECHK         0x10000000 /* EEPROM Present bit */
#define  CTL_EERLD         0x20000000 /* EEPROM Reload Register */
#define  CTL_RESET         0x40000000 /* !! Adapter Reset !! */
#define  CTL_READY         0x80000000 /* Local Init Done */

/*
 * Accesses near the end of memory can cause the PLX chip
 * to pre-fetch data off of end-of-ram.  Limit the size of
 * memory so host-side accesses cannot occur.
 */

#define PLX_PREFETCH   32

/*
 * The PCI Interface, via the PCI-9060 Chip, has up to eight (8) Mailbox
 * Registers.  The PUTS (Power-Up Test Suite) handles the board-side
 * interface/interaction using the first 4 registers.  Specifications for
 * the use of the full PUTS' command and status interface is contained
 * within a separate SBE PUTS Manual.  The Host-Side Device Driver only
 * uses a subset of the full PUTS interface.
 */


/*****************************************/
/***    MAILBOX #(-1) - MEM ACCESS STS ***/
/*****************************************/

#define MBX_STS_VALID      0x57584744 /* 'WXGD' */
#define MBX_STS_DILAV      0x44475857 /* swapped = 'DGXW' */

/*****************************************/
/***    MAILBOX #0  -  PUTS STATUS     ***/
/*****************************************/

#define MBX_STS_MASK       0x000000ff /* PUTS Status Register bits */
#define MBX_STS_TMASK      0x0000000f /* register bits for TEST number */

#define MBX_STS_PCIRESET   0x00000100 /* Host issued PCI reset request */
#define MBX_STS_BUSY       0x00000080 /* PUTS is in progress */
#define MBX_STS_ERROR      0x00000040 /* PUTS has failed */
#define MBX_STS_RESERVED   0x000000c0 /* Undefined -> status in transition.
					 We are in process of changing
					 bits; we SET Error bit before
                                         RESET of Busy bit */

#define MBX_RESERVED_5     0x00000020 /* FYI: reserved/unused bit */
#define MBX_RESERVED_4     0x00000010 /* FYI: reserved/unused bit */


/******************************************/
/***    MAILBOX #1  -  PUTS COMMANDS    ***/
/******************************************/

/*
 * Any attempt to execute an unimplement command results in the PUTS
 * interface executing a NOOP and continuing as if the offending command
 * completed normally.  Note: this supplies a simple method to interrogate
 * mailbox command processing functionality.
 */

#define MBX_CMD_MASK       0xffff0000 /* PUTS Command Register bits */

#define MBX_CMD_ABORTJ     0x85000000 /* abort and jump */
#define MBX_CMD_RESETP     0x86000000 /* reset and pause at start */
#define MBX_CMD_PAUSE      0x87000000 /* pause immediately */
#define MBX_CMD_PAUSEC     0x88000000 /* pause on completion */
#define MBX_CMD_RESUME     0x89000000 /* resume operation */
#define MBX_CMD_STEP       0x8a000000 /* single step tests */

#define MBX_CMD_BSWAP      0x8c000000 /* identify byte swap scheme */
#define MBX_CMD_BSWAP_0    0x8c000000 /* use scheme 0 */
#define MBX_CMD_BSWAP_1    0x8c000001 /* use scheme 1 */

#define MBX_CMD_SETHMS     0x8d000000 /* setup host memory access window
					 size */
#define MBX_CMD_SETHBA     0x8e000000 /* setup host memory access base
					 address */
#define MBX_CMD_MGO        0x8f000000 /* perform memory setup and continue
					 (IE. Done) */
#define MBX_CMD_NOOP       0xFF000000 /* dummy, illegal command */

/*****************************************/
/***    MAILBOX #2  -  MEMORY SIZE     ***/
/*****************************************/

#define MBX_MEMSZ_MASK     0xffff0000 /* PUTS Memory Size Register bits */

#define MBX_MEMSZ_128KB    0x00020000 /* 128 kilobyte board */
#define MBX_MEMSZ_256KB    0x00040000 /* 256 kilobyte board */
#define MBX_MEMSZ_512KB    0x00080000 /* 512 kilobyte board */
#define MBX_MEMSZ_1MB      0x00100000 /* 1 megabyte board */
#define MBX_MEMSZ_2MB      0x00200000 /* 2 megabyte board */
#define MBX_MEMSZ_4MB      0x00400000 /* 4 megabyte board */
#define MBX_MEMSZ_8MB      0x00800000 /* 8 megabyte board */
#define MBX_MEMSZ_16MB     0x01000000 /* 16 megabyte board */


/***************************************/
/***    MAILBOX #2  -  BOARD TYPE    ***/
/***************************************/

#define MBX_BTYPE_MASK          0x0000ffff /* PUTS Board Type Register */
#define MBX_BTYPE_FAMILY_MASK   0x0000ff00 /* PUTS Board Family Register */
#define MBX_BTYPE_SUBTYPE_MASK  0x000000ff /* PUTS Board Subtype */

#define MBX_BTYPE_PLX9060       0x00000100 /* PLX family type */
#define MBX_BTYPE_PLX9080       0x00000300 /* PLX wanXL100s family type */

#define MBX_BTYPE_WANXL_4       0x00000104 /* wanXL400, 4-port */
#define MBX_BTYPE_WANXL_2       0x00000102 /* wanXL200, 2-port */
#define MBX_BTYPE_WANXL_1s      0x00000301 /* wanXL100s, 1-port */
#define MBX_BTYPE_WANXL_1t      0x00000401 /* wanXL100T1, 1-port */


/*****************************************/
/***    MAILBOX #3  -  SHMQ MAILBOX    ***/
/*****************************************/

#define MBX_SMBX_MASK           0x000000ff /* PUTS SHMQ Mailbox bits */


/***************************************/
/***    GENERIC HOST-SIDE DRIVER     ***/
/***************************************/

#define MBX_ERR    0
#define MBX_OK     1

/* mailbox check routine - type of testing */
#define MBXCHK_STS      0x00    /* check for PUTS status */
#define MBXCHK_NOWAIT   0x01    /* dont care about PUTS status */

/* system allocates this many bytes for address mapping mailbox space */
#define MBX_ADDR_SPACE_360 0x80	/* wanXL100s/200/400 */
#define MBX_ADDR_MASK_360 (MBX_ADDR_SPACE_360-1)

#endif /* __PLX9060_H */
