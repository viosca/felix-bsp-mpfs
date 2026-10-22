/**
 * @file felix/arch/polarfire/mss_serial_port.c
 */


#include "mss_serial_port.h"
#include "kernel/devices.h"
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"

/* =========================================================================
 * Embedded UART User Data (Merged from uart_userdata.h)
 * ========================================================================= */

/**
 * @brief Enum to identify what type of pointer is inside the generic container.
 */
typedef enum {
    UARTUserDataType_UNKNOWN = 0,
    UARTUserDataType_DEVICE  = 1, /* Points to a FlxDev_t */
} UARTUserDataType_t;

/**
 * @brief Generic wrapper assigned to vendor HAL 'user_data' pointers.
 */
typedef struct {
    UARTUserDataType_t type;
    union {
        FlxDev_t *device;
        void *generic;
    };
} UARTUserData_t;

/**
 * @brief Injects the generic Felix userdata into a vendor UART instance.
 */
static inline void uart_set_userdata(mss_uart_instance_t *uart, UARTUserData_t *userdata) {
    if (uart) {
        uart->user_data = (void *)userdata;
    }
}

/**
 * @brief Safely extracts the generic Felix wrapper from a vendor UART instance.
 */
static inline FlxDev_t* uart_get_dev(mss_uart_instance_t *uart) {
    if (!uart || !uart->user_data) return NULL;
    UARTUserData_t *ud = (UARTUserData_t *)uart->user_data;
    if (ud->type != UARTUserDataType_DEVICE) return NULL;
    return ud->device;
}

/* =========================================================================
 * Vendor Serial Implementation
 * ========================================================================= */

static UARTUserData_t g_uart_userdata[FLX_DEV_MAX_DEVICES];

static PLIC_IRQn_Type get_uart_irq_source(mss_uart_instance_t *uart) {
    if (uart == &g_mss_uart0_lo || uart == &g_mss_uart0_hi) return PLIC_MMUART0_INT_OFFSET;
    if (uart == &g_mss_uart1_lo || uart == &g_mss_uart1_hi) return PLIC_MMUART1_INT_OFFSET;
    if (uart == &g_mss_uart2_lo || uart == &g_mss_uart2_hi) return PLIC_MMUART2_INT_OFFSET;
    if (uart == &g_mss_uart3_lo || uart == &g_mss_uart3_hi) return PLIC_MMUART3_INT_OFFSET;
    if (uart == &g_mss_uart4_lo || uart == &g_mss_uart4_hi) return PLIC_MMUART4_INT_OFFSET;
    return MMUART0_PLIC_77;
}

static mss_peripherals get_uart_peripheral_id(mss_uart_instance_t *uart) {
    if (uart == &g_mss_uart0_lo || uart == &g_mss_uart0_hi) return MSS_PERIPH_MMUART0;
    if (uart == &g_mss_uart1_lo || uart == &g_mss_uart1_hi) return MSS_PERIPH_MMUART1;
    if (uart == &g_mss_uart2_lo || uart == &g_mss_uart2_hi) return MSS_PERIPH_MMUART2;
    if (uart == &g_mss_uart3_lo || uart == &g_mss_uart3_hi) return MSS_PERIPH_MMUART3;
    if (uart == &g_mss_uart4_lo || uart == &g_mss_uart4_hi) return MSS_PERIPH_MMUART4;
    return MSS_PERIPH_MMUART0;
}

/* --- Hardware Interrupt Service Routines --- */

#define MSS_UART_DATA_READY 0x01

static void mss_serial_rx_handler(mss_uart_instance_t *uart) {
    FlxDev_t *dev = uart_get_dev(uart);
    if (!dev) return;

    /* Drain FIFO and pass UP to the Generic OS Layer */
    while (uart->hw_reg->LSR & MSS_UART_DATA_READY) {
        flx_serial_isr_rx_byte(dev->id, (uint8_t)uart->hw_reg->RBR);
    }
}

static void mss_serial_tx_handler(mss_uart_instance_t *uart) {
    FlxDev_t *dev = uart_get_dev(uart);
    if (!dev) return;

    uint8_t count = 0;
    uint8_t chunk[16];
    uint8_t byte;

    /* Pull DOWN from the Generic OS Layer */
    while (count < sizeof(chunk) && flx_serial_isr_tx_pop(dev->id, &byte)) {
        chunk[count++] = byte;
    }

    if (count > 0) {
        MSS_UART_fill_tx_fifo(uart, chunk, count);
        uart->tx_buff_size = 1;
        uart->tx_idx = 0;
        uart->hw_reg->IER |= MSS_UART_TBE_IRQ;
    } else {
        uart->tx_buff_size = 0;
        uart->tx_idx = 0;
        uart->hw_reg->IER &= ~MSS_UART_TBE_IRQ;
        flx_serial_isr_tx_done(dev->id);
    }
}

/* --- Hardware Operations Table Implementation --- */

static int mss_hw_init(int dev_id, void *hw_instance) {
    mss_uart_instance_t *uart = (mss_uart_instance_t *)hw_instance;

    /* Bind userdata so the ISRs can figure out which device fired */
    FlxDev_t *dev = flx_dev_get(dev_id);
    if (dev) {
        g_uart_userdata[dev_id].type = UARTUserDataType_DEVICE;
        g_uart_userdata[dev_id].device = dev;
        uart_set_userdata(uart, &g_uart_userdata[dev_id]);
    }

    PLIC_IRQn_Type irq = get_uart_irq_source(uart);
    mss_peripherals periph = get_uart_peripheral_id(uart);
    uint32_t hart_id = read_csr(mhartid);

    (void)mss_config_clk_rst(periph, (uint8_t) hart_id, PERIPHERAL_ON);
    MSS_UART_init(uart, MSS_UART_115200_BAUD,
                  MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);

    MSS_UART_set_rx_handler(uart, mss_serial_rx_handler, MSS_UART_FIFO_SINGLE_BYTE);
    MSS_UART_set_tx_handler(uart, mss_serial_tx_handler);

    PLIC_SetPriority(irq, 1);
    PLIC_EnableIRQ(irq);
    //set_csr(mie, MIP_MSIP | MIP_MEIP);
    return 0;
}

static void mss_tx_kickstart(void *hw_instance, volatile bool *tx_active_flag) {
    mss_uart_instance_t *uart = (mss_uart_instance_t *)hw_instance;
    /* When kickstarting from idle, the hardware FIFO may already be empty.
     * On 16550/MSS UARTs, enabling TBE in IER does not assert an interrupt
     * if the transmitter is already empty. Calling mss_serial_tx_handler directly
     * primes the FIFO with initial bytes so transmission begins and TBE IRQ fires. */
    mss_serial_tx_handler(uart);
}

static void mss_irq_mask(void *hw_instance, bool mask) {
    mss_uart_instance_t *uart = (mss_uart_instance_t *)hw_instance;
    PLIC_IRQn_Type irq = get_uart_irq_source(uart);
    if (mask) {
        PLIC_DisableIRQ(irq);
    } else {
        PLIC_EnableIRQ(irq);
    }
}

const FlxUartOps_t g_mss_uart_ops = {
    .hw_init = mss_hw_init,
    .tx_kickstart = mss_tx_kickstart,
    .irq_mask = mss_irq_mask
};

