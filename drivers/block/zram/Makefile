# SPDX-License-Identifier: GPL-2.0-only
zram-objs-common := zcomp.o zram_drv.o
zram-objs-common-$(CONFIG_HYBRIDSWAP_CORE) += ../../moto_swap/hybridswap/hybridswap_main.o \
				../../moto_swap/hybridswap/hybridswap_eswap.o \
				../../moto_swap/hybridswap/hybridswap_swapd.o

ifeq ($(CONFIG_ZRAM_MOTO_SWAP),y)
moto_swap-y := $(zram-objs-common) $(zram-objs-common-y)
obj-$(CONFIG_ZRAM) += moto_swap.o
else
zram-y := $(zram-objs-common) $(zram-objs-common-y)
obj-$(CONFIG_ZRAM)	+=	zram.o
endif
