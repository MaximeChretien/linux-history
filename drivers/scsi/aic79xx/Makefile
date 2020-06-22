#
# drivers/scsi/aic79xx/Makefile
#
# Makefile for the Linux aic79xx SCSI driver.
#

O_TARGET := aic79xx_drv.o

list-multi	:= aic79xx.o

obj-$(CONFIG_SCSI_AIC79XX)	+= aic79xx.o

EXTRA_CFLAGS += -I..
#EXTRA_CFLAGS += -g

# Platform Specific U320 Files
obj-aic79xx = aic79xx_osm.o aic79xx_proc.o aic79xx_osm_pci.o
# Core Files
obj-aic79xx += aic79xx_core.o aic79xx_pci.o
ifeq ($(CONFIG_AIC79XX_REG_PRETTY_PRINT),y)
obj-aic79xx += aic79xx_reg_print.o
endif

# Override our module desitnation
MOD_DESTDIR = $(shell cd .. && $(CONFIG_SHELL) $(TOPDIR)/scripts/pathdown.sh)

include $(TOPDIR)/Rules.make

aic79xx_core.o: aic79xx_seq.h
$(obj-aic79xx): aic79xx_reg.h
aic79xx.o: aic79xx_seq.h aic79xx_reg.h $(obj-aic79xx)
	$(LD) $(LD_RFLAG) -r -o $@ $(obj-aic79xx)

ifeq ($(CONFIG_AIC79XX_BUILD_FIRMWARE),y)
aic79xx_gen = aic79xx_seq.h aic79xx_reg.h
ifeq ($(CONFIG_AIC79XX_REG_PRETTY_PRINT),y)
aic79xx_gen += aic79xx_reg_print.c
aic79xx_asm_cmd = aicasm/aicasm -I. -r aic79xx_reg.h		\
		 -p aic79xx_reg_print.c -i aic79xx_osm.h	\
		 -o aic79xx_seq.h aic79xx.seq
else
aic79xx_asm_cmd = aicasm/aicasm -I. -r aic79xx_reg.h \
		 -o aic79xx_seq.h aic79xx.seq
endif
$(aic79xx_gen): aic79xx.seq aic79xx.reg aicasm/aicasm
	$(aic79xx_asm_cmd)
endif

aicasm/aicasm: aicasm/*.[chyl]
	$(MAKE) -C aicasm
