/*
 * @file syscalls.c
 * @brief newlib syscall stubs for the freestanding test system.
 *
 * newlib is linked only for string/formatting helpers, but pulling it in
 * references the syscall layer; _sbrk backs malloc with the linker heap
 * region and stdio is routed to the console UART.
 *
 * @author zhugengyu
 * @date 10.09.2026
 */

#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#include "console.h"

extern char HeapBase;
extern char HeapLimit;

void _exit(int status)
{
	(void)status;

	for (;;) {
		__asm volatile("wfi");
	}
}

void *_sbrk(ptrdiff_t incr)
{
	static char *brk = &HeapBase;
	char *prev = brk;

	if (brk + incr > &HeapLimit) {
		errno = ENOMEM;
		return (void *)-1;
	}
	brk += incr;

	return prev;
}

int _write(int fd, const void *buf, size_t len)
{
	(void)fd;

	com_console_write(buf, len);

	return (int)len;
}

int _read(int fd, void *buf, size_t len)
{
	(void)fd;

	if (len == 0) {
		return 0;
	}

	return (int)com_console_read(buf, len);
}

int _close(int fd)
{
	(void)fd;

	return -1;
}

off_t _lseek(int fd, off_t off, int whence)
{
	(void)fd;
	(void)off;
	(void)whence;

	return (off_t)-1;
}

int _fstat(int fd, struct stat *st)
{
	(void)fd;

	st->st_mode = S_IFCHR;

	return 0;
}

int _isatty(int fd)
{
	(void)fd;

	return 1;
}

int _kill(int pid, int sig)
{
	(void)pid;
	(void)sig;

	errno = EINVAL;

	return -1;
}

int _getpid(void)
{
	return 1;
}
