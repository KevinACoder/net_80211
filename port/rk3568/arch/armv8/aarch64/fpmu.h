/*
 * Copyright (C) 2023, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fpmu.h
 * Created Date: 2023-10-31 08:59:10
 * Last Modified: 2024-06-03 17:55:23
 * Description:  This file is for
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0      huanghe     2023/11/10      first release
 */

#ifndef FPMU_H
#define FPMU_H

#include "ftypes.h"
#include "ferror_code.h"

#include "faarch.h"
#include "fdebug.h"


#ifdef __cplusplus
extern "C"
{
#endif
#define FPMU_AARCH64_DEBUG_TAG "FPMU_AARCH64"
#define FPMU_AARCH64_DEBUG_W(format, ...) \
    FT_DEBUG_PRINT_W(FPMU_AARCH64_DEBUG_TAG, format, ##__VA_ARGS__)


/*
 * PMXEVTYPER: Event selection reg
 */
#define FPMU_EVTYPE_MASK                0xc800ffff /* Mask for writable bits */
#define FPMU_EVTYPE_EVENT               0xffff     /* Mask for EVENT bits */


#define FPMU_ID_AA64DFR0_PMUVER_8_0     0x1
#define FPMU_ID_AA64DFR0_PMUVER_8_1     0x4
#define FPMU_ID_AA64DFR0_PMUVER_8_4     0x5
#define FPMU_ID_AA64DFR0_PMUVER_8_5     0x6
#define FPMU_ID_AA64DFR0_PMUVER_IMP_DEF 0xf

#define FPMCNT_ENCLR_ALL_MASK           (~0U)
#define PMINT_ENCLR_ALL_MASK            (~0U)


#define FPMU_PMCR_E                     (1 << 0) /* Enable all counters */
#define FPMU_PMCR_P                     (1 << 1) /* Reset all counters */
#define FPMU_PMCR_C                     (1 << 2) /* Cycle counter reset */
#define FPMU_PMCR_D                     (1 << 3) /* CCNT counts every 64th cpu cycle */
#define FPMU_PMCR_X                     (1 << 4) /* Export to ETM */
#define FPMU_PMCR_DP                    (1 << 5) /* Disable CCNT if non-invasive debug*/
#define FPMU_PMCR_LC                    (1 << 6) /* Overflow on 64 bit cycle counter */
#define FPMU_PMCR_LP                    (1 << 7) /* Long event counter enable */
#define PMCR_N_SHIFT                    11
#define PMCR_N_MASK                     0xf800
#define PMCR_GET_N(x)                   ((x & PMCR_N_MASK) >> PMCR_N_SHIFT)
#define FPMU_PMCR_MASK                  0xff /* Mask for writable bits */


#define FPMU_CASE(n, case_macro) \
    case n:                      \
        case_macro(n);           \
        break

#define FPMU_SWITCH(x, case_macro)                             \
    do                                                         \
    {                                                          \
        switch (x)                                             \
        {                                                      \
            FPMU_CASE(0, case_macro);                          \
            FPMU_CASE(1, case_macro);                          \
            FPMU_CASE(2, case_macro);                          \
            FPMU_CASE(3, case_macro);                          \
            FPMU_CASE(4, case_macro);                          \
            FPMU_CASE(5, case_macro);                          \
            FPMU_CASE(6, case_macro);                          \
            FPMU_CASE(7, case_macro);                          \
            FPMU_CASE(8, case_macro);                          \
            FPMU_CASE(9, case_macro);                          \
            FPMU_CASE(10, case_macro);                         \
            FPMU_CASE(11, case_macro);                         \
            FPMU_CASE(12, case_macro);                         \
            FPMU_CASE(13, case_macro);                         \
            FPMU_CASE(14, case_macro);                         \
            FPMU_CASE(15, case_macro);                         \
            FPMU_CASE(16, case_macro);                         \
            FPMU_CASE(17, case_macro);                         \
            FPMU_CASE(18, case_macro);                         \
            FPMU_CASE(19, case_macro);                         \
            FPMU_CASE(20, case_macro);                         \
            FPMU_CASE(21, case_macro);                         \
            FPMU_CASE(22, case_macro);                         \
            FPMU_CASE(23, case_macro);                         \
            FPMU_CASE(24, case_macro);                         \
            FPMU_CASE(25, case_macro);                         \
            FPMU_CASE(26, case_macro);                         \
            FPMU_CASE(27, case_macro);                         \
            FPMU_CASE(28, case_macro);                         \
            FPMU_CASE(29, case_macro);                         \
            FPMU_CASE(30, case_macro);                         \
            default:                                           \
                FPMU_AARCH64_DEBUG_W("Invalid PMEV* index\n"); \
        }                                                      \
    } while (0)

#define WRITE_FPMUEVTYPERN(n) \
    AARCH64_WRITE_SYSREG(pmevtyper##n##_el0, event_type_config)

static inline void FPmuWriteEventType(u32 counter_id, u32 event_type_config)
{
    event_type_config &= FPMU_EVTYPE_MASK;

    FPMU_SWITCH(counter_id, WRITE_FPMUEVTYPERN);
}

#define READ_FPMUEVTYPERN(n) \
    return (AARCH64_READ_SYSREG(pmevtyper##n##_el0) & FPMU_EVTYPE_MASK)

static inline u32 FPmuReadEventType(u32 counter_id)
{
    FPMU_SWITCH(counter_id, READ_FPMUEVTYPERN);
    return 0;
}


#define RETURN_READ_PMEVCNTR(n) return AARCH64_READ_SYSREG(pmevcntr##n##_el0)

static inline u32 FPmuReadCycleCnt(u32 counter_id)
{
    FPMU_SWITCH(counter_id, RETURN_READ_PMEVCNTR);
    return 0;
}

#define RETURN_WRITE_PMEVCNTR(n) return AARCH64_WRITE_SYSREG(pmevcntr##n##_el0, value)
static inline void FPmuWriteCycleCnt(u32 counter_id, u32 value)
{
    FPMU_SWITCH(counter_id, RETURN_WRITE_PMEVCNTR);
}


static inline void FPmuEnableEventIrq(u32 mask)
{
    AARCH64_WRITE_SYSREG(pmintenset_el1, mask);
}


static inline u32 FPmuGetEventIrq(void)
{
    return AARCH64_READ_SYSREG(pmintenset_el1);
}

static inline void FPmuEnableCounter(u32 mask)
{
    ISB();
    AARCH64_WRITE_SYSREG(pmcntenset_el0, mask);
}

static inline void FPmuDisableCounterMask(u32 mask)
{
    /* The counter and interrupt enable registers are unknown at reset. */
    AARCH64_WRITE_SYSREG(pmcntenclr_el0, mask);

    /*
	 * Make sure the effects of disabling the counter are visible before we
	 * start configuring the event.
	 */
    ISB();
}


static inline void FPmuDisableIntens(u32 mask)
{
    AARCH64_WRITE_SYSREG(pmintenclr_el1, mask);

    ISB();

    AARCH64_WRITE_SYSREG(pmovsclr_el0, mask);
    ISB();
}

static inline void FPmuPmcrWrire(u32 pmcr)
{
    pmcr &= FPMU_PMCR_MASK;
    ISB();
    AARCH64_WRITE_SYSREG(pmcr_el0, pmcr);
}

static inline u32 FPmuPmcrRead(void)
{
    return AARCH64_READ_SYSREG(pmcr_el0);
}

static inline u64 FPmuEdfrRead(void)
{
    return AARCH64_READ_SYSREG(ID_DFR0_EL1);
}


static inline u64 FPmuPmceid0(void)
{
    return AARCH64_READ_SYSREG(pmceid0_el0);
}


static inline u64 FPmuPmceid1(void)
{
    return AARCH64_READ_SYSREG(pmceid1_el0);
}

static inline void FPmuPmccfiltrSet(u32 event_type_config)
{
    AARCH64_WRITE_SYSREG(pmccfiltr_el0, event_type_config);
}

static inline u32 FPmuPmccfiltrGet(void)
{
    return AARCH64_READ_SYSREG(PMCCFILTR_EL0);
}


static inline void FPmuPmccntrSet(u64 value)
{
    AARCH64_WRITE_SYSREG(pmccntr_el0, value);
}


static inline unsigned long FPmuPmccntrGet(void)
{
    return AARCH64_READ_SYSREG(pmccntr_el0);
}

static inline void FPmuPmswincSet(u32 mask)
{
    AARCH64_WRITE_SYSREG(PMSWINC_EL0, mask);
}


static inline u32 FPmuPmovsclrGet(void)
{
    return AARCH64_READ_SYSREG(pmovsclr_el0);
}

/**
 * @name: FPmuGetRestIrqFlags
 * @msg: Retrieves and clears the IRQ flags indicating which PMU counters have overflowed.
 * @return {u32} - The IRQ flags for overflowed counters.
 */
static inline u32 FPmuGetRestIrqFlags(void)
{
    u32 value;
    /* Read bit check for overflowed */
    value = (u32)AARCH64_READ_SYSREG(pmovsclr_el0);

    value &= 0xffffffff;
    AARCH64_WRITE_SYSREG(pmovsclr_el0, value);
    return value;
}


#ifdef __cplusplus
}
#endif

#endif
