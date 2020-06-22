#
# Makefile for the kernel ata, atapi, and ide block device drivers.
#
# 12 September 2000, Bartlomiej Zolnierkiewicz <bkz@linux-ide.org>
# Rewritten to use lists instead of if-statements.
#
# Note : at this point, these files are compiled on all systems.
# In the future, some of these should be built conditionally.
#

O_TARGET := idedriver.o

export-objs := ide-iops.o ide-taskfile.o ide-proc.o ide.o ide-probe.o ide-dma.o ide-lib.o setup-pci.o ide-io.o ide-disk.o

all-subdirs	:= arm legacy pci ppc raid
mod-subdirs	:= arm legacy pci ppc raid

obj-y		:=
obj-m		:=
ide-obj-y	:=

subdir-$(CONFIG_BLK_DEV_IDE) += legacy ppc arm raid pci

# First come modules that register themselves with the core

ifeq ($(CONFIG_BLK_DEV_IDE),y)
  obj-y		+= pci/idedriver-pci.o
endif

# Core IDE code - must come before legacy

obj-$(CONFIG_BLK_DEV_IDE)		+= ide-probe.o ide-geometry.o ide-iops.o ide-taskfile.o ide.o ide-lib.o ide-io.o ide-default.o
obj-$(CONFIG_BLK_DEV_IDEDISK)		+= ide-disk.o
obj-$(CONFIG_BLK_DEV_IDECD)		+= ide-cd.o
obj-$(CONFIG_BLK_DEV_IDETAPE)		+= ide-tape.o
obj-$(CONFIG_BLK_DEV_IDEFLOPPY)		+= ide-floppy.o

ifeq ($(CONFIG_BLK_DEV_IDEPCI),y)
obj-$(CONFIG_BLK_DEV_IDE)		+= setup-pci.o
endif
ifeq ($(CONFIG_BLK_DEV_IDEDMA_PCI),y)
obj-$(CONFIG_BLK_DEV_IDE)		+= ide-dma.o
endif
obj-$(CONFIG_BLK_DEV_ISAPNP)		+= ide-pnp.o


ifeq ($(CONFIG_BLK_DEV_IDE),y)
obj-$(CONFIG_PROC_FS)			+= ide-proc.o
endif

ifeq ($(CONFIG_BLK_DEV_IDE),y)
  obj-y		+= legacy/idedriver-legacy.o
  obj-y		+= ppc/idedriver-ppc.o
  obj-y		+= arm/idedriver-arm.o
else
  ifeq ($(CONFIG_BLK_DEV_HD_ONLY),y)
	obj-y	+= legacy/idedriver-legacy.o
  endif
endif


ifeq ($(CONFIG_BLK_DEV_IDE),y)
# RAID must be last of all
  obj-y		+= raid/idedriver-raid.o
endif

include $(TOPDIR)/Rules.make
