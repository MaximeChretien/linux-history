/*
 *   $Id: pci-sd0001.h,v 1.1.2.1 2003/06/24 08:40:50 dwmw2 Exp $
 *
 *   linux/arch/sh/kernel/pci_sd0001.h
 *
 *   Support Hitachi Semcon SD0001 SH3 PCI Host Bridge .
 *  
 *
 *   Copyright (C) 2000  Masayuki Okada (macha@adc.hitachi-ul.co.jp)
 *                       Hitachi ULSI Systems Co., Ltd.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *
 *   Revision History
 *    ----------------
 *
 */

/*
 * SD0001 PCI�֥�å����쥸�������ӥå����
 */


#ifndef __PCI_SD0001_H
#define __PCI_SD0001_H

#define SD0001_IO_BASE (P2SEGADDR(CONFIG_PCI_SD0001_BASE)+0x00800000)
#define SD0001_MEM_BASE (P2SEGADDR(CONFIG_PCI_SD0001_BASE)+0x01000000)

#define SD0001_REG(x) ((volatile u32 *)P2SEGADDR(CONFIG_PCI_SD0001_BASE + (x)))

#define sd0001_writel(value, reg) do { *(SD0001_REG(SD0001_REG_##reg)) = value; } while(0)
#define sd0001_readl(reg) (*(SD0001_REG(SD0001_REG_##reg)))

#define SD0001_REG_REV			(0x00)	/* PCI Class & Revision Code */
#define SD0001_REG_RESET		(0x08)	/* �ꥻ�å� */
#define SD0001_REG_SDRAM_CTL		(0x10)	/* SDRAM �⡼��/���� */
#define SD0001_REG_INT_STS1		(0x20)	/* �����װ�ɽ�� */
#define SD0001_REG_INT_ENABLE		(0x24)	/* �����ߥޥ��� */
#define SD0001_REG_INT_STS2		(0x28)	/* ����ߥ��ơ����� */
#define SD0001_REG_DMA1_CTL_STS		(0x30)	/* DMA ���ޥ�� & ���ơ����� */
#define SD0001_REG_DMA1_SADR		(0x34)	/* DMA ���������ɥ쥹 */
#define SD0001_REG_DMA1_DADR		(0x38)	/* DMA �ǥ����ƥ��͡�����󥢥ɥ쥹 */
#define SD0001_REG_DMA1_CNT		(0x3c)	/* DMA ž���Х��ȿ� */
#define SD0001_REG_DMA2_CTL_STS		(0x40)	/* DMA ���ޥ�� & ���ơ����� */
#define SD0001_REG_DMA2_SADR		(0x44)	/* DMA ���������ɥ쥹 */
#define SD0001_REG_DMA2_DADR		(0x48)	/* DMA �ǥ����ƥ��͡�����󥢥ɥ쥹 */
#define SD0001_REG_DMA2_CNT		(0x4c)	/* DMA ž���Х��ȿ� */
#define SD0001_REG_PCI_CTL		(0x50)	/* PCI�Х�ư��⡼�� */
#define SD0001_REG_PCI_IO_OFFSET	(0x58)	/* PCIľ��I/O�����������ե��å� */
#define SD0001_REG_PCI_MEM_OFFSET	(0x5c)	/* PCIľ�ܥ��ꥢ���������ե��å� */
#define SD0001_REG_INDIRECT_ADR		(0x60)	/* PCI Configuration�쥸�������ɥ쥹 */
#define SD0001_REG_INDIRECT_DATA	(0x64)	/* PCI Configuration�ǡ����쥸���� */
#define SD0001_REG_INDIRECT_CTL		(0x68)	/* PCI�Х����ܥ����������� */
#define SD0001_REG_INDIRECT_STS		(0x6c)	/* PCI�Х����ܥ����������ơ����� */
#define SD0001_REG_AWAKE		(0x70)	/* AWAKE����� */
#define SD0001_REG_MAIL			(0x74)	/* Mail�̿� */


/*
 * SD0001 �쥸�����γƥӥåȤε�ǽ�����
 */
/* MODE �쥸���� */

/* RST �쥸���� */
#define	 SD0001_RST_SWRST		0x80000000	/* SD0001�Υꥻ�å� */
#define	 SD0001_RST_BUSRST		0x40000000	/* PCI�Х����եȥꥻ�å� */
#define	 SD0001_RST_MASK		0xc0000000	/* RST�쥸��������ޥ��� */


/* SH3 SD0001  PCI Devices Space & Mamory Size */
#define	 SD0001_PCI_IO_WINDOW		0x00800000	/* PCI I/O���� Window: 8MiB */
#define	 SD0001_PCI_MEM_WINDOW		0x03000000	/* PCI MEM���� Window: 48MiB */
#define  SD0001_PCI_WINDOW_SHIFT	22		
#define	 SD0001_SDRAM_MAX		0x04000000	/* SDMAN������ 64MiB */


/* pci_ctl �쥸���� */
#define  SD0001_CTL_MASTER_SWAP		0x80000000	/* PCI�Х��ޥ�����ž������
							   Byte Swap ON */
#define  SD0001_CTL_PCI_EDCONV		0x40000000	/* PCI�ǥХ���������������
							   Endian�Ѵ� ON */
#define	 SD0001_CTL_RETRY_MASK		0x000000f0	/* PCI��ȥ饤�����и��Х�������� */
#define  SD0001_CTL_NOGNT		0x00000002	/* PCI GNT�����ȯ����� */

/* SD_MDCTL �쥸���� */
#define	 SD0001_SDMD_KIND_MASK		0xc0000000	/* SDRAM ���� ����ޥ���*/
#define	 SD0001_SDMD_KIND_16		0x00000000	/* SDRAM 16Mbits */
#define	 SD0001_SDMD_KIND_64		0x40000000	/* SDRAM 64Mbits */
#define	 SD0001_SDMD_KIND_128		0x80000000	/* SDRAM 128Mbits */
#define	 SD0001_SDMD_KIND_256		0xc0000000	/* SDRAM 256Mbits */
#define	 SD0001_SDMD_SIZE_MASK		0x30000000	/* SDRAM SIZE ����ޥ��� */
#define	 SD0001_SDMD_SIZE_4		0x00000000	/* �ǡ����Х� 4bits */
#define	 SD0001_SDMD_SIZE_8		0x10000000	/* �ǡ����Х� 8bits */
#define	 SD0001_SDMD_SIZE_16		0x20000000	/* �ǡ����Х� 16bits */
#define	 SD0001_SDMD_SIZE_32		0x30000000	/* �ǡ����Х� 32bits */
#define	 SD0001_SDMD_REF_MASK		0x0000f000	/* ��ե�å��塦�������� ����ޥ��� */
#define	 SD0001_SDMD_REF_DEF		0x00000000	/* ��ե�å��塦�������� �ǥե����(128cycles) */
#define	 SD0001_SDMD_REF_128		0x00001000	/* ��ե�å��塦�������� 128cycles */
#define	 SD0001_SDMD_REF_256		0x00002000	/* ��ե�å��塦�������� 256cycles */
#define	 SD0001_SDMD_REF_384		0x00003000	/* ��ե�å��塦�������� 384cycles */
#define	 SD0001_SDMD_REF_512		0x00004000	/* ��ե�å��塦�������� 512cycles */
#define	 SD0001_SDMD_REF_640		0x00005000	/* ��ե�å��塦�������� 640cycles */
#define	 SD0001_SDMD_REF_768		0x00006000	/* ��ե�å��塦�������� 768cycles */
#define	 SD0001_SDMD_REF_896		0x00007000	/* ��ե�å��塦�������� 896cycles */
#define	 SD0001_SDMD_REF_STOP		0x00008000	/* ��ե�å������ */

#define	 SD0001_SDMD_LMODE_MASK		0x00000070	/* CAS�쥤�ƥ� ����ޥ��� */
#define  SD0001_SDMD_LMODE_1		0x00000000	/* CAS�쥤�ƥ� 1 */
#define  SD0001_SDMD_LMODE_2		0x00000010	/* CAS�쥤�ƥ� 2 */
#define  SD0001_SDMD_LMODE_3		0x00000020	/* CAS�쥤�ƥ� 3 */
#define  SD0001_SDMD_MASK		0xf000f070	/* SDMS ����ޥ��� */


