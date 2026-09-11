/*	$NetBSD: comreg.h,v 1.28 2022/10/06 19:59:55 riastradh Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)comreg.h	7.2 (Berkeley) 5/9/91
 */

/*
 * Register map and bit definitions for 16550-family UARTs, derived
 * from NetBSD sys/dev/ic/comreg.h and ns16550reg.h (the com_std_map
 * register indices come from sys/dev/ic/comvar.h). Trimmed to the
 * polled-console subset used by this test system.
 */

#ifndef _DRIVERS_COM_COMREG_H_
#define _DRIVERS_COM_COMREG_H_

#ifndef COM_TOLERANCE
#define	COM_TOLERANCE	30	/* baud rate tolerance, in 0.1% units */
#endif

/*
 * Register selectors: indices into the com register map. RX and TX
 * data share the data register; DLBL/EFR share registers with IER/IIR
 * selected through LCR bits.
 */
#define	COM_REG_RXDATA		0
#define	COM_REG_TXDATA		1
#define	COM_REG_DLBL		2
#define	COM_REG_DLBH		3
#define	COM_REG_IER		4
#define	COM_REG_IIR		5
#define	COM_REG_FIFO		6
#define	COM_REG_EFR		8
#define	COM_REG_LCR		10
#define	COM_REG_MCR		11
#define	COM_REG_LSR		12
#define	COM_REG_MSR		13
#define	COM_REGMAP_NENTRIES	14

/* raw offsets in a byte-strapped 16550 (the register map values) */
#define	com_data	0	/* data register (R/W) */
#define	com_dlbl	0	/* divisor latch low (W) */
#define	com_dlbh	1	/* divisor latch high (W) */
#define	com_ier		1	/* interrupt enable (W) */
#define	com_iir		2	/* interrupt identification (R) */
#define	com_fifo	2	/* FIFO control (W) */
#define	com_efr		2	/* enhanced feature register (R/W) */
#define	com_lcr		3	/* line control register (R/W) */
#define	com_mcr		4	/* modem control register (R/W) */
#define	com_lsr		5	/* line status register (R/W) */
#define	com_msr		6	/* modem status register (R/W) */

/* interrupt enable register */
#define	IER_ERXRDY	0x1	/* Enable receiver interrupt */
#define	IER_ETXRDY	0x2	/* Enable transmitter empty interrupt */
#define	IER_ERLS	0x4	/* Enable line status interrupt */
#define	IER_EMSC	0x8	/* Enable modem status interrupt */

/* interrupt identification register */
#define	IIR_IMASK	0xf
#define	IIR_NOPEND	0x1	/* No pending interrupts */
#define	IIR_FIFO_MASK	0xc0	/* set if FIFOs are enabled */

/* fifo control register */
#define	FIFO_ENABLE	0x01	/* Turn the FIFO on */
#define	FIFO_RCV_RST	0x02	/* Reset RX FIFO */
#define	FIFO_XMT_RST	0x04	/* Reset TX FIFO */
#define	FIFO_TRIGGER_1	0x00	/* Trigger RXRDY intr on 1 character */

/* enhanced feature register */
#define	EFR_RXFLOWNONE	0x00	/* No automatic XON/XOFF receive */

/* line control register */
#define	LCR_EERS	0xBF	/* Enable access to Enhanced Register Set */
#define	LCR_DLAB	0x80	/* Divisor latch access enable */
#define	LCR_SBREAK	0x40	/* Break Control */
#define	LCR_PZERO	0x38	/* Space parity */
#define	LCR_PONE	0x28	/* Mark parity */
#define	LCR_PEVEN	0x18	/* Even parity */
#define	LCR_PODD	0x08	/* Odd parity */
#define	LCR_PENAB	0x08	/* XXX - low order bit of all parity */
#define	LCR_STOPB	0x04	/* 2 stop bits per serial word */
#define	LCR_8BITS	0x03	/* 8 bits per serial word */
#define	LCR_7BITS	0x02	/* 7 bits */
#define	LCR_6BITS	0x01	/* 6 bits */
#define	LCR_5BITS	0x00	/* 5 bits */

/* modem control register */
#define	MCR_LOOPBACK	0x10	/* Loop test: echos from TX to RX */
#define	MCR_IENABLE	0x08	/* Out2: enables UART interrupts */
#define	MCR_DRS		0x04	/* Out1: resets some internal modems */
#define	MCR_RTS		0x02	/* Request To Send */
#define	MCR_DTR		0x01	/* Data Terminal Ready */

/* line status register */
#define	LSR_RCV_FIFO	0x80
#define	LSR_TSRE	0x40	/* Transmitter empty: byte sent */
#define	LSR_TXRDY	0x20	/* Transmitter buffer empty */
#define	LSR_BI		0x10	/* Break detected */
#define	LSR_FE		0x08	/* Framing error: bad stop bit */
#define	LSR_PE		0x04	/* Parity error */
#define	LSR_OE		0x02	/* Overrun, lost incoming byte */
#define	LSR_RXRDY	0x01	/* Byte ready in Receive Buffer */
#define	LSR_RCV_MASK	0x1f	/* Mask for incoming data or error */

#endif	/* _DRIVERS_COM_COMREG_H_ */
