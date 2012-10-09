#
# Makefile for the linux Meta-specific parts of the memory manager.
#

obj-y	:= init.o fault.o ioremap.o extable.o maccess.o

obj-$(CONFIG_NUMA)	+= numa.o
obj-$(CONFIG_HIGHMEM)	+= highmem.o
obj-$(CONFIG_META_L2C)	+= l2cache.o

ifdef CONFIG_META21_MMU
obj-y 	+= mmu-meta2.o
else
obj-y 	+= mmu-meta1.o
endif

obj-$(CONFIG_HUGETLB_PAGE) += hugetlbpage.o
