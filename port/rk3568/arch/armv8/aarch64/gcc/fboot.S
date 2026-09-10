/*
 * Copyright (C) 2022, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fboot.S
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2024-11-26 19:34:50
 * Description:  This file is for 64bit Processor Boot Code
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 *  1.0  huanghe	2021/11/13		initialization
 *  1.1  zhugengyu	2022/06/05		add debugging information
 *  1.2  zhugengyu  2023/02/22      support dcache flush before bootup
 *  1.3  huanghe    2024-11-26      add el2 and relocate 
 */

#include "sdkconfig.h"
#include "fboot.h"

.globl _boot
.global _vector_table
.global _vector_el2_table




.set vector_base, _vector_table
.set el1_stack, __el1_stack

#if defined(CONFIG_USE_ACPICA)
.global rsdp_addr

.section .rodata
.align 3  /* Align to 8 bytes */
rsdp_addr:
    .quad 0
#endif

.section .boot,"ax"

_boot:
#if defined(CONFIG_USE_ACPICA)
        adr x8, rsdp_addr
        ldr x1, [x8]
        cbnz x1, rsdp_already_set
        
        str x0, [x8] /* set rsdp */

rsdp_already_set:
#endif
        bl  FTraceUartInit
        bl  FTraceBootup
        mrs	x0, CurrentEL
        cmp x0, 0xc
        b.eq    El3Entry
        cmp	x0, 0x8
#ifdef CONFIG_USE_EL2
        b.eq	EL2HyperEntry
#else
        b.eq	El2Entry
#endif
        cmp	x0, 0x4
        b.eq	El1Entry
        bne	hang
    El3Entry:
        /*  initialize sctlr_el2 and hcr_el2 to save values before entering el2.*/
        msr sctlr_el2, xzr
        msr hcr_el2, xzr
        /*  determine the el2 execution state.*/
        mrs x0, scr_el3
        orr x0, x0, #(1<<10) 
        /*  rw el2 execution state is aarch64.*/
        orr x0, x0, #(1<<0) 
        /*  ns el1 is non-secure world.*/
        msr scr_el3, x0
        mov x0, #0b01001 
        /*  daif=0000 */
        msr spsr_el3, x0 
        /*  m[4:0]=01001 el2h must match scr_el3.rw*/

        bl  FTraceEL3
        /*  determine el2 entry.*/
        adr x0, El2Entry 
        /*  El2Entry points to the first instruction of */
        msr elr_el3, x0 
        /*  el2 code.*/
        eret
    El2Entry:
        dcache  cisw /* flush dcache  */

        /* Ensure I-cache, D-cache and mmu are disabled for EL2/Stage1 */
        disable_mmu sctlr_el2, x9

        ic      iallu
        dsb     nsh
        isb

        /* Ensure I-cache, D-cache and mmu are disabled for EL1/Stage2 */
        mov     x9, #(1 << 31)
        msr     hcr_el2, x9

        /* Ensure traps to EL2 are disabled */
        mov     x9, #0x33ff
        msr     cptr_el2, x9
        msr     hstr_el2, xzr
        msr     vttbr_el2, xzr

        /* Ensure I-cache, D-cache and mmu are disabled for EL1/Stage1 */
        disable_mmu sctlr_el1 , x9

        mov     x9, #(FPSR_F_BIT | FPSR_I_BIT | FPSR_A_BIT | FPSR_D_BIT | FPSR_MODE_EL1h)
        msr     spsr_el2, x9

        /* access generic timer registers */
        mov x0, 0x3
        msr CNTHCTL_EL2, x0

        bl  FTraceEL2
        adr x0, El1Entry 
        /*  El1Entry points to the first instruction of */
        msr elr_el2, x0 
        /*  el1 code.*/
        eret

EL2HyperEntry:
    msr sctlr_el2, xzr
    isb

    mrs x0, hcr_el2
    orr x0, x0, #(1<<31) 
    orr x0, x0, #(1<<27) /* HCR_EL2.TGE = 1 */
    orr x0, x0, #(1<<5)  /* HCR_EL2.AMO = 1 */
    orr x0, x0, #(1<<4)  /* HCR_EL2.IMO = 1 */
    orr x0, x0, #(1<<3)  /* HCR_EL2.FMO = 1 */
    msr hcr_el2, x0

    mov x0,0
    msr CNTHCTL_EL2, x0

    ldr x1,=vector_base
    msr VBAR_EL2,x1

    /* floating point access does not cause execution of any instructions to be trapped. */
    mrs	x0, CPACR_EL1
	orr	x0, x0, #(0x3 << 20)
	msr	CPACR_EL1, x0
    isb

    ldr x2,___el2_stack
    mov	 sp,x2

    bl   FTraceEL2
    bl 	 _startup		/* jump to start */

El1Entry:
InitEl1:
    mov      x0, #0
	mov      x1, #0
	mov      x2, #0
	mov      x3, #0
	mov      x4, #0
	mov      x5, #0
	mov      x6, #0
	mov      x7, #0
	mov      x8, #0
	mov      x9, #0
	mov      x10, #0
	mov      x11, #0
	mov      x12, #0
	mov      x13, #0
	mov      x14, #0
	mov      x15, #0
	mov      x16, #0
	mov      x17, #0
	mov      x18, #0
	mov      x19, #0
	mov      x20, #0
	mov      x21, #0
	mov      x22, #0
	mov      x23, #0
	mov      x24, #0
	mov      x25, #0
	mov      x26, #0
	mov      x27, #0
	mov      x28, #0
	mov      x29, #0
	mov      x30, #0
    mov	    x1,#0x0
    msr     SCTLR_EL1, x1
    isb
    
    /* Set vector table base address */
    ldr x1,=vector_base
    msr VBAR_EL1,x1

    /* floating point access does not cause execution of any instructions to be trapped. */
    mrs	x0, CPACR_EL1
	orr	x0, x0, #(0x3 << 20)
	msr	CPACR_EL1, x0
    isb

    /* Define stack pointer for current exception level */
    ldr x2,___el1_stack
    mov	 sp,x2

    bl   FTraceEL1
    bl 	 _startup		/* jump to start */
hang:
	wfi
    b   HangPrint
	b	hang

/* 
* Load start address storage
*/
.align 3
text_region_start:
    .dword __text_region_start
__image_end:
    .dword _code_end

___el1_stack:
    .dword __el1_stack
___el2_stack:
    .dword __el2_stack
