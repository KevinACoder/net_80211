/*
 * Copyright (C) 2024, Phytium Technology Co., Ltd.   All Rights Reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  
 * 
 * FilePath: fvectors.S
 * Date: 2024-01-15 14:53:41
 * LastEditTime: 2024-01-16 17:31:50
 * Description:  This file is for the initial vector table for the processor
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe     2021/7/3     first release
 * 1.1   wangxiaodong 2024/1/16   Simplifies floating-point operations
 */

#include "sdkconfig.h"

.globl _boot
.globl _vector_table

.globl FiqInterrupt
.globl IrqInterrupt
.globl SerrorInterrupt
.globl SynchronousInterrupt
.globl SErrorInterruptHandler
.globl SynchronousInterruptHandler

#define EXCEPTION_FRAME_SIZE 704 /* 704 = 22*8 + 32*16 + 2*8, except sp and spsr in FExcFrame */

.macro SaveRegister
	stp	X0,X1, [sp,#-0x10]!
	stp	X2,X3, [sp,#-0x10]!
	stp	X4,X5, [sp,#-0x10]!
	stp	X6,X7, [sp,#-0x10]!
	stp	X8,X9, [sp,#-0x10]!
	stp	X10,X11, [sp,#-0x10]!
	stp	X12,X13, [sp,#-0x10]!
	stp	X14,X15, [sp,#-0x10]!
	stp	X16,X17, [sp,#-0x10]!
	stp X18,X19, [sp,#-0x10]!
	stp X29,X30, [sp,#-0x10]!

	stp	q0,q1, [sp,#-0x20]!
	stp	q2,q3, [sp,#-0x20]!
	stp	q4,q5, [sp,#-0x20]!
	stp	q6,q7, [sp,#-0x20]!
	stp	q8,q9, [sp,#-0x20]!
	stp	q10,q11, [sp,#-0x20]!
	stp	q12,q13, [sp,#-0x20]!
	stp	q14,q15, [sp,#-0x20]!
	stp	q16,q17, [sp,#-0x20]!
	stp	q18,q19, [sp,#-0x20]!
	stp	q20,q21, [sp,#-0x20]!
	stp	q22,q23, [sp,#-0x20]!
	stp	q24,q25, [sp,#-0x20]!
	stp	q26,q27, [sp,#-0x20]!
	stp	q28,q29, [sp,#-0x20]!
	stp	q30,q31, [sp,#-0x20]!
.endm


.macro RestoreRegister
	ldp	q30,q31, [sp], #0x20
	ldp	q28,q29, [sp], #0x20
	ldp	q26,q27, [sp], #0x20
	ldp	q24,q25, [sp], #0x20
	ldp	q22,q23, [sp], #0x20
	ldp	q20,q21, [sp], #0x20
	ldp	q18,q19, [sp], #0x20
	ldp	q16,q17, [sp], #0x20
	ldp	q14,q15, [sp], #0x20
	ldp	q12,q13, [sp], #0x20
	ldp	q10,q11, [sp], #0x20
	ldp	q8,q9, [sp], #0x20
	ldp	q6,q7, [sp], #0x20
	ldp	q4,q5, [sp], #0x20
	ldp	q2,q3, [sp], #0x20
	ldp	q0,q1, [sp], #0x20

	ldp X29,X30, [sp], #0x10
	ldp X18,X19, [sp], #0x10
	ldp	X16,X17, [sp], #0x10
	ldp	X14,X15, [sp], #0x10
	ldp	X12,X13, [sp], #0x10
	ldp	X10,X11, [sp], #0x10
	ldp	X8,X9, [sp], #0x10
	ldp	X6,X7, [sp], #0x10
	ldp	X4,X5, [sp], #0x10
	ldp	X2,X3, [sp], #0x10
	ldp	X0,X1, [sp], #0x10
.endm


.org 0

.section .vectors, "a"

.align 11
_vector_table:
.set	VBAR, _vector_table
.org 	VBAR

	b	_boot
.org (VBAR + 0x200)
#if defined(CONFIG_ENABLE_GDB_STUB)
	b	GdbDebugInterruptHandler
#else
	b	SynchronousInterruptHandler
#endif

.org (VBAR + 0x280)
	b	IRQInterruptHandler

.org (VBAR + 0x300)
	b	FIQInterruptHandler

.org (VBAR + 0x380)
	b	SErrorInterruptHandler

SynchronousInterruptHandler:
	SaveRegister
#ifndef CONFIG_USE_EL2
	mrs 	x0, CPACR_EL1
	mrs 	x1, ELR_EL1
	mrs		x2, SPSR_EL1
#else
	mrs 	x0, CPACR_EL1
	mrs 	x1, ELR_EL2
	mrs		x2, SPSR_EL2
#endif
	stp	    x0, x1, [sp,#-0x10]!
	mov		x21, sp 	
	add		x21, x21, #EXCEPTION_FRAME_SIZE 
	stp		x2,x21, [sp,#-0x10]!
	mov 	x0, sp

	bl		SynchronousInterrupt

	eret


IRQInterruptHandler:

	SaveRegister
	/* Save the status of SPSR, ELR and CPTR to stack */
#ifndef CONFIG_USE_EL2
	mrs 	x0, CPACR_EL1
	mrs 	x1, ELR_EL1
	mrs		x2, SPSR_EL1
#else
	mrs 	x0, CPACR_EL1
	mrs 	x1, ELR_EL2
	mrs		x2, SPSR_EL2
#endif

	stp	x0, x1, [sp,#-0x10]!
	str	x2, [sp,#-0x10]!

	MRS   X0, S3_0_C12_C12_0 /* ICCIAR in x0 as parameter. */

	/* Maintain the ICCIAR value across the function call. */
	STP		X0, X1, [SP, #-0x10]!

	bl	FExceptionInterruptHandler

	/* Restore the ICCIAR value. */
	LDP		X0, X1, [SP], #0x10

	/* Use barrier instructions to ensure ordering of memory operations. */
	dsb	 	sy
	/* End IRQ processing by writing ICCIAR to the EOI register. */
	MSR   S3_0_C12_C12_1, x0
	isb

	/* Restore the status of SPSR, ELR and CPTR from stack */
	ldr	x2,[sp],0x10
	ldp	x0, x1, [sp],0x10

#ifndef CONFIG_USE_EL2
	msr	CPACR_EL1, x0
	msr	ELR_EL1, x1
	msr	SPSR_EL1, x2
#else
	msr	CPACR_EL1, x0
	msr	ELR_EL2, x1
	msr	SPSR_EL2, x2
#endif

	RestoreRegister
	eret

FIQInterruptHandler:

	SaveRegister

	bl	FIQInterrupt

	RestoreRegister

	eret

SErrorInterruptHandler:

	SaveRegister
	/* Save the status of SPSR, ELR and CPTR to stack */
#ifndef CONFIG_USE_EL2
	mrs 	x0, CPACR_EL1
	mrs 	x1, ELR_EL1
	mrs		x2, SPSR_EL1
#else
	mrs 	x0, CPACR_EL1
	mrs 	x1, ELR_EL2
	mrs		x2, SPSR_EL2
#endif
	stp	    x0, x1, [sp,#-0x10]!
	mov		x21, sp 	
	add		x21, x21, #EXCEPTION_FRAME_SIZE
	stp		x21,x21, [sp,#-0x10]!
	mov 	x0, sp

	mov 	x0, sp
	bl      SErrorInterrupt

	RestoreRegister

	eret


.align 8


.end