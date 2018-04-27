PLATFORM_FLAVOR ?= mt8173

include core/arch/arm/cpu/cortex-armv8-0.mk

CFG_TZDRAM_START ?= 0xbe000000
CFG_TZDRAM_SIZE ?= 0x01e00000
CFG_SHMEM_START ?= 0xbfe00000
CFG_SHMEM_SIZE ?= 0x00200000

$(call force,CFG_8250_UART,y)
$(call force,CFG_GENERIC_BOOT,y)
$(call force,CFG_PM_STUBS,y)
$(call force,CFG_SECURE_TIME_SOURCE_CNTPCT,y)
$(call force,CFG_WITH_ARM_TRUSTED_FW,y)

ta-targets = ta_arm32

ifeq ($(CFG_ARM64_core),y)
$(call force,CFG_WITH_LPAE,y)
ta-targets += ta_arm64
else
$(call force,CFG_ARM32_core,y)
endif

CFG_WITH_STACK_CANARIES ?= y

ifeq ($(PLATFORM_FLAVOR),mt8173)
# 2**1 = 2 cores per cluster
$(call force,CFG_CORE_CLUSTER_SHIFT,1)
endif
