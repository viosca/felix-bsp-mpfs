/**
 * @file felix/arch/riscv64/polarfire/driver/sdcard_hal.c
 * @brief PolarFire-specific implementation of the SD Card HAL.
 * * This file implements the generic sdcard_hal.h contract using
 * the Microchip MSS MMC bare-metal drivers.
 */

#include "drivers/sdcard_hal.h"
#include "kernel/blk_manager.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h> 
#include "kernel/syscalls.h" 
#include "kernel/klog.h"

#include "mpfs_hal/mss_hal.h"
#include "mpfs_hal/common/mss_plic.h"
#include "mpfs_hal/common/mss_hart_ints.h"

/* Include Microchip's hardware drivers */
#include "drivers/mss/mss_mmc/mss_mmc.h"
#include "drivers/mss/mss_mmc/mss_mmc_regs.h" 
#include "drivers/mss/mss_gpio/mss_gpio.h"
#include "fpga_design_config/fpga_design_config.h"

static mss_mmc_cfg_t g_mmc;
static int s_card_initialized = 0;
static int s_pads_configured = 0;

int sdcard_hal_init(void) {
    /* NOTICE: GPIO Multiplexer initialization was moved to sdcard_hal_irq_enable() 
     * so the hardware Card Detect pin can reach the CPU before initialization! */

    uint8_t hart_id = (uint8_t)read_csr(mhartid);
    
    /* Ensure the MMC peripheral clock is on during initialization */
    mss_config_clk_rst(MSS_PERIPH_EMMC, hart_id, PERIPHERAL_ON);

    memset(&g_mmc, 0, sizeof(g_mmc));
    
    g_mmc.clk_rate = MSS_MMC_CLOCK_50MHZ; 
    g_mmc.card_type = MSS_MMC_CARD_TYPE_SD;
    g_mmc.data_bus_width = MSS_MMC_DATA_WIDTH_4BIT; 
    g_mmc.bus_speed_mode = MSS_SDCARD_MODE_HIGH_SPEED; 
    
    /* NOTE: We omit .bus_voltage intentionally to match HSS. The Discovery Kit 
     * handles the voltage physically via VDDI4 and the auto-sensing level shifter. 
	 */

    /* First Attempt: Initialize the MMC hardware */
    mss_mmc_status_t status = MSS_MMC_init(&g_mmc);

    /* The 50ms Wake-Up Retry Logic! */
    if (status != MSS_MMC_INIT_SUCCESS) {
        kprintf("[HAL] First init failed (%d), waiting 50ms for level shifter...\r\n", status);
        
        /* Reset the MMC block so the hardware state machine is clean (Matches HSS mmc_reset_block) */
        mss_config_clk_rst(MSS_PERIPH_EMMC, hart_id, PERIPHERAL_OFF);
        mss_config_clk_rst(MSS_PERIPH_EMMC, hart_id, PERIPHERAL_ON);
        
        /* Wait 50 milliseconds (50,000 microseconds) */
        sys_usleep(50000); 

        /* When retrying after a reset, the SD card expects standard speed 
         * We downgrade the struct to 25MHz Standard Speed before retrying. */
        g_mmc.clk_rate = MSS_MMC_CLOCK_25MHZ;
        g_mmc.bus_speed_mode = MSS_SDCARD_MODE_DEFAULT_SPEED;

        /* Try one more time now that the SD card is awake! */
        status = MSS_MMC_init(&g_mmc);
    }

    if (status != MSS_MMC_INIT_SUCCESS) {
        kprintf("[HAL] MSS_MMC_init fatally failed with status: %d\r\n", status);
        s_card_initialized = 0;
        return -1;
    }

    s_card_initialized = 1;

    /* MSS_MMC_init soft-resets the MMC hardware, which permanently 
     * clears SRS13 and SRS14. We MUST re-enable the hotplug interrupts here! */
    sdcard_hal_irq_enable();

    return 0; 
}

int sdcard_hal_deinit(void) {
    s_card_initialized = 0;
    return 0;
}

void sdcard_hal_clear_interrupts(void) {
    MMC->SRS12 = (SRS12_CARD_INSERTION | SRS12_CARD_REMOVAL);
}

int sdcard_hal_is_inserted(void) {
    /* Ensure the MMC peripheral clock is active to read the hardware register */
    uint8_t hart_id = (uint8_t)read_csr(mhartid);
    mss_config_clk_rst(MSS_PERIPH_EMMC, hart_id, PERIPHERAL_ON);

    /* Return 1 if physically inserted, 0 if not */
    return (MMC->SRS09 & SRS9_CARD_INSERTED) ? 1 : 0;
}

