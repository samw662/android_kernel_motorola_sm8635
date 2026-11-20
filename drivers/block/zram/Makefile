# SPDX-License-Identifier: GPL-2.0-only
zram-y	:=	zcomp.o zram_drv.o
zram-$(CONFIG_HYBRIDSWAP_CORE) += ../../moto_swap/hybridswap/hybridswap_main.o \
				../../moto_swap/hybridswap/hybridswap_eswap.o \
				../../moto_swap/hybridswap/hybridswap_swapd.o

obj-$(CONFIG_ZRAM)	+=	zram.o
