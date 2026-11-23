# SPDX-License-Identifier: GPL-2.0-only
KERNEL_SRC := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
VENDOR := vendor/qcom/opensource
MM_ROOT := $(KERNEL_SRC)/$(VENDOR)/mm-drivers/
WLAN_PROFILE ?= kiwi_v2
KBUILD_ARGS = O=$(KERNEL_OUT) ARCH=arm64 LLVM=$(LLVM) LLVM_IAS=1 \
	KCFLAGS=-D__ANDROID_COMMON_KERNEL__ LOCALVERSION= KERNEL_SRC=$(KERNEL_SRC)

.PHONY: help check-inputs securemsm mmrm hw-fence sync-fence ext-display synx dsp graphics rmnet-mem rmnet-sch
.DEFAULT_GOAL := help

help:
	@printf '%s\n' 'Targets: securemsm mmrm hw-fence sync-fence ext-display synx dsp graphics rmnet-mem rmnet-sch' \
	  'Set KERNEL_OUT to a prepared kernel output and LLVM to the toolchain prefix.'

check-inputs:
	@case "$(KERNEL_OUT)" in /*) ;; *) printf '%s\n' 'KERNEL_OUT must be absolute' >&2; exit 1;; esac
	@out=$$(realpath -e "$(KERNEL_OUT)") || exit 1; \
	  case "$$out/" in "$(KERNEL_SRC)/"*) printf '%s\n' 'KERNEL_OUT must be outside the kernel source tree' >&2; exit 1;; esac
	@test -n "$(KERNEL_OUT)" && test -f "$(KERNEL_OUT)/.config" && \
	  test -s "$(KERNEL_OUT)/Module.symvers" || \
	  { printf '%s\n' 'KERNEL_OUT must contain .config and Module.symvers' >&2; exit 1; }
	@grep -qx 'CONFIG_ARCH_CLIFFS=y' "$(KERNEL_OUT)/.config" && \
	  grep -qx 'CONFIG_ARCH_PINEAPPLE=y' "$(KERNEL_OUT)/.config" || \
	  { printf '%s\n' 'ctwo modules require CONFIG_ARCH_CLIFFS=y and CONFIG_ARCH_PINEAPPLE=y' >&2; exit 1; }
	@test -n "$(LLVM)" || { printf '%s\n' 'Set LLVM to the toolchain prefix' >&2; exit 1; }

securemsm: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/securemsm-kernel \
	  SSG_MODULE_ROOT=$(KERNEL_SRC)/$(VENDOR)/securemsm-kernel \
	  CONFIG_QSEECOM=n BOARD_PLATFORM=pineapple modules

mmrm: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/mmrm-driver \
	  MMRM_ROOT=$(KERNEL_SRC)/$(VENDOR)/mmrm-driver BOARD_PLATFORM=pineapple modules

hw-fence: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/mm-drivers/hw_fence \
	  MSM_HW_FENCE_ROOT=$(MM_ROOT) BOARD_PLATFORM=pineapple modules

sync-fence: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/mm-drivers/sync_fence \
	  SYNC_FENCE_ROOT=$(MM_ROOT) BOARD_PLATFORM=pineapple modules

ext-display: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/mm-drivers/msm_ext_display \
	  MSM_EXT_DISPLAY_ROOT=$(MM_ROOT) BOARD_PLATFORM=pineapple modules

synx: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/synx-kernel \
	  SYNX_ROOT=$(KERNEL_SRC)/$(VENDOR)/synx-kernel BOARD_PLATFORM=pineapple modules

dsp: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/dsp-kernel \
	  DSP_ROOT=$(KERNEL_SRC)/$(VENDOR)/dsp-kernel BOARD_PLATFORM=pineapple modules

graphics: hw-fence
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/graphics-kernel \
	  KGSL_PATH=$(KERNEL_SRC)/$(VENDOR)/graphics-kernel CONFIG_QCOM_KGSL=m \
	  BOARD_PLATFORM=pineapple \
	  KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/mm-drivers/hw_fence/Module.symvers modules

rmnet-mem: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet-ext/mem modules

rmnet-sch: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet-ext/sch modules

.PHONY: b2-bm-adsp-ulog
b2-bm-adsp-ulog: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/power/bm_adsp_ulog modules

.PHONY: b2-goodix-fod
b2-goodix-fod: b2-mmi-relay
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/input/misc/goodix_fod_mmi \
	  CONFIG_MMI_RELAY=m KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/mmi_relay/Module.symvers modules

.PHONY: b2-mmi-annotate
b2-mmi-annotate: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/mmi_annotate modules

.PHONY: b2-mmi-info
b2-mmi-info: b2-mmi-annotate
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/mmi_info KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/mmi_annotate/Module.symvers modules

.PHONY: b2-mmi-relay
b2-mmi-relay: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/mmi_relay modules

.PHONY: b2-mmi-sys-temp
b2-mmi-sys-temp: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/misc/mmi_sys_temp modules

.PHONY: b2-moto-usbnet
b2-moto-usbnet: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/moto_f_usbnet modules

.PHONY: b2-moto-reboot
b2-moto-reboot: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/moto_reboot_reason modules

.PHONY: b2-moto-sched check-moto-sched
check-moto-sched: check-inputs
	@grep -qx 'CONFIG_SCHED_MOTO_UNFAIR=y' "$(KERNEL_OUT)/.config" && \
	  grep -Eq '^CONFIG_SCHED_WALT=(y|m)$$' "$(KERNEL_OUT)/.config" || \
	  { printf '%s\n' 'Moto Scheduler requires CONFIG_SCHED_MOTO_UNFAIR=y and CONFIG_SCHED_WALT=y or m' >&2; exit 1; }
	@for sym in set_moto_sched_ops set_moto_sched_enabled get_moto_sched_enabled walt_moto_uclamp_ready; do \
	  awk -v sym="$$sym" '$$2 == sym { found = 1 } END { exit !found }' "$(KERNEL_OUT)/Module.symvers" || \
	    { printf 'Missing WALT export %s in kernel Module.symvers\n' "$$sym" >&2; exit 1; }; \
	done

b2-moto-sched: check-moto-sched
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/moto_sched modules

.PHONY: b2-sensors-class
b2-sensors-class: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/sensors modules

.PHONY: b2-st54spi
b2-st54spi: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/ese/st54spi_gpio modules

.PHONY: b2-stmvl53l3
b2-stmvl53l3: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/input/misc/vl53L1_14_1_2 modules

.PHONY: b2-watchdogtest
b2-watchdogtest: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/watchdogtest modules

.PHONY: b2-wl2868c
b2-wl2868c: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/regulator/wl2868c modules

.PHONY: b2-ubwcp
b2-ubwcp: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=vendor/qcom/opensource/mm-sys-kernel/ubwcp modules

.PHONY: b2-camera
b2-camera: mmrm synx securemsm
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/camera-kernel \
	  CAMERA_KERNEL_ROOT=$(KERNEL_SRC)/$(VENDOR)/camera-kernel KERNEL_ROOT=$(KERNEL_SRC) MODNAME=camera TARGET_SYNX_ENABLE=y BOARD_PLATFORM=pineapple \
	  'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/mmrm-driver/Module.symvers $(KERNEL_OUT)/$(VENDOR)/synx-kernel/Module.symvers $(KERNEL_OUT)/$(VENDOR)/securemsm-kernel/Module.symvers' modules

.PHONY: b2-display
b2-display: mmrm hw-fence sync-fence ext-display securemsm
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/display-drivers \
	  DISPLAY_ROOT=$(KERNEL_SRC)/$(VENDOR)/display-drivers KERNEL_ROOT=$(KERNEL_SRC) CONFIG_DRM_MSM=m MODNAME=msm_drm BOARD_PLATFORM=pineapple \
	  'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/mmrm-driver/Module.symvers $(KERNEL_OUT)/$(VENDOR)/mm-drivers/hw_fence/Module.symvers $(KERNEL_OUT)/$(VENDOR)/mm-drivers/sync_fence/Module.symvers $(KERNEL_OUT)/$(VENDOR)/mm-drivers/msm_ext_display/Module.symvers $(KERNEL_OUT)/$(VENDOR)/securemsm-kernel/Module.symvers' modules

.PHONY: b2-dataipa
b2-dataipa: rmnet-mem
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/dataipa CONFIG_IPA_KERNEL_TESTS_MODULE=n \
	  KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/datarmnet-ext/mem/Module.symvers modules

.PHONY: b3-bluetooth
b3-bluetooth: securemsm wlan-platform
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/bt-kernel BT_ROOT=$(KERNEL_SRC)/$(VENDOR)/bt-kernel BT_KERNEL_ROOT=$(KERNEL_SRC)/$(VENDOR)/bt-kernel CONFIG_MSM_BT_POWER=m CONFIG_BTFM_SLIM=m CONFIG_BT_HW_SECURE_DISABLE=y BOARD_PLATFORM=pineapple 'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/securemsm-kernel/Module.symvers $(KERNEL_OUT)/$(VENDOR)/wlan/platform/Module.symvers' modules

.PHONY: b3-video
b3-video: mmrm hw-fence
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/video-driver/msm_video VIDEO_ROOT=$(KERNEL_SRC)/$(VENDOR)/video-driver CONFIG_MSM_VIDC_V4L2=m 'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/mmrm-driver/Module.symvers $(KERNEL_OUT)/$(VENDOR)/mm-drivers/hw_fence/Module.symvers' modules

.PHONY: b3-eva
b3-eva: mmrm synx dsp
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/eva-kernel EVA_ROOT=$(KERNEL_SRC)/$(VENDOR)/eva-kernel BOARD_PLATFORM=pineapple 'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/mmrm-driver/Module.symvers $(KERNEL_OUT)/$(VENDOR)/synx-kernel/Module.symvers $(KERNEL_OUT)/$(VENDOR)/dsp-kernel/Module.symvers' modules

.PHONY: b3-rmnet-core
b3-rmnet-core: b2-dataipa rmnet-mem
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet/core 'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/dataipa/Module.symvers $(KERNEL_OUT)/$(VENDOR)/datarmnet-ext/mem/Module.symvers' modules

.PHONY: b3-rmnet-aps
b3-rmnet-aps: b3-rmnet-core
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet-ext/aps RMNET_CORE_INC_DIR=$(KERNEL_SRC)/$(VENDOR)/datarmnet/core KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/datarmnet/core/Module.symvers modules

.PHONY: b3-rmnet-offload
b3-rmnet-offload: b3-rmnet-core
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet-ext/offload RMNET_CORE_INC_DIR=$(KERNEL_SRC)/$(VENDOR)/datarmnet/core KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/datarmnet/core/Module.symvers modules

.PHONY: b3-rmnet-perf
b3-rmnet-perf: b3-rmnet-core b3-rmnet-shs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet-ext/perf RMNET_CORE_INC_DIR=$(KERNEL_SRC)/$(VENDOR)/datarmnet/core 'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/datarmnet/core/Module.symvers $(KERNEL_OUT)/$(VENDOR)/datarmnet-ext/shs/Module.symvers' modules

.PHONY: b3-rmnet-perf-tether
b3-rmnet-perf-tether: b3-rmnet-core
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet-ext/perf_tether RMNET_CORE_INC_DIR=$(KERNEL_SRC)/$(VENDOR)/datarmnet/core KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/datarmnet/core/Module.symvers modules

.PHONY: b3-rmnet-shs
b3-rmnet-shs: b3-rmnet-core
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet-ext/shs RMNET_CORE_INC_DIR=$(KERNEL_SRC)/$(VENDOR)/datarmnet/core KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/datarmnet/core/Module.symvers modules

.PHONY: b3-rmnet-wlan
b3-rmnet-wlan: b3-rmnet-core
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/datarmnet-ext/wlan RMNET_CORE_INC_DIR=$(KERNEL_SRC)/$(VENDOR)/datarmnet/core KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/datarmnet/core/Module.symvers modules

.PHONY: b3-mmi-charger
b3-mmi-charger: b2-mmi-info
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/power/mmi_charger KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/mmi_info/Module.symvers modules

.PHONY: b3-qti-charger
b3-qti-charger: b3-mmi-charger b2-bm-adsp-ulog
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/power/qti_glink_charger 'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/power/mmi_charger/Module.symvers $(KERNEL_OUT)/drivers/power/bm_adsp_ulog/Module.symvers' modules

.PHONY: b3-adaptive-charge
b3-adaptive-charge: b3-mmi-charger
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/power/qpnp_adaptive_charge CONFIG_USE_MMI_CHARGER=y KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/power/mmi_charger/Module.symvers modules

.PHONY: b3-lpd-mitigate
b3-lpd-mitigate: b3-qti-charger b2-mmi-relay
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/power/mmi_lpd_mitigate 'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/power/qti_glink_charger/Module.symvers $(KERNEL_OUT)/drivers/mmi_relay/Module.symvers' modules

.PHONY: b3-con-dfpar
b3-con-dfpar: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/moto_con_dfpar modules

.PHONY: b3-mmap-fault
b3-mmap-fault: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/moto_mmap_fault modules

.PHONY: b3-utags
b3-utags: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/misc/utag modules

.PHONY: b5-audio
b5-audio: ext-display
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/audio-kernel \
	  AUDIO_ROOT=$(KERNEL_SRC)/$(VENDOR)/audio-kernel MODNAME=audio_dlkm BOARD_PLATFORM=pineapple \
	  CONFIG_SND_SOC_PINEAPPLE=m CONFIG_SND_SOC_WSA884X=m CONFIG_SND_SOC_WSA883X=m \
	  CONFIG_SND_SOC_WCD9XXX_V2=m CONFIG_SND_SOC_WCD937X=m CONFIG_SND_SOC_WCD937X_SLAVE=m \
	  CONFIG_SND_SOC_WCD938X=m CONFIG_SND_SOC_WCD938X_SLAVE=m CONFIG_SND_SOC_WCD939X=m CONFIG_SND_SOC_WCD939X_SLAVE=m \
	  CONFIG_SND_SOC_WCD_MBHC=m CONFIG_SND_SWR_HAPTICS=m CONFIG_SND_SOC_MSM_STUB=m \
	  CONFIG_SND_SOC_MSM_HDMI_CODEC_RX=m CONFIG_SND_SOC_SWR_DMIC=m \
	  KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/mm-drivers/msm_ext_display/Module.symvers modules

.PHONY: b5-touchscreen b5-goodix b5-st21nfc b5-sx937x check-panel-notifier
check-panel-notifier: check-inputs
	@grep -Eq '^CONFIG_QCOM_PANEL_EVENT_NOTIFIER=(y|m)$$' "$(KERNEL_OUT)/.config" || \
	  { printf '%s\n' 'Touchscreen requires CONFIG_QCOM_PANEL_EVENT_NOTIFIER=y or m' >&2; exit 1; }

b5-touchscreen: check-panel-notifier b2-sensors-class b2-mmi-relay
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/input/touchscreen/touchscreen_mmi \
	  CONFIG_DRM_PANEL_EVENT_NOTIFICATIONS=m \
	  'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/sensors/Module.symvers $(KERNEL_OUT)/drivers/mmi_relay/Module.symvers' modules

b5-goodix: b5-touchscreen b2-mmi-info
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/input/touchscreen/goodix_berlin_mmi \
	  TARGET_PRODUCT=ctwo CONFIG_DRM_PANEL_EVENT_NOTIFICATIONS=m CONFIG_INPUT_TOUCHSCREEN_MMI=m CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI=y CONFIG_GTP_FOD=y \
	  'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/mmi_info/Module.symvers $(KERNEL_OUT)/drivers/input/touchscreen/touchscreen_mmi/Module.symvers' modules

b5-st21nfc: b2-mmi-info
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/nfc/st21nfc TARGET_PRODUCT=ctwo \
	  KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/mmi_info/Module.symvers modules

b5-sx937x: b2-sensors-class
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/misc/sx937x_multi \
	  KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/sensors/Module.symvers modules

.PHONY: b5-qm35
b5-qm35: b2-mmi-info
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=drivers/uwb/qm35 \
	  KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/drivers/mmi_info/Module.symvers modules

.PHONY: nxp-nci
nxp-nci: securemsm
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=vendor/nxp/opensource/driver \
	  NFC_ROOT=$(KERNEL_SRC)/vendor/nxp/opensource/driver \
	  'KCFLAGS=-D__ANDROID_COMMON_KERNEL__ -DNFC_SECURE_PERIPHERAL_ENABLED' \
	  KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/securemsm-kernel/Module.symvers modules

.PHONY: wlan-platform wlan
wlan-platform: check-inputs
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/wlan/platform \
	  WLAN_PLATFORM_ROOT=$(KERNEL_SRC)/$(VENDOR)/wlan/platform BOARD_PLATFORM=pineapple modules

wlan: wlan-platform b2-dataipa
	@test -n "$(WLAN_PROFILE)" && test -f "$(KERNEL_SRC)/$(VENDOR)/wlan/qcacld-3.0/configs/pineapple_gki_$(WLAN_PROFILE)_defconfig" || \
	  { printf '%s\n' 'Set WLAN_PROFILE to the verified device profile' >&2; exit 1; }
	+$(MAKE) -C $(KERNEL_SRC) $(KBUILD_ARGS) M=$(VENDOR)/wlan/qcacld-3.0 \
	  MODNAME=qca_cld3_kiwi_v2 DEVNAME=kiwi_v2 DYNAMIC_SINGLE_CHIP=kiwi_v2 \
	  CONFIG_QCA_CLD_WLAN=m CONFIG_IPA_OUT_OF_TREE=y WLAN_ROOT=$(KERNEL_SRC)/$(VENDOR)/wlan/qcacld-3.0 \
	  WLAN_COMMON_INC=$(KERNEL_SRC)/$(VENDOR)/wlan/qca-wifi-host-cmn \
	  WLAN_FW_API=$(KERNEL_SRC)/$(VENDOR)/wlan/fw-api WLAN_PROFILE=$(WLAN_PROFILE) \
	  'KBUILD_EXTRA_SYMBOLS=$(KERNEL_OUT)/$(VENDOR)/dataipa/Module.symvers $(KERNEL_OUT)/$(VENDOR)/wlan/platform/Module.symvers' modules
