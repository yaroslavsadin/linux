#
# Makefile for TBX library files..
#

asflags-y += -I$(srctree)/arch/metag/include/asm/tbx -mmetac=2.1 \
		-Wa,-mfpu=metac21 -mdsp
ccflags-y += -I$(srctree)/arch/metag/include/asm/tbx -mmetac=2.1

ifeq ($(CONFIG_SMP),y)
asflags-y += -DTBX_PERCPU_SP_SAVE
endif

lib-y = tbicache.o tbicore.o tbictx.o tbilogf.o \
	tbipcx.o tbiroot.o tbisoft.o tbistring.o tbitimer.o tbidefr.o

lib-$(CONFIG_META_DSP) += tbidspram.o
lib-$(CONFIG_META_FPU) += tbictxfpu.o
