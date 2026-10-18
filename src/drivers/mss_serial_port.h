/**
 * @file felix/arch/polarfire/mss_serial_port.h
 */
#ifndef _MSS_SERIAL_PORT_H
#define _MSS_SERIAL_PORT_H

#include "drivers/flx_serial_core.h"

/* Exposes the PolarFire operations table for the kernel to use */
extern const FlxUartOps_t g_mss_uart_ops;

#endif /* _MSS_SERIAL_PORT_H */
