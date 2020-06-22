#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# Makefile for the sn2 io routines.

EXTRA_CFLAGS    := -DLITTLE_ENDIAN

O_TARGET := snio.o

export-objs := pciio.o 

obj-y				+= ml_SN_intr.o shub_intr.o shuberror.o shub.o \
				   bte_error.o pcibr/pcibr.o pic.o geo_op.o \
				   l1.o l1_command.o klconflib.o klgraph.o \
				   ml_SN_init.o ml_iograph.o module.o pciio.o \
				   xbow.o xtalk.o \
				   pci_bus_cvlink.o sgi_io_init.o shubio.o

subdir-y			+= pcibr

obj-$(CONFIG_SHUB_1_0_SPECIFIC) += efi-rtc.o

include $(TOPDIR)/Rules.make
