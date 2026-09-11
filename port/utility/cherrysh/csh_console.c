/*
 * @file csh_console.c
 * @brief CherrySH console over polled UART2 for the RK3568 test system.
 *
 * The console task drives chry_shell_task_repl() with blocking sget, so
 * a single task is enough: the REPL only yields while waiting for input.
 * Command tables come from the FSymTab/VSymTab linker sections.
 *
 * @author zhugengyu
 * @date 10.09.2026
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "console.h"
#include "csh.h"
#include "csh_console.h"

#define CONSOLE_TASK_PRIORITY 3
#define CONSOLE_TASK_STACK 2048

static chry_shell_t csh;

static uint16_t csh_sput_cb(chry_readline_t *rl, const void *data, uint16_t size)
{
	(void)rl;

	com_console_write(data, size);

	return size;
}

static uint16_t csh_sget_cb(chry_readline_t *rl, void *data, uint16_t size)
{
	(void)rl;

	for (;;) {
		size_t n = com_console_read(data, 1);

		if (n > 0) {
			return (uint16_t)n;
		}
		vTaskDelay(1);
	}
}

static int cmd_version(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	csh_printf(&csh, "net_80211 freertos test system, built %s %s\r\n",
	    __DATE__, __TIME__);

	return 0;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_version, version, "show the build timestamp",
    "version\r\n    - show the build timestamp\r\n");

/* bare command names resolve through PATH; the export macro places
 * commands under /sbin */
#define ENV_PATH_VALUE "/sbin:/bin"
const char env_path_var[] = ENV_PATH_VALUE;
CSH_RVAR_EXPORT(env_path_var, PATH, sizeof(ENV_PATH_VALUE));

int console_printf(const char *fmt, ...)
{
	static char buf[512];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > (int) sizeof(buf)) {
		n = sizeof(buf);
	}
	if (n > 0) {
		com_console_write(buf, (size_t) n);
	}
	return n;
}

static void console_task(void *param)
{
	(void)param;

	for (;;) {
		int ret = chry_shell_task_repl(&csh);

		if (ret == 1) {
			/* readline wants more input; sget already blocks,
			 * so this is only reachable transiently */
			vTaskDelay(1);
		} else if (ret < 0) {
			vTaskDelay(10);
		}
	}
}

int console_start(void)
{
	static char prompt_buffer[128];
	static char history_buffer[128];
	static char line_buffer[256];
	chry_shell_init_t init;
	int ret;

	if (com_console_init() != 0) {
		return -1;
	}

	memset(&init, 0, sizeof(init));
	init.sput = csh_sput_cb;
	init.sget = csh_sget_cb;

	/* FSymTab/VSymTab bounds are provided by linker.ld */
	{
		extern const int __fsymtab_start;
		extern const int __fsymtab_end;
		extern const int __vsymtab_start;
		extern const int __vsymtab_end;

		init.command_table_beg = &__fsymtab_start;
		init.command_table_end = &__fsymtab_end;
		init.variable_table_beg = &__vsymtab_start;
		init.variable_table_end = &__vsymtab_end;
	}

	init.prompt_buffer = prompt_buffer;
	init.prompt_buffer_size = sizeof(prompt_buffer);
	init.history_buffer = history_buffer;
	init.history_buffer_size = sizeof(history_buffer);
	init.line_buffer = line_buffer;
	init.line_buffer_size = sizeof(line_buffer);

	init.uid = 0;
	init.user[0] = "root";
	/* chry_shell_init() strnlen()s the hash unconditionally: an empty
	 * string, not NULL, means "no password". */
	init.hash[0] = "";
	init.host = "rk3568";
	init.user_data = NULL;

	ret = chry_shell_init(&csh, &init);
	if (ret != 0) {
		return -1;
	}

	return xTaskCreate(console_task, "console", CONSOLE_TASK_STACK,
	    NULL, CONSOLE_TASK_PRIORITY, NULL);
}
