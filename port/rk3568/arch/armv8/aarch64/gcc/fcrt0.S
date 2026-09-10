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
 * FilePath: fcrt0.S
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:31:05
 * Description:  This file is for 64bit C run-time code
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 *  1.0  huanghe   2021/11/13       initialization
 *  1.1  zhugengyu	2022/06/05	add debugging information
 *  1.2  zhugengyu 2023/02/23   support dcache flush for early boot
 */
#include "sdkconfig.h"
#include "fboot.h"

    .globl crt_entry

    .section ".got2","aw"
    .align 2

    .text

    .align 4	
.LsbssStart:
    .quad __sbss_start__

.LsbssEnd:
    .quad __sbss_end__


.LbssStart:
    .quad __bss_start__

.LbssEnd:
    .quad __bss_end__


.LdataStart:
    .quad __data_start__

.LdataEnd:
    .quad  __data_end__

.LRomEnd:
    .quad __rom_end__

    .globl _startup
_startup:
    /* Mask all exceptions first: bootloader (go) may leave IRQ/FIQ
     * unmasked, and interrupt subsystems are not initialized yet. */
    msr DAIFSet, #0xf
    TLBI    VMALLE1
    ic      IALLU   /* Invalidate I cache to PoU */
	bl 		InvalidateFlushDcaches
	dsb	 	sy
	isb

	/* Disable MMU first */
	mov	x1,#0x0
	msr     SCTLR_EL1, x1
	isb

    mov x0,#0

/* clear sbss */

ldr x1,.LsbssStart
ldr x2,.LsbssEnd

.LloopsBss:
	cmp	x1,x2
	bge	.LenclsBss		    /* If no SBSS, no clearing required */
	str	x0, [x1], #8
	b	.LloopsBss

.LenclsBss:
    /* clear bss */
	ldr	x1,.LbssStart		/* calculate beginning of the BSS */
	ldr	x2,.LbssEnd		    /* calculate end of the BSS */

.LloopBss:
	cmp	x1,x2
	bge	.LenclBss		/* If no BSS, no clearing required */
	str	x0, [x1], #8
	b	.LloopBss

.LenclBss:
    /* Current bootelf image model loads .data directly at its run address.
     * Keep source == __data_start__ unless the linker script later uses AT().
     */
    ldr x0,.LdataStart
    ldr x1,.LdataStart
    ldr x2,.LdataEnd

.LdataCopy:
    ldp x10,x11,[x0],#16 /* copy from source address [x1] */
    stp x10,x11,[x1],#16 /* copy from source address [x1] */
    cmp x1,x2
    blt .LdataCopy

.LendataCopy:

#ifdef CONFIG_USE_EL2
    /* enable IRQ & FIQ */
	msr	DAIFCLR, #3

    /* Setup HCR */
    ldr	x0,=__hcr_set
    ldr x0, [x0]
	msr	hcr_el2, x0

    /* Disable EL2 MMU/caches if the bootloader (e.g. u-boot) left them on.
     * MmuInit requires SCTLR_EL2.M == 0 (FASSERT in fmmu.c). */
    mrs     x3, sctlr_el2
    tbz     x3, #0, .LmmuOffDone
    dsb     sy
    msr     sctlr_el2, xzr
    isb
    mov     x0, #0
    bl      InvalidateFlushDcaches
    tlbi    alle2
    dsb     sy
    isb
.LmmuOffDone:

	bl MmuInit

    ldr	x1,=sctlr_el2_or
    ldr x1, [x1]

    ldr	x0,=sctlr_el2_bic
    ldr x0, [x0]
    
    mrs x2, sctlr_el2			/* read control register */

    bic	x2, x2, x0			/* clear bits them */
	orr	x2, x2, x1			/* set them */
    msr sctlr_el2 ,x2
    dsb	sy
	isb
#else

	bl MmuInit
    /*Enable MMU */ 
	mov	x1,#0x0
	orr     x1, x1, #(1 << 18)     /* Set WFE non trapping */
	orr     x1, x1, #(1 << 17)     /* Set WFI non trapping */
	orr     x1, x1, #(1 << 5)      /* Set CP15 barrier enabled */
	orr     x1, x1, #(1 << 14)     /* This control does not cause any instructions to be trapped. */
	orr     x1, x1, #(1 << 12)     /* Set I bit */
	orr     x1, x1, #(1 << 2)      /* Set C bit */
	orr     x1, x1, #(1 << 0)      /* Set M bit */
	msr     SCTLR_EL1, x1
	isb
#endif

crt_entry:
#ifndef CONFIG_USE_EL2
    ldr x2,___el1_stack
#else
    ldr x2,___el2_stack
#endif
    mov	 sp,x2

    bl FEarlyUartProbe        /* init early uart */
	bl InterruptEarlyInit
    bl FPsciInit
#ifdef CONFIG_ENABLE_FREBOOT
    bl FEarlyReboot                  /* init and start watchdog */
#endif
#ifdef CONFIG_ENABLE_DMA_REGION
    bl FDmaRegionInit