/* ������ �ޥ��� */
#define	 SD0001_INT_INTEN		0x80000000	/* �������ߥޥ��� */
#define	 SD0001_INT_RETRY		0x20000000	/* PCI�Х���ȥ饤��������С� */
#define	 SD0001_INT_TO			0x10000000	/* PCI�Х������ॢ���� */
#define	 SD0001_INT_SSERR		0x08000000	/* SERR#�������� */
#define	 SD0001_INT_RSERR		0x04000000	/* SERR#���� */
#define	 SD0001_INT_RPERR		0x02000000	/* PERR#���� */
#define	 SD0001_INT_SPERR		0x01000000	/* PERR#�������� */
#define	 SD0001_INT_STABT		0x00800000	/* �������åȥ��ܡ���ȯ�� */
#define	 SD0001_INT_RTABT		0x00400000	/* �������åȥ��ܡ��ȸ��� */
#define	 SD0001_INT_LOCK		0x00200000	/* �ǥåɥ�å����� */
#define	 SD0001_INT_RMABT		0x00100000	/* �ޥ��������ܡ��ȸ��� */
#define	 SD0001_INT_BUSERR		0x3ff00000	/* PCI�Х����顼����� */
#define	 SD0001_INT_AWINT		0x00001000	/* AWAKE����� */
#define	 SD0001_INT_DMA1		0x00000020	/* DMA����ͥ�1��λ */
#define	 SD0001_INT_DMA2		0x00000010	/* DMA����ͥ�2��λ */
#define	 SD0001_INT_INTD		0x00000008	/* PCI�Х�INTD# */
#define	 SD0001_INT_INTC		0x00000004	/* PCI�Х�INTC# */
#define	 SD0001_INT_INTB		0x00000002	/* PCI�Х�INTB# */
#define	 SD0001_INT_INTA		0x00000001	/* PCI�Х�INTA# */
#define	 SD0001_INT_VAILD		0x3ff0103f	/* ������ͭ���ӥå� */

/* CONFIG_ADDRESS �쥸���� */
#define	 SD0001_CONFIG_ADDR_EN		0x80000000	/* ����ե����졼����󥵥����륤�͡��֥� */


/* INDRCT_CMD �쥸���� */
#define	 SD0001_INDRCTC_BE_MASK		0x000f0000	/* �Х��ȥ��͡��֥� */
#define	 SD0001_INDRCTC_BE_BYTE		0x00010000	/* �Х��ȥ����������ϰ��� */
#define	 SD0001_INDRCTC_BE_WORD		0x00030000	/* ��ɥ����������ϰ��� */
#define	 SD0001_INDRCTC_BE_LONG		0x000f0000	/* ��󥰥�ɥ������� */
#define	 SD0001_INDRCTC_CMDEN		0x00008000	/* CMD Enable */
#define	 SD0001_INDRCTC_CMD_IOR		0x00000200	/* I/O Read CMD */
#define	 SD0001_INDRCTC_CMD_IOW		0x00000300	/* I/O Write CMD */
#define	 SD0001_INDRCTC_CMD_MEMR	0x00000600	/* Memory Read CMD */
#define	 SD0001_INDRCTC_CMD_MEMW	0x00000700	/* Memory Write CMD */
#define	 SD0001_INDRCTC_CMD_INTA	0x00000000	/* Interrupt Ack CMD */
#define	 SD0001_INDRCTC_CMD_MASK	0x00000f00	/* CMD ������ */
#define	 SD0001_INDRCTC_FLGRESET	0x00000080	/* INDRCT_FLG�Υ��顼�ե饰�Υꥻ�å� */
#define	 SD0001_INDRCTC_IOWT		0x00000008	/* ����I/O�饤�Ȼؼ� */
#define	 SD0001_INDRCTC_IORD		0x00000004	/* ����I/O�꡼�ɻؼ� */
#define	 SD0001_INDRCTC_COWT		0x00000002	/* ����ե����졼�����饤�Ȼؼ� */
#define	 SD0001_INDRCTC_CORD		0x00000001	/* ����ե����졼�����꡼�ɻؼ� */
#define	 SD0001_INDRCTC_MASK		0x000f8f8f	/* INDRCT_CMD ������ޥ��� */

/* INDRCT_FLG �쥸���� */
#define	 SD0001_INDRCTF_MABTRCV		0x00080000	/* �ޥ��������ܡ���ȯ�� */
#define	 SD0001_INDRCTF_INDFLG		0x00000001	/* ���ܥ��������¹��� */


