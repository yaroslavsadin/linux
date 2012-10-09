#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2007,2012  Imagination Technologies Ltd.
#

suffix-y := bin
suffix-$(CONFIG_KERNEL_GZIP)	:= gz
suffix-$(CONFIG_KERNEL_BZIP2)	:= bz2
suffix-$(CONFIG_KERNEL_LZMA)	:= lzma
suffix-$(CONFIG_KERNEL_XZ)	:= xz
suffix-$(CONFIG_KERNEL_LZO)	:= lzo

targets := uImage uImage.gz uImage.bz2 uImage.lzma uImage.xz uImage.lzo \
	   uImage.bin
extra-y += vmlinux.bin vmlinux.bin.gz vmlinux.bin.bz2 vmlinux.bin.lzma \
	   vmlinux.bin.xz vmlinux.bin.lzo

UIMAGE_LOADADDR = $(CONFIG_PAGE_OFFSET)

VMLINUX = vmlinux

ifeq ($(CONFIG_FUNCTION_TRACER),y)
ORIG_CFLAGS := $(KBUILD_CFLAGS)
KBUILD_CFLAGS = $(subst -pg, , $(ORIG_CFLAGS))
endif

$(obj)/vmlinux.bin: $(VMLINUX)
	$(Q)$(OBJCOPY) -O binary $(strip-flags) $(VMLINUX) $(obj)/vmlinux.bin

$(obj)/vmlinux.bin.gz: $(obj)/vmlinux.bin FORCE
	$(call if_changed,gzip)

$(obj)/vmlinux.bin.bz2: $(obj)/vmlinux.bin FORCE
	$(call if_changed,bzip2)

$(obj)/vmlinux.bin.lzma: $(obj)/vmlinux.bin FORCE
	$(call if_changed,lzma)

$(obj)/vmlinux.bin.xz: $(obj)/vmlinux.bin FORCE
	$(call if_changed,xzkern)

$(obj)/vmlinux.bin.lzo: $(obj)/vmlinux.bin FORCE
	$(call if_changed,lzo)

$(obj)/uImage.bz2: $(obj)/vmlinux.bin.bz2
	$(call if_changed,uimage,bzip2)

$(obj)/uImage.gz: $(obj)/vmlinux.bin.gz
	$(call if_changed,uimage,gzip)

$(obj)/uImage.lzma: $(obj)/vmlinux.bin.lzma
	$(call if_changed,uimage,lzma)

$(obj)/uImage.xz: $(obj)/vmlinux.bin.xz
	$(call if_changed,uimage,xz)

$(obj)/uImage.lzo: $(obj)/vmlinux.bin.lzo
	$(call if_changed,uimage,lzo)

$(obj)/uImage.bin: $(obj)/vmlinux.bin
	$(call if_changed,uimage,none)

$(obj)/uImage: $(obj)/uImage.$(suffix-y)
	@ln -sf $(notdir $<) $@
	@echo '  Image $@ is ready'

clean-files += vmlinux.bin

export suffix-y