#endif
    bl FBootCore
    /* Enable SError Exception for asynchronous abort */
	mrs x1,DAIF
    bic	x1,x1,#(0x1<<8) /* clear SError interrupt mask bit */
	bic	x1,x1,#(0x1<<7)	/* clear IRQ mask bit */
    msr	DAIF,x1

#ifdef CONFIG_ENABLE_CXX
    /* Call constructors of all cpp global objects */
    bl  FCxxInitGlobals
#endif

	bl FTraceCEntry
	
    /* make sure argc and argv are valid */
	mov	x0, #0
	mov	x1, #0
	
    bl main

#ifdef CONFIG_ENABLE_CXX
    /* Call deconstructors of all cpp global objects */
    bl  FCxxDeInitGlobals
#endif

/* print after main.c end */
.weak FEndPrint
.weak FEndReboot

.Lexit: /* should never get here */
    b FEndPrint
    b FEndReboot
.end:
    b .end

/* flush or invalidate dcache, refer to rt-thread __asm_flush_dcache_all */
InvalidateFlushDcacheLevel:
    lsl    x12, x0, #1
    msr    csselr_el1, x12       /* select cache level */
    isb                          /* sync change of cssidr_el1 */
    mrs    x6, ccsidr_el1        /* read the new cssidr_el1 */
    and    x2, x6, #7            /* x2 <- log2(cache line size)-4 */
    add    x2, x2, #4            /* x2 <- log2(cache line size) */
    mov    x3, #0x3ff			 
    and    x3, x3, x6, lsr #3    /* x3 <- max number of #ways, ccsidr_el1[12:3] */
    clz    w5, w3                /* bit position of #ways */
    mov    x4, #0x7fff
    and    x4, x4, x6, lsr #13    /* x4 <- max number of #sets, ccsidr_el1[27:13] */
    /* x12 <- cache level << 1 */
    /* x2 <- line length offset */
    /* x3 <- number of cache ways - 1 */
    /* x4 <- number of cache sets - 1 */
    /* x5 <- bit position of #ways */

InvalidateFlushCacheSet:
    mov    x6, x3            /* x6 <- working copy of #ways */	

InvalidateFlushCacheWay:
    lsl    x7, x6, x5		  /* x7 = x6 << x5 */
    orr    x9, x12, x7        /* x9 = x12 | x7,  map way and level to cisw value */
    lsl    x7, x4, x2
    orr    x9, x9, x7        /* map set number to cisw value */
    tbz    w1, #0, 1f		 /* x1 = 0f, flush cache */
    dc    isw, x9			 /* invalidate dcache */
    b    2f
1:    dc    cisw, x9         /* clean & invalidate by set/way */
2:    subs    x6, x6, #1        /* decrement the way */
    b.ge    InvalidateFlushCacheWay
    subs    x4, x4, #1        /* decrement the set */
    b.ge    InvalidateFlushCacheSet

    ret

.global InvalidateFlushDcaches
InvalidateFlushDcaches:
    mov    x1, x0                /* x1 = 0 flush, x1 = 1 invalidate */
    dsb    sy                    /* barrier for full system */
    mrs    x10, clidr_el1        /* read clidr_el1 */
    lsr    x11, x10, #24
    and    x11, x11, #0x7        /* x11 <- loc bit[26:24], level of cache hierarchy */
    cbz    x11, InvalidateFlushDcacheEnd        /* if loc is 0, no cache, exit */
    mov    x15, lr           /* preserve LR */
    mov    x0, #0            /* start flush at cache level 0 */
    /* x0  <- cache level */
    /* x10 <- clidr_el1 */
    /* x11 <- loc */
    /* x15 <- return address */

InvalidateFlushCachesLoopLevel:
    lsl    x12, x0, #1         /* x12 = x0 * 2 */
    add    x12, x12, x0        /* x0 <- tripled cache level */
    lsr    x12, x10, x12       /* get x10, clidr_el1[ctype-n] to x12 */
    and    x12, x12, #7        /* x12 <- cache type */
    cmp    x12, #2             /* if not 000(no-cache), 001(i-cache only) */
    b.lt   InvalidateFlushCachesSkipLevel /* skip if no cache or icache */
    bl     InvalidateFlushDcacheLevel     /* x1 = 0 flush, 1 invalidate */

InvalidateFlushCachesSkipLevel:
    add    x0, x0, #1        /* increment cache level */
    cmp    x11, x0
    b.gt   InvalidateFlushCachesLoopLevel

    mov    x0, #0
    msr    csselr_el1, x0        /* restore csselr_el1 */
    dsb    sy
    isb
    mov    lr, x15

InvalidateFlushDcacheEnd:
	ret

.Lstart:
	.size	_startup,.Lstart-_startup


.align 3
__hcr_set:
	.dword HCR_DEFAULT_BITS
/* Or bit is mmu enable,Data access Cacheability control, Instruction access Cacheability control */
sctlr_el2_or:
    .dword 0x1005 
/* bic bit is Alignment check disable , SP Alignment check disable , accesses to the CTR_EL0 not to el2 */
sctlr_el2_bic:
    .dword 0x8000A
___el1_stack:
    .dword __el1_stack
___el2_stack:
    .dword __el2_stack
