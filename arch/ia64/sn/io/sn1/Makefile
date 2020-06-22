#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# Makefile for the sn1 io routines.

EXTRA_CFLAGS    := -DLITTLE_ENDIAN

O_TARGET := snio.o

export-objs := pciio.o

obj-$(CONFIG_IA64_SGI_SN1)	+= ml_SN_intr.o mem_refcnt.o hubcounters.o \
				   ip37.o huberror.o hub_intr.o pcibr.o \
				   eeprom.o l1.o l1_command.o pciio.o \
				   klconflib.o klgraph.o \
				   efi-rtc.o xbow.o xtalk.o \
				   ml_SN_init.o ml_iograph.o module.o \
				   pci_bus_cvlink.o sgi_io_init.o


include $(TOPDIR)/Rules.make
