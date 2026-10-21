/*******************************************************************************
 * Copyright 2025 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * Application code running on e51
 *
 */

#include <stdio.h>
#include <string.h>
#include "mpfs_hal/mss_hal.h"
#include "mpfs_hal/startup_gcc/system_startup.h"
#include "mpfs_hal/startup_gcc/system_startup_defs.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"
//#include "inc/uart_mapping.h"

volatile uint32_t count_sw_ints_h0 = 0U;

static void init_constructors(void)
{
    void (**fun_ptr)(void) = (void (**)(void))&__init_array_start;
    while (fun_ptr != (void (**)(void))&__init_array_end)
    {
        (*fun_ptr)();
        fun_ptr++;
    }
}

extern void u54_1(void);

/*
 * Override the weak main_first_hart_app() from MPFS HAL.
 * When Hart 0 is first hart (e.g. JTAG boot):
 *   Initializes memory, signals Hart 1, and runs e51().
 * When Hart 1 is first hart (e.g. HSS bootloader):
 *   Initializes memory and runs u54_1().
 */
int main_first_hart_app(HLS_DATA* hls)
{
    uint64_t hartid = read_csr(mhartid);

    init_memory();
    init_constructors();

    hls->in_wfi_indicator = HLS_MAIN_HART_STARTED;

    if (hartid == 0U)
    {
        /* Allow Hart 1 to observe HLS_MAIN_HART_STARTED and enter WFI */
        for (volatile uint32_t i = 0; i < 50000; i++) { }

        /* Clear pending interrupts and wake up Hart 1 (U54_1) */
        clear_soft_interrupt();
        set_csr(mie, MIP_MSIP);
        raise_soft_interrupt(1U);
        __enable_irq();

        /* Run Hart 0 application code */
        e51();
    }
    else if (hartid == 1U)
    {
        /* Run Hart 1 application code directly */
        u54_1();
    }

    return 0;
}

/* Main function for the hart0(e51 processor).
 * Application code running on hart0 is placed here
 */

void e51(void)
{
    volatile uint32_t icount = 0U;

#if 0
    const uint8_t g_info_string[] =
            " \r\n\r\n **** Hello World !! ****\r\n";
    extern struct mss_uart_instance* p_uartmap_e51;
    // DISABLED: UART initialization is handled by U54_1 for the CLI.
    // Enabling this here would cause contention for UART0.
    (void)mss_config_clk_rst(MSS_PERIPH_MMUART_E51,
            (uint8_t) MPFS_HAL_FIRST_HART,
            PERIPHERAL_ON);

    MSS_UART_init(p_uartmap_e51,
            MSS_UART_115200_BAUD,
            MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);

    MSS_UART_polled_tx_string(p_uartmap_e51, g_info_string);
#endif

    /* Clear pending software interrupt in case there was any. */
    clear_soft_interrupt();
    set_csr(mie, MIP_MSIP);

    /* Raise software interrupt to wake hart 1 */
    raise_soft_interrupt(1U);

    __enable_irq();

    while (1U)
    {
        icount++;

        if (0x100000U == icount)
        {
            icount = 0U;
        }
    }
    /* never return */
}

/* hart0 software interrupt handler */
void Software_h0_IRQHandler(void)
{
    count_sw_ints_h0++;
}