int sdcard_hal_status(void) {
    /* Ensure the MMC peripheral clock is active so we don't stall the AXI bus 
     * when reading the hardware registers before initialization has occurred. */
    uint8_t hart_id = (uint8_t)read_csr(mhartid);
    mss_config_clk_rst(MSS_PERIPH_EMMC, hart_id, PERIPHERAL_ON);

    /* The PolarFire MMC controller is a standard SD Host Controller (SDHCI).
     * The Present State register (SRS09) Card Inserted bit reflects the SD_CD# pin. */
    if (!(MMC->SRS09 & SRS9_CARD_INSERTED)) {
        /* The card is physically missing from the slot */
        s_card_initialized = 0; /* Force re-initialization upon next insertion */
        return SDCARD_HAL_STATUS_NODISK;
    }

    if (!s_card_initialized) {
        return SDCARD_HAL_STATUS_NOINIT;
    }

    return SDCARD_HAL_STATUS_READY;
}

int sdcard_hal_read_blocks(uint8_t *buff, uint32_t sector, uint32_t count) {
    /* If reading a single block */
    if (count == 1) {
        mss_mmc_status_t res = MSS_MMC_single_block_read(sector, (uint32_t*)buff);
        return (res == MSS_MMC_TRANSFER_SUCCESS) ? 0 : -1;
    }

    /* If reading multiple blocks, we use the SDMA read function.
     * The size parameter in Microchip's API is typically in bytes, not sectors. */
    mss_mmc_status_t res = MSS_MMC_sdma_read(sector, buff, count * 512);

    /* Wait for the DMA transfer to complete */
    if (res == MSS_MMC_TRANSFER_IN_PROGRESS) {
        do {
            res = MSS_MMC_get_transfer_status();
        } while (res == MSS_MMC_TRANSFER_IN_PROGRESS);
    }

    return (res == MSS_MMC_TRANSFER_SUCCESS) ? 0 : -1;
}

int sdcard_hal_write_blocks(const uint8_t *buff, uint32_t sector, uint32_t count) {
    /* If writing a single block */
    if (count == 1) {
        mss_mmc_status_t res = MSS_MMC_single_block_write((const uint32_t*)buff, sector);
        return (res == MSS_MMC_TRANSFER_SUCCESS) ? 0 : -1;
    }

    /* If writing multiple blocks, use SDMA */
    mss_mmc_status_t res = MSS_MMC_sdma_write((const uint8_t *)buff, sector, count * 512);

    /* Wait for the DMA transfer to complete */
    if (res == MSS_MMC_TRANSFER_IN_PROGRESS) {
        do {
            res = MSS_MMC_get_transfer_status();
        } while (res == MSS_MMC_TRANSFER_IN_PROGRESS);
    }

    return (res == MSS_MMC_TRANSFER_SUCCESS) ? 0 : -1;
}

int sdcard_hal_ioctl(int cmd, void *buff) {
    switch (cmd) {
        case SDCARD_HAL_CTRL_SYNC:
            /* If we are using blocking DMA loops above, sync is inherently handled.
             * If we switch to interrupt-driven DMA later, we check transfer status here. */
            return 0;

        case SDCARD_HAL_GET_SECTOR_COUNT:
            /* Microchip doesn't expose a clean sector count getter by default in some versions,
             * but we can pull it from the CSD register if necessary.
             * For now, returning a static large number keeps FatFs happy for basic ops. */
            if (buff) *(uint32_t*)buff = 500000000; /* Approx 256GB dummy value */
            return 0;

        case SDCARD_HAL_GET_SECTOR_SIZE:
            if (buff) *(uint16_t*)buff = 512; /* Standard SD Card sector size */
            return 0;

        case SDCARD_HAL_GET_BLOCK_SIZE:
            if (buff) *(uint32_t*)buff = 1; /* Erase block size */
            return 0;

        default:
            return -1;
    }
}
/* -------------------------------------------------------------------------
 * Hardware Interrupt Control (Satisfying the HAL Contract)
 * ------------------------------------------------------------------------- */

