/*
 * Copyright (C) 2025, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fboot.h
 * Created Date: 2025-03-19 10:32:06
 * Last Modified: 2025-03-19 10:33:08
 * Description:  This file is for
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 */
#ifndef FBOOT_H
#define FBOOT_H

#ifdef __ASSEMBLER__

/* SCTLR related macros & defines */
#define SCTLR_TE_MASK                   0x40000000
#define SCTLR_AFE_MASK                  0x20000000
#define SCTLR_TRE_MASK                  0x10000000
#define SCTLR_NFI_MASK                  0x08000000
#define SCTLR_EE_MASK                   0x02000000
#define SCTLR_VE_MASK                   0x01000000
#define SCTLR_U_MASK                    0x00400000
#define SCTLR_FI_MASK                   0x00200000
#define SCTLR_HA_MASK                   0x00020000
#define SCTLR_RR_MASK                   0x00004000
#define SCTLR_V_MASK                    0x00002000
#define SCTLR_I_MASK                    0x00001000
#define SCTLR_Z_MASK                    0x00000800
#define SCTLR_SW_MASK                   0x00000400
#define SCTLR_B_MASK                    0x00000080
#define SCTLR_C_MASK                    0x00000004
#define SCTLR_A_MASK                    0x00000002
#define SCTLR_M_MASK                    0x00000001

/* HCR_EL2 */
#define HCR_INITVAL                     0x000000000
#define HCR_FWB_MASK                    _AC(0x400000000000, UL)
#define HCR_FWB_SHIFT                   46
#define HCR_APL_MASK                    _AC(0x20000000000, UL)
#define HCR_APL_SHIFT                   41
#define HCR_APK_MASK                    _AC(0x10000000000, UL)
#define HCR_APK_SHIFT                   40
#define HCR_TEA_MASK                    _AC(0x2000000000, UL)
#define HCR_TEA_SHIFT                   37
#define HCR_TERR_MASK                   _AC(0x1000000000, UL)
#define HCR_TERR_SHIFT                  36
#define HCR_TLOR_MASK                   _AC(0x800000000, UL)
#define HCR_TLOR_SHIFT                  35
#define HCR_E2H_MASK                    _AC(0x400000000, UL)
#define HCR_E2H_SHIFT                   34
#define HCR_ID_MASK                     _AC(0x200000000, UL)
#define HCR_ID_SHIFT                    33
#define HCR_CD_MASK                     _AC(0x100000000, UL)
#define HCR_CD_SHIFT                    32
#define HCR_RW_MASK                     0x080000000
#define HCR_RW_SHIFT                    31
#define HCR_TRVM_MASK                   0x040000000
#define HCR_TRVM_SHIFT                  30
#define HCR_HCD_MASK                    0x020000000
#define HCR_HCD_SHIFT                   29
#define HCR_TDZ_MASK                    0x010000000
#define HCR_TDZ_SHIFT                   28
#define HCR_TGE_MASK                    0x008000000
#define HCR_TGE_SHIFT                   27
#define HCR_TVM_MASK                    0x004000000
#define HCR_TVM_SHIFT                   26
#define HCR_TTLB_MASK                   0x002000000
#define HCR_TTLB_SHIFT                  25
#define HCR_TPU_MASK                    0x001000000
#define HCR_TPU_SHIFT                   24
#define HCR_TPC_MASK                    0x000800000
#define HCR_TPC_SHIFT                   23
#define HCR_TSW_MASK                    0x000400000
#define HCR_TSW_SHIFT                   22
#define HCR_TACR_MASK                   0x000200000
#define HCR_TACR_SHIFT                  21
#define HCR_TIDCP_MASK                  0x000100000
#define HCR_TIDCP_SHIFT                 20
#define HCR_TSC_MASK                    0x000080000
#define HCR_TSC_SHIFT                   19
#define HCR_TID3_MASK                   0x000040000
#define HCR_TID3_SHIFT                  18
#define HCR_TID2_MASK                   0x000020000
#define HCR_TID2_SHIFT                  17
#define HCR_TID1_MASK                   0x000010000
#define HCR_TID1_SHIFT                  16
#define HCR_TID0_MASK                   0x000008000
#define HCR_TID0_SHIFT                  15
#define HCR_TWE_MASK                    0x000004000
#define HCR_TWE_SHIFT                   14
#define HCR_TWI_MASK                    0x000002000
#define HCR_TWI_SHIFT                   13
#define HCR_DC_MASK                     0x000001000
#define HCR_DC_SHIFT                    12
#define HCR_BSU_MASK                    0x000000C00
#define HCR_BSU_SHIFT                   10
#define HCR_FB_MASK                     0x000000200
#define HCR_FB_SHIFT                    9
#define HCR_VSE_MASK                    0x000000100
#define HCR_VSE_SHIFT                   8
#define HCR_VI_MASK                     0x000000080
#define HCR_VI_SHIFT                    7
#define HCR_VF_MASK                     0x000000040
#define HCR_VF_SHIFT                    6
#define HCR_AMO_MASK                    0x000000020
#define HCR_AMO_SHIFT                   5
#define HCR_IMO_MASK                    0x000000010
#define HCR_IMO_SHIFT                   4
#define HCR_FMO_MASK                    0x000000008
#define HCR_FMO_SHIFT                   3
#define HCR_PTW_MASK                    0x000000004
#define HCR_PTW_SHIFT                   2
#define HCR_SWIO_MASK                   0x000000002
#define HCR_SWIO_SHIFT                  1
#define HCR_VM_MASK                     0x000000001
#define HCR_VM_SHIFT                    0

