ifeq ($(CONFIG_OF),y)

# Built-in dtb
builtindtb-y		:= angel4

ifneq ($(CONFIG_ARC_BUILTIN_DTB_NAME),"")
	builtindtb-y	:= $(CONFIG_ARC_BUILTIN_DTB_NAME)
endif

obj-y	+= $(patsubst "%",%,$(builtindtb-y)).dtb.o

clean-files := *.dtb

endif
