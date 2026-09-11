/*
 * @file
 * @brief Polled 16550-family UART console, derived from NetBSD
 *        sys/dev/ic/com.c (see com.c for the upstream notice).
 *
 * The register map handles DesignWare APB UARTs (reg-shift / 32-bit
 * register width) the same way com_init_regs_stride_width() does
 * upstream: every map entry is shifted by the register stride and
 * accessed at the configured width.
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#ifndef _DRIVERS_COM_COM_H_
#define _DRIVERS_COM_COM_H_

#include <stdint.h>
#include <stddef.h>

#include "comreg.h"

/* termios-compatible line format bits (a subset of <sys/termios.h>) */
#define	COMCSIZE	0x30
#define	COMCS5		0x00
#define	COMCS6		0x10
#define	COMCS7		0x20
#define	COMCS8		0x30
#define	COMCSTOPB	0x400	/* send two stop bits */
#define	COMPARENB	0x1000	/* parity enabled */
#define	COMPARODD	0x2000	/* odd parity */

/*
 * Mapped register window: base address plus the stride and access
 * width of the implementation, and the register index map (the
 * com_std_map layer upstream: selectors like COM_REG_LSR index into
 * cr_map, whose entries hold the raw register offsets shifted by the
 * stride).
 */
struct com_regs {
	uintptr_t	cr_iobase;
	unsigned int	cr_regshift;	/* register stride shift */
	unsigned int	cr_width;	/* register access width in bytes */
	uint8_t		cr_map[COM_REGMAP_NENTRIES];
};

/*
 * Fill in a mapped register window for a UART at cr_iobase whose
 * registers sit at offset (index << cr_regshift) and are accessed at
 * cr_width bytes. Mirrors com_init_regs_stride_width() upstream.
 */
void	com_init_regs_stride_width(struct com_regs *regs, uintptr_t iobase,
	    unsigned int regshift, unsigned int width);

/* register access through the map (the CSR_* macros upstream) */
uint8_t	com_reg_read(const struct com_regs *regs, unsigned int reg);
void	com_reg_write(const struct com_regs *regs, unsigned int reg,
	    uint8_t value);

#define	CSR_READ_1(regs, reg)	com_reg_read((regs), (reg))
#define	CSR_WRITE_1(regs, reg, v) com_reg_write((regs), (reg), (v))

/*
 * Baud rate divisor with the upstream tolerance check; returns -1
 * when the requested rate is out of range or too far off.
 */
int	comspeed(long speed, long frequency);

/* termios line format to LCR value (cflag2lcr upstream) */
uint8_t	com_cflag2lcr(unsigned int cflag);

/*
 * Program the line parameters and FIFOs. Equivalent to the
 * COM_TYPE_DW_APB branch of cominit() upstream; cflag carries the
 * line format (COMCS8 | no parity | 1 stop for this console).
 */
int	cominit(struct com_regs *regs, int rate, int frequency,
	    unsigned int cflag);

/*
 * Polled character I/O (com_common_getc/com_common_putc upstream,
 * without the read-ahead and console-magic handling of a full tty).
 * com_common_getc returns -1 when the receiver is empty.
 */
int	com_common_getc(const struct com_regs *regs);
void	com_common_putc(const struct com_regs *regs, int c);

#endif	/* _DRIVERS_COM_COM_H_ */
