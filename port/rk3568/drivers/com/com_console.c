/*
 * @file
 * @brief Console attachment for the polled 16550 UART console.
 *
 * Runs in two phases: the startup code calls com_console_early_init()
 * before main() so the printk/printf backends have a working output
 * byte, and main-time consumers call com_console_init() for the
 * idempotent full attach before using the read/write API.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stddef.h>
#include <stdint.h>

#include "com.h"
#include "console.h"

/*
 * UART2 on this board: 0xfe660000, 24 MHz reference clock, wired to
 * the 115200 8N1 console; the DesignWare APB registers sit at a 4-byte
 * stride and are accessed 32 bits wide (dts reg-shift/reg-io-width).
 */
#define	COM_CONSOLE_BASE	0xFE660000u
#define	COM_CONSOLE_REGSHIFT	2
#define	COM_CONSOLE_WIDTH	4
#define	COM_CONSOLE_RATE	115200
#define	COM_CONSOLE_FREQ	24000000

static struct com_regs com_console_regs;
static int com_console_ready;

void (*printf_call)(char c);

static void com_console_outc(char c)
{

	if (com_console_ready) {
		com_common_putc(&com_console_regs, c);
	}
}

static void com_console_attach(void)
{

	if (com_console_ready) {
		return;
	}
	com_init_regs_stride_width(&com_console_regs, COM_CONSOLE_BASE,
	    COM_CONSOLE_REGSHIFT, COM_CONSOLE_WIDTH);
	cominit(&com_console_regs, COM_CONSOLE_RATE, COM_CONSOLE_FREQ,
	    COMCS8);
	printf_call = com_console_outc;
	com_console_ready = 1;
}

/* startup hook: called from the C runtime entry before main() */
void com_console_early_init(void)
{

	com_console_attach();
}

int com_console_init(void)
{

	com_console_attach();
	return com_console_ready ? 0 : -1;
}

size_t com_console_write(const void *buf, size_t len)
{
	const char *p = buf;
	size_t n;

	com_console_attach();
	for (n = 0; n < len; n++) {
		com_common_putc(&com_console_regs, *p++);
	}
	return len;
}

size_t com_console_read(void *buf, size_t len)
{
	char *p = buf;
	size_t n;

	for (n = 0; n < len; n++) {
		int c = com_common_getc(&com_console_regs);

		if (c < 0) {
			break;
		}
		*p++ = (char) c;
	}
	return n;
}
