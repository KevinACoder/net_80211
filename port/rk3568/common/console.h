/*
 * @file
 * @brief Console front end shared by the early printk backends and the
 *        application: one polled 16550 UART (see drivers/com/).
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#ifndef _COMMON_CONSOLE_H_
#define _COMMON_CONSOLE_H_

#include <stddef.h>

/*
 * Byte-output hook consumed by the printk/printf backends; the console
 * driver points it at the UART during the startup attachment.
 */
extern void (*printf_call)(char c);

/* startup hook: called from the C runtime entry before main() */
void com_console_early_init(void);

/* idempotent full attach; returns 0 when the console is up */
int com_console_init(void);

/* polled byte API over the console UART */
size_t com_console_write(const void *buf, size_t len);
size_t com_console_read(void *buf, size_t len);

#endif	/* _COMMON_CONSOLE_H_ */
