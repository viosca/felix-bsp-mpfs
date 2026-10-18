/**
 * @file felix/arch/riscv64/polarfire/mpfs_hal/common/mss_legacy_defines.h
 * @brief Shadow header to hijack the Microchip SDHCI Interrupt Vector.
 */

#ifndef FELIX_SHADOW_MSS_LEGACY_DEFINES_H
#define FELIX_SHADOW_MSS_LEGACY_DEFINES_H
/* 1. Instruct GCC to continue down the -I list and include the REAL
 * mss_legacy_defines.h from the mpfs-platform directory first. */
#include_next "mpfs_hal/common/mss_legacy_defines.h"

/* 2. Now that their definitions are loaded, delete the specific alias
 * that prevents us from implementing PLIC_mmc_main_IRQHandler in our OS. */
#undef mmc_main_plic_IRQHandler

#endif /* FELIX_SHADOW_MSS_LEGACY_DEFINES_H */