/* Awake �쥸���� */
#define	 SD0001_AWAKE_AWOK		0x80000000	/* AWAKE READɽ�� */
#define	 SD0001_AWAKE_AWV		0x7fffffff	/* AWAKE READ�տ���� */

/* Mail �쥸���� */
#define	 SD0001_MAIL_FLAG		0x80000000	/* MAIL �����ȯ�� */
#define	 SD0001_MAIL_DATA		0x7fffffff	/* MAIL ������տ���� */


/*
 * SD0001 ��¢ DMAC �쥸������ǽ���
 */

/* DMATCR �쥸���� */
#define	 SD0001_DMATCR_MASK		0x0fffffff	/* DMATCR����ޥ��� */
#define	 SD0001_DMATCR_MAX		0x04000000	/* ����ž���Х��ȿ� */



/* DMCMD �쥸���� */
#define	 SD0001_DMCMD_EXEC		0x80000000	/* DMA���ϻؼ� / �¹���ɽ�� */
#define	 SD0001_DMCMD_NEND		0x40000000	/* DMA�����ˤ�봰λ */
#define	 SD0001_DMCMD_AEND		0x20000000	/* DMA���Ԥˤ�봰λ */
#define	 SD0001_DMCMD_STATUS		0x60000000	/* DMA��λ���ơ����� */

#define  SD0001_DMCMD_SWAP		0x00010000	/* ž���ǡ����� Byte Swap */
#define	 SD0001_DMCMD_DSR_RAM_PCI	0x00000100	/* DMAž�������� SDRAM �� PCI */
#define	 SD0001_DMCMD_DSR_PCI_RAM	0x00000200	/* DMAž�������� PCI �� SDRAM */
#define	 SD0001_DMCMD_DSR_RAM_RAM	0x00000000	/* DMAž�������� SDRAM �� SDRAM */
#define	 SD0001_DMCMD_MASK		0xe0010f00	/* DMCMD�쥸��������ޥ��� */


/* INTM PCI �쥸���� */
#define  SD0001_INTMPCI_PERR		0x80000000	/* Detected Perr ����ߥޥ��� */
#define  SD0001_INTMPCI_SERR		0x40000000	/* Signalled SERR ����ߥޥ��� */
#define  SD0001_INTMPCI_MBAT		0x20000000	/* Received Master Abort ����ߥޥ��� */
#define  SD0001_INTMPCI_RTABT		0x10000000	/* Received Target Abort ����ߥޥ��� */
#define  SD0001_INTMPCI_STABT		0x08000000	/* Signalled Target Abort ����ߥޥ��� */
#define  SD0001_INTMPCI_DPERR		0x01000000	/* DPerr Detected ����ߥޥ��� */
#define  SD0001_INTMPCI_MAIL		0x00800000	/* MAIL�쥸��������� */


/* RESET PCI �쥸����*/
#define  SD0001_RSTPCI_SWRST		0x80000000	/* �������åȥǥХ��� �ꥻ�å� */
#define  SD0001_RSTPCI_ALIVE		0x40000000	/* �������åȥ⡼�ɤλ���RESET�쥸������ */
							/*    bit30(PCIRST)���͡� Read Only */


extern int pci_setup_sd0001(void);

void sd0001_outl(unsigned long, unsigned long); 
unsigned long sd0001_inl(unsigned long);
void sd0001_outw(unsigned short, unsigned long);
unsigned short sd0001_inw(unsigned long);
void sd0001_outb(unsigned char, unsigned long);
unsigned char sd0001_inb(unsigned long);
void sd0001_insb(unsigned long, void *, unsigned long);
void sd0001_insw(unsigned long, void *, unsigned long);
void sd0001_insl(unsigned long, void *, unsigned long);
void sd0001_outsb(unsigned long, const void *, unsigned long);
void sd0001_outsw(unsigned long, const void *, unsigned long);
void sd0001_outsl(unsigned long, const void *, unsigned long);
unsigned char sd0001_inb_p(unsigned long);
void sd0001_outb_p(unsigned char, unsigned long);

void *sd0001_ioremap(unsigned long, unsigned long);
void sd0001_iounmap(void *);


#endif /* __PCI_SD0001_H */
