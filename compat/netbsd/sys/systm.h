/*
 * @file
 * @brief Kernel services shell: panic, delay, printf.
 */

#ifndef _SYS_SYSTM_H_
#define _SYS_SYSTM_H_

#include <sys/cdefs.h>
#include "types.h"
#include <sys/intr.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/* NetBSD calls panic with the arguments in a second pair of
 * parentheses: panic(("message %d", x)). */

void delay(unsigned int us);
#define DELAY(x) delay(x)

void get_random_bytes(void *, size_t);

int kprintf(const char *fmt, ...) __printflike(1, 2);
#define printf kprintf
#define uprintf kprintf
#define aprint_naive kprintf
#define aprint_verbose kprintf
#define aprint_debug kprintf

#define NBBY 8
static inline void setbit(volatile unsigned char *p, unsigned int n) {
	p[n / NBBY] |= (unsigned char) (1 << (n % NBBY));
}
static inline void clrbit(volatile unsigned char *p, unsigned int n) {
	p[n / NBBY] &= (unsigned char) ~(1 << (n % NBBY));
}
static inline int isset(const volatile unsigned char *p, unsigned int n) {
	return p[n / NBBY] & (1 << (n % NBBY));
}
static inline int isclr(const volatile unsigned char *p, unsigned int n) {
	return !(p[n / NBBY] & (1 << (n % NBBY)));
}

ipl_t splnet(void);
void splx(ipl_t);

int uimin(int a, int b);
int uimax(int a, int b);

int copyin(const void *, void *, size_t);
int copyout(const void *, void *, size_t);
int copystr(const void *, void *, size_t, size_t *);

#endif /* _SYS_SYSTM_H_ */
