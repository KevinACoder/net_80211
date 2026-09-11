/*
 * @file csh_console.h
 * @brief CherrySH console over polled UART2 for the RK3568 test system.
 *
 * @author zhugengyu
 * @date 10.09.2026
 */

#ifndef CSH_CONSOLE_H_
#define CSH_CONSOLE_H_

#include <stdint.h>

/* Initialize UART2 and create the REPL task. Call before
 * vTaskStartScheduler(). Returns pdPASS on success. */
int console_start(void);

/* printf into the console UART (safe to call from any task). */
int console_printf(const char *fmt, ...);

#endif