#define HCR_DEFAULT_BITS                (HCR_AMO_MASK | HCR_IMO_MASK | HCR_FMO_MASK)


#define FPSR_F_BIT         0x00000040
#define FPSR_I_BIT         0x00000080
#define FPSR_A_BIT         0x00000100
#define FPSR_D_BIT         0x00000200

#define FPSR_MODE_EL0t     0x00000000
#define FPSR_MODE_EL1t     0x00000004
#define FPSR_MODE_EL1h     0x00000005
#define FPSR_MODE_EL2t     0x00000008
#define FPSR_MODE_EL2h     0x00000009
#define FPSR_MODE_SVC_32   0x00000013


.macro disable_mmu sctlr tmp
    mrs     \tmp, \sctlr
    bic     \tmp, \tmp, #(1 << 0)
    bic     \tmp, \tmp, #(1 << 2)
    bic     \tmp, \tmp, #(1 << 12)
    msr     \sctlr, \tmp
    isb
.endm

.macro dcache op
    dsb     sy
    mrs     x0, clidr_el1
    and     x3, x0, #0x7000000
    lsr     x3, x3, #23

    cbz     x3, finished_\op
    mov     x10, #0

loop1_\op:
    add     x2, x10, x10, lsr #1
    lsr     x1, x0, x2
    and     x1, x1, #7
    cmp     x1, #2
    b.lt    skip_\op

    msr     csselr_el1, x10
    isb

    mrs     x1, ccsidr_el1
    and     x2, x1, #7
    add     x2, x2, #4
    mov     x4, #0x3ff
    and     x4, x4, x1, lsr #3
    clz     w5, w4
    mov     x7, #0x7fff
    and     x7, x7, x1, lsr #13

loop2_\op:
    mov     x9, x4

loop3_\op:
    lsl     x6, x9, x5
    orr     x11, x10, x6
    lsl     x6, x7, x2
    orr     x11, x11, x6
    dc      \op, x11
    subs    x9, x9, #1
    b.ge    loop3_\op
    subs    x7, x7, #1
    b.ge    loop2_\op

skip_\op:
    add     x10, x10, #2
    cmp     x3, x10
    b.gt    loop1_\op

finished_\op:
    mov     x10, #0
    msr     csselr_el1, x10
    dsb     sy
    isb
.endm

#else /* !__ASSEMBLER__ */
#warning "Including assembly-specific header in C code"
#endif
#endif // !