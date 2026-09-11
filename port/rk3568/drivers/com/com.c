/*	$NetBSD: com.c,v 1.388 2025/02/12 12:00:00 riastradh Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2004, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; the console routines trace back to work by
 * Gordon W. Ross and Chris G. Demetriou.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Polled 16550-family UART console, derived from NetBSD
 * sys/dev/ic/com.c (rev 1.388). Only the subsystem the test system
 * needs survives the derivation: comspeed(), cflag2lcr(),
 * cominit() for DesignWare APB UARTs, and the polled
 * com_common_getc()/com_common_putc() pair. The tty layer, the
 * interrupt-driven ring buffers, the read-ahead and the console
 * magic-sequence handling are all upstream-only.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdint.h>

#include "com.h"

/* initializer for typical 16550-ish hardware (com_std_map upstream) */
static const uint8_t com_std_map[COM_REGMAP_NENTRIES] = {
	[COM_REG_RXDATA]	= com_data,
	[COM_REG_TXDATA]	= com_data,
	[COM_REG_DLBL]		= com_dlbl,
	[COM_REG_DLBH]		= com_dlbh,
	[COM_REG_IER]		= com_ier,
	[COM_REG_IIR]		= com_iir,
	[COM_REG_FIFO]		= com_fifo,
	[COM_REG_EFR]		= com_efr,
	[COM_REG_LCR]		= com_lcr,
	[COM_REG_MCR]		= com_mcr,
	[COM_REG_LSR]		= com_lsr,
	[COM_REG_MSR]		= com_msr,
};

void
com_init_regs_stride_width(struct com_regs *regs, uintptr_t iobase,
    unsigned int regshift, unsigned int width)
{
	unsigned int i;

	regs->cr_iobase = iobase;
	regs->cr_regshift = regshift;
	regs->cr_width = width;
	for (i = 0; i < COM_REGMAP_NENTRIES; i++) {
		regs->cr_map[i] = (uint8_t) (com_std_map[i] << regshift);
	}
}

uint8_t
com_reg_read(const struct com_regs *regs, unsigned int reg)
{
	uintptr_t addr = regs->cr_iobase + regs->cr_map[reg];

	if (regs->cr_width == 4) {
		return (uint8_t) *(volatile uint32_t *) addr;
	}
	return *(volatile uint8_t *) addr;
}

void
com_reg_write(const struct com_regs *regs, unsigned int reg, uint8_t value)
{
	uintptr_t addr = regs->cr_iobase + regs->cr_map[reg];

	if (regs->cr_width == 4) {
		*(volatile uint32_t *) addr = value;
	} else {
		*(volatile uint8_t *) addr = value;
	}
}

/*ARGSUSED*/
int
comspeed(long speed, long frequency)
{
#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */

	int x, err;
	int divisor = 16;

	if (speed == 0)
		return (0);
	if (speed < 0)
		return (-1);
	x = divrnd(frequency / divisor, speed);
	if (x <= 0)
		return (-1);
	err = divrnd(((long long) frequency) * 1000 / divisor,
	    speed * x) - 1000;
	if (err < 0)
		err = -err;
	if (err > COM_TOLERANCE)
		return (-1);
	return (x);

#undef	divrnd
}

uint8_t
com_cflag2lcr(unsigned int cflag)
{
	uint8_t lcr = 0;

	switch (cflag & COMCSIZE) {
	case COMCS8:
		lcr |= LCR_8BITS;
		break;
	case COMCS7:
		lcr |= LCR_7BITS;
		break;
	case COMCS6:
		lcr |= LCR_6BITS;
		break;
	case COMCS5:
		lcr |= LCR_5BITS;
		break;
	}
	if (cflag & COMCSTOPB)
		lcr |= LCR_STOPB;
	if (cflag & COMPARENB) {
		lcr |= LCR_PENAB;
		if (!(cflag & COMPARODD))
			lcr |= LCR_PEVEN;
	}
	return (lcr);
}

/*
 * Initialize UART for use as console: program the divisor, the line
 * format, the modem pins and a reset 1-byte-trigger FIFO, and leave
 * interrupts off (the console is polled).
 */
int
cominit(struct com_regs *regsp, int rate, int frequency, unsigned int cflag)
{

	rate = comspeed(rate, frequency);
	if (rate != -1) {
		CSR_WRITE_1(regsp, COM_REG_LCR, LCR_EERS);
		CSR_WRITE_1(regsp, COM_REG_EFR, 0);
		CSR_WRITE_1(regsp, COM_REG_LCR, LCR_DLAB);
		CSR_WRITE_1(regsp, COM_REG_DLBL, rate & 0xff);
		CSR_WRITE_1(regsp, COM_REG_DLBH, rate >> 8);
	}
	CSR_WRITE_1(regsp, COM_REG_LCR, com_cflag2lcr(cflag));
	CSR_WRITE_1(regsp, COM_REG_MCR, MCR_DTR | MCR_RTS);

	CSR_WRITE_1(regsp, COM_REG_FIFO,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST |
	    FIFO_TRIGGER_1);

	CSR_WRITE_1(regsp, COM_REG_IER, 0);

	return (0);
}

int
com_common_getc(const struct com_regs *regsp)
{
	uint8_t c;

	/* don't block until a character becomes available */
	if (!(CSR_READ_1(regsp, COM_REG_LSR) & LSR_RXRDY)) {
		return -1;
	}

	c = CSR_READ_1(regsp, COM_REG_RXDATA);
	return (c);
}

void
com_common_putc(const struct com_regs *regsp, int c)
{
	int timo;

	/* wait for any pending transmission to finish */
	timo = 150000;
	while (!(CSR_READ_1(regsp, COM_REG_LSR) & LSR_TXRDY) && --timo)
		continue;

	CSR_WRITE_1(regsp, COM_REG_TXDATA, c);
	__asm volatile("dsb sy");
}