void sdcard_hal_irq_enable(void) {
    uint8_t hart_id = (uint8_t)read_csr(mhartid);

    mss_config_clk_rst(MSS_PERIPH_EMMC, hart_id, PERIPHERAL_ON);

    if (!s_pads_configured) {
        /* 1. FLIP THE HARDWARE MULTIPLEXER TO THE SD CARD! 
         * Must be done BEFORE we check status so SRS09 can see the Card Detect pin! */
        MSS_GPIO_init(GPIO0_LO);
        MSS_GPIO_config(GPIO0_LO, MSS_GPIO_12, MSS_GPIO_OUTPUT_MODE);
        MSS_GPIO_set_output(GPIO0_LO, MSS_GPIO_12, 1);
        sys_usleep(10000);

#ifdef LIBERO_SETTING_IOMUX1_CR
        SYSREG->IOMUX1_CR = LIBERO_SETTING_IOMUX1_CR;
        SYSREG->IOMUX2_CR = LIBERO_SETTING_IOMUX2_CR;
        SYSREG->IOMUX3_CR = LIBERO_SETTING_IOMUX3_CR;
        SYSREG->IOMUX4_CR = LIBERO_SETTING_IOMUX4_CR;
        SYSREG->IOMUX5_CR = LIBERO_SETTING_IOMUX5_CR;
        SYSREG->IOMUX6_CR = LIBERO_SETTING_IOMUX6_CR;
        
#ifdef LIBERO_SETTING_MSSIO_BANK4_IO_CFG_0_1_CR
        SYSREG->MSSIO_BANK4_IO_CFG_0_1_CR   = LIBERO_SETTING_MSSIO_BANK4_IO_CFG_0_1_CR;
        SYSREG->MSSIO_BANK4_IO_CFG_2_3_CR   = LIBERO_SETTING_MSSIO_BANK4_IO_CFG_2_3_CR;
        SYSREG->MSSIO_BANK4_IO_CFG_4_5_CR   = LIBERO_SETTING_MSSIO_BANK4_IO_CFG_4_5_CR;
        SYSREG->MSSIO_BANK4_IO_CFG_6_7_CR   = LIBERO_SETTING_MSSIO_BANK4_IO_CFG_6_7_CR; 
        SYSREG->MSSIO_BANK4_IO_CFG_8_9_CR   = LIBERO_SETTING_MSSIO_BANK4_IO_CFG_8_9_CR;
        SYSREG->MSSIO_BANK4_IO_CFG_10_11_CR = LIBERO_SETTING_MSSIO_BANK4_IO_CFG_10_11_CR;
        SYSREG->MSSIO_BANK4_IO_CFG_12_13_CR = LIBERO_SETTING_MSSIO_BANK4_IO_CFG_12_13_CR;
#endif
#endif
        s_pads_configured = 1;
    }

    /* Clear stale interrupts before unmasking the line */
    sdcard_hal_clear_interrupts();

    /* Route Insertion and Removal events to the Main Interrupt */
    MMC->SRS13 |= (SRS12_CARD_INSERTION | SRS12_CARD_REMOVAL);
    MMC->SRS14 |= (SRS12_CARD_INSERTION | SRS12_CARD_REMOVAL);

    PLIC_SetPriority(MMC_main_PLIC, 2);
    PLIC_SetPriority_Threshold(0); 
    PLIC_EnableIRQ(MMC_main_PLIC);
    
    //set_csr(mie, MIP_MEIP);
}

void sdcard_hal_irq_disable(void) {
    /* Mask ONLY the hotplug signals so we don't blind the SDMA interrupts! */
    MMC->SRS14 &= ~(SRS12_CARD_INSERTION | SRS12_CARD_REMOVAL);
}


/* =========================================================================
 * MICROCHIP HAL WEAK ISR OVERRIDES (Via -include Eclipse Hack)
 * ========================================================================= */

/* CRITICAL FIX: The Microchip header (mss_legacy_defines.h) secretly aliases 
 * mmc_main_plic_IRQHandler to PLIC_mmc_main_IRQHandler. We MUST undefine it 
 * here, otherwise the call at the bottom becomes an infinite direct recursion. */
#undef mmc_main_plic_IRQHandler

/* This is the real Microchip driver exposed by our Eclipse -include shadow header */
extern uint8_t mmc_main_plic_IRQHandler(void);

uint8_t PLIC_mmc_main_IRQHandler(void) {
    uint32_t status = MMC->SRS12;

    /* Check specifically for Card Insertion or Card Removal */
    if (status & (SRS12_CARD_INSERTION | SRS12_CARD_REMOVAL)) {
        
        /* 1. Clear ONLY the hotplug hardware interrupt flags (Write-1-to-Clear) */
        MMC->SRS12 = (status & (SRS12_CARD_INSERTION | SRS12_CARD_REMOVAL));

        /* 2. Mask our hotplug interrupts so we don't spam the CPU during debounce */
        sdcard_hal_irq_disable();

        /* 3. AXI Bus Flush!
         * We must perform a dummy read of the hardware blocks. This forces the
         * CPU to stall until the previous writes physically propagate through 
         * the AXI bus and the hardware drops the PLIC interrupt wire. */
        volatile uint32_t dummy1 = MMC->SRS12;
        volatile uint32_t dummy2 = MMC->SRS14;
        (void)dummy1;
        (void)dummy2;

        /* 4. Push the event to the Block Manager (Never printf in here!) */
        flx_blk_manager_push_event(FLX_BLK_DEV_SDCARD0, FLX_BLK_EVENT_STATE_CHANGE);
    }

    /* CHAIN THE INTERRUPT: Pass execution down to the Microchip driver 
     * so it can process its SDMA/Command interrupts unbothered. */
    return mmc_main_plic_IRQHandler();
}
