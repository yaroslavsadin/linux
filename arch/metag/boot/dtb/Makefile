dtb-y += skeleton.dtb
dtb-y += tz1090_generic.dtb
dtb-y += tz1090_01ry.dtb
dtb-y += tz1090_01sp.dtb
dtb-y += tz1090_01tt.dtb
dtb-y += tz1090_01xk.dtb

# Built-in dtb
builtindtb-y := skeleton
builtindtb-$(CONFIG_SOC_COMET) := tz1090_generic
builtindtb-$(CONFIG_COMET_BUB) := tz1090_01ry
builtindtb-$(CONFIG_TZ1090_01XX) := tz1090_01sp
builtindtb-$(CONFIG_POLARIS) := tz1090_01xk

ifeq ($(CONFIG_METAG_BUILTIN_DTB),y)
ifneq '$(CONFIG_METAG_BUILTIN_DTB_NAME)' '""'
BUILTIN_DTB := $(patsubst "%",%,$(CONFIG_METAG_BUILTIN_DTB_NAME)).dtb.o
else
BUILTIN_DTB := $(patsubst "%",%,$(builtindtb-y)).dtb.o
endif
else
BUILTIN_DTB :=
endif
obj-y += $(BUILTIN_DTB)

clean-files += $(obj)/$(dtb-y) $(obj)/*.dtb.S $(obj)/*.dtb.o

# Rule to build device tree blobs
$(obj)/%.dtb: $(src)/../dts/%.dts FORCE
	$(call if_changed_dep,dtc)

$(obj)/dtbs: $(addprefix $(obj)/, $(dtb-y))
