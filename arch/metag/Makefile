#
# metag/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" cleaning up for this architecture.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
#               2007,2008,2012 by Imagination Technologies Ltd.
#

LDFLAGS		:=
OBJCOPYFLAGS	:= -O binary -R .note -R .comment -S

CHECKFLAGS-$(CONFIG_META12) += -DMETAC_1_2
CHECKFLAGS-$(CONFIG_META21) += -DMETAC_2_1
CHECKFLAGS	+= -D__metag__ $(CHECKFLAGS-y)

KBUILD_DEFCONFIG := meta2_defconfig

sflags-$(CONFIG_META12) += -mmetac=1.2

ifeq ($(CONFIG_META12),y)
# Only use TBI API 1.4 if DSP is enabled for META12 cores
sflags-$(CONFIG_META_DSP) += -DTBI_1_4
endif

sflags-$(CONFIG_META21) += -mmetac=2.1 -DTBI_1_4

# Default SoC .c files
score-y  :=

head-y := arch/metag/kernel/head.o

core-y					+= arch/metag/kernel/ \
					   arch/metag/mm/

ifneq ($(score-y),)
core-y                                  += arch/metag/$(score-y)/
endif

ifneq ($(machdir-y),)
core-y  += $(addprefix arch/metag/boards/, \
             $(filter-out ., $(patsubst %,%/,$(machdir-y))))
endif

libs-y 					+= arch/metag/lib/ \
					   arch/metag/tbx/

cflags-$(CONFIG_META_FUNCTION_TRACE) +=  -mhwtrace-leaf -mhwtrace-retpc

cflags-$(CONFIG_META21) += -mextensions=bex

KBUILD_CFLAGS += -pipe

KBUILD_CFLAGS += -ffunction-sections

KBUILD_CFLAGS += $(sflags-y) $(cflags-y)
KBUILD_AFLAGS += $(sflags-y)

LDFLAGS_vmlinux := $(ldflags-y)

boot := arch/metag/boot

BOOT_TARGETS = uImage uImage.bz2 uImage.gz uImage.lzma uImage.xz uImage.lzo \
	       uImage.bin vmlinux.bin
PHONY += $(BOOT_TARGETS)

all: vmlinux.bin

$(BOOT_TARGETS): vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

archclean:
	$(Q)$(MAKE) $(clean)=$(boot)

define archhelp
  echo  '* vmlinux.bin	- Binary kernel image (arch/$(ARCH)/boot/vmlinux.bin)'
  @echo '  uImage  	- Alias to bootable U-Boot image'
  @echo '  uImage.bin	- Kernel-only image for U-Boot (bin)'
  @echo '  uImage.gz	- Kernel-only image for U-Boot (gzip)'
  @echo '  uImage.bz2	- Kernel-only image for U-Boot (bzip2)'
  @echo '  uImage.lzma	- Kernel-only image for U-Boot (lzma)'
  @echo '  uImage.xz	- Kernel-only image for U-Boot (xz)'
  @echo '  uImage.lzo	- Kernel-only image for U-Boot (lzo)'
endef
