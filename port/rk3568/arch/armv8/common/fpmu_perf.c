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
 * FilePath: fpmu_perf.c
 * Created Date: 2023-10-31 08:59:04
 * Last Modified: 2024-11-22 15:28:12
 * Description:  This file is for pmu uses a specific implementation of the api
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 * 1.0      huanghe     2023-11-10      first release
 * 1.1     zhangyan   2024/08/15       fix misra_c_2012_rule_10_7
 */
#include <string.h>
#include <string.h>
#include "fpmu.h"
#include "fpmu_perf.h"
#include "fbitmap.h"
#include "fdebug.h"
#include "fassert.h"

#define FPMU_DEBUG_TAG          "FPMU"
#define FPMU_ERROR(format, ...) FT_DEBUG_PRINT_E(FPMU_DEBUG_TAG, format, ##__VA_ARGS__)
#define FPMU_DEBUG_I(format, ...) \
    FT_DEBUG_PRINT_I(FPMU_DEBUG_TAG, format, ##__VA_ARGS__)
#define FPMU_DEBUG_D(format, ...) \
    FT_DEBUG_PRINT_D(FPMU_DEBUG_TAG, format, ##__VA_ARGS__)
#define FPMU_DEBUG_W(format, ...) \
    FT_DEBUG_PRINT_W(FPMU_DEBUG_TAG, format, ##__VA_ARGS__)
#define FPMU_DEBUG_E(format, ...) \
    FT_DEBUG_PRINT_E(FPMU_DEBUG_TAG, format, ##__VA_ARGS__)

typedef struct
{
    const char *mnemonic;
    const char *description;
} EventInfo;


static EventInfo events[] = {
    {"SW_INCR", "Instruction architecturally executed, Condition code check pass, "
                "software increment"},
    {"L1I_CACHE_REFILL", "Attributable Level 1 instruction cache refill"},
    {"L1I_TLB_REFILL", "Attributable Level 1 instruction TLB refill"},
    {"L1D_CACHE_REFILL", "Attributable Level 1 data cache refill"},
    {"L1D_CACHE", "Attributable Level 1 data cache access"},
    {"L1D_TLB_REFILL", "Attributable Level 1 data TLB refill"},
    {"LD_RETIRED",
     "Instruction architecturally executed, Condition code check pass, load"},
    {"ST_RETIRED",
     "Instruction architecturally executed, Condition code check pass, store"},
    {"INST_RETIRED", "Instruction architecturally executed"},
    {"EXC_TAKEN", "Exception taken"},
    {"EXC_RETURN", "Instruction architecturally executed, Condition code check pass, "
                   "exception return"},
    {"CID_WRITE_RETIRED", "Instruction architecturally executed, Condition code check "
                          "pass, write to CONTEXTIDR"},
    {"PC_WRITE_RETIRED", "Instruction architecturally executed, Condition code check "
                         "pass, software change of the PC"},
    {"BR_IMMED_RETIRED", "Instruction architecturally executed, immediate branch"},
    {"BR_RETURN_RETIRED", "Instruction architecturally executed, Condition code check "
                          "pass, procedure return"},
    {"UNALIGNED_LDST_RETIRED", "Instruction architecturally executed, Condition code "
                               "check pass, unaligned load or store"},
    {"BR_MIS_PRED", "Mispredicted or not predicted branch speculatively executed"},
    {"CPU_CYCLES", "Cycle"},
    {"BR_PRED", "Predictable branch speculatively executed"},
    {"MEM_ACCESS", "Data memory access"},
    {"L1I_CACHE", "Attributable Level 1 instruction cache access"},
    {"L1D_CACHE_WB", "Attributable Level 1 data cache write-back"},
    {"L2D_CACHE", "Attributable Level 2 data cache access"},
    {"L2D_CACHE_REFILL", "Attributable Level 2 data cache refill"},
    {"L2D_CACHE_WB", "Attributable Level 2 data cache write-back"},
    {"BUS_ACCESS", "Bus access"},
    {"MEMORY_ERROR", "Local memory error"},
    {"INST_SPEC", "Operation speculatively executed"},
    {"TTBR_WRITE_RETIRED",
     "Instruction architecturally executed, Condition code check pass, write to TTBR"},
    {"BUS_CYCLES", "Bus cycle"},
    {"CHAIN", "For odd-numbered counters, increments the count by one for each "
              "overflow of the preceding even-numbered counter. For even-numbered "
              "counters, there is no increment."},
    {"L1D_CACHE_ALLOCATE", "Attributable Level 1 data cache allocation without refill"},
    {"L2D_CACHE_ALLOCATE", "Attributable Level 2 data cache allocation without refill"},
    {"BR_RETIRED", "Instruction architecturally executed, branch"},
    {"BR_MIS_PRED_RETIRED",
     "Instruction architecturally executed, mispredicted branch"},
    {"STALL_FRONTEND", "No operation issued due to the frontend"},
    {"STALL_BACKEND", "No operation issued due to backend"},
    {"L1D_TLB", "Attributable Level 1 data or unified TLB access"},
    {"L1I_TLB", "Attributable Level 1 instruction TLB access"},
    {"L2I_CACHE", "Attributable Level 2 instruction cache access"},
    {"L2I_CACHE_REFILL", "Attributable Level 2 instruction cache refill"},
    {"L3D_CACHE_ALLOCATE",
     "Attributable Level 3 data or unified cache allocation without refill"},
    {"L3D_CACHE_REFILL", "Attributable Level 3 data or unified cache refill"},
    {"L3D_CACHE", "Attributable Level 3 data or unified cache access"},
    {"L3D_CACHE_WB", "Attributable Level 3 data or unified cache write-back"},
    {"L2D_TLB_REFILL", "Attributable Level 2 data or unified TLB refill"},
    {"L2I_TLB_REFILL", "Attributable Level 2 instruction TLB refill"},
    {"L2D_TLB", "Attributable Level 2 data or unified TLB access"},
    {"L2I_TLB", "Attributable Level 2 instruction TLB access"}};


/**
 * @name: FPmuPrintEventBitmap
 * @msg: Prints information for each bit set in the event_id_bitmap that corresponds to an event.
 * @param {const FBitPerWordType*} event_id_bitmap - Pointer to the bitmap representing event IDs.
 * @return {void}
 */
static void FPmuPrintEventBitmap(const FBitPerWordType *event_id_bitmap)
{
    const u32 bit_length = sizeof(FBitPerWordType) * 8;
    for (u32 i = 0; i < bit_length; i++)
    {

        if (event_id_bitmap[i / bit_length] & (1U << (i % bit_length)) &&
            i < sizeof(events) / sizeof(EventInfo))
        {
            FPMU_DEBUG_D("Event Number: 0x%04x, Event Mnemonic: %s, Description: %s\n",
                         i, events[i].mnemonic, events[i].description);
        }
        else if (event_id_bitmap[i / bit_length] & (1U << (i % bit_length)))
        {
            FPMU_DEBUG_D("Event Number: 0x%04zx, Event Mnemonic: Unknown, Description: "
                         "Unknown\n",
                         i);
        }
    }
}

/**
 * @name: FPmuFeatureProbeDebugPrint
 * @msg: Prints debug information for the FPmu instance, including PMU version, number of counters, and event ID bitmaps.
 * @param {const FPmu*} instance_p - Pointer to the FPmu instance to debug print.
 * @return {void}
 */
void FPmuFeatureProbeDebugPrint(const FPmu *instance_p)
{
    if (instance_p == NULL)
    {
        FPMU_DEBUG_E("FPmu instance is NULL\n");
        return;
    }

    /* Print PMU version */

    /* Print number of counters */
    FPMU_DEBUG_D("Number of Counters: %u\n", instance_p->counter_num);

    /* Print raw event ID bitmaps */
    FPMU_DEBUG_D("Raw Event ID 0: 0x%lx\n", instance_p->event_id_bitmap);

    /* Print extended event ID bitmaps */
    FPMU_DEBUG_D("Extended Event ID 0: 0x%llx\n", instance_p->event_ext_id_bitmap);

    FPmuPrintEventBitmap(&instance_p->event_id_bitmap);
}


/**
 * @name: FPmuFeatureProbe
 * @msg: Probes the PMU features and initializes the FPmu instance accordingly.
 * @param {FPmu*} instance_p - Pointer to the FPmu instance to be probed and initialized.
 * @return {FError} - Status of the feature probe operation.
 */
static FError FPmuFeatureProbe(FPmu *instance_p)
{
    u32 pmceid[2];


    instance_p->counter_num = PMCR_GET_N(FPmuPmcrRead());
    /* Add  Performance Monitors Cycle Counter */
    instance_p->counter_num++;

    /* updata event id  */
    pmceid[0] = FPmuPmceid0();
    pmceid[1] = FPmuPmceid1();

    FBitMapCopyClearTail(&instance_p->event_id_bitmap, (const FBitPerWordType *)pmceid,
                         FPMU_EVENTID_MASK_LENGTH);

    return FPMU_SUCCESS;
}

/**
 * @name: FPmuReset
 * @msg: Resets the PMU control registers and counter interrupt enables to a known state.
 * @param {FPmu*} instance_p - Pointer to the FPmu instance to reset.
 * @return {void}
 */
void FPmuReset(FPmu *instance_p)
{
    u32 pmcr;

    FPmuDisableCounterMask(FPMCNT_ENCLR_ALL_MASK);

    FPmuDisableIntens(PMINT_ENCLR_ALL_MASK);

    /*
	 * Initialize & Reset PMNC. Request overflow interrupt for
	 * 64bit cycle counter .
    */
    pmcr = FPMU_PMCR_P | FPMU_PMCR_C | FPMU_PMCR_LC;

    FPmuPmcrWrire(pmcr);
}

/**
 * @name: FPmuCfgInitialize
 * @msg: Initializes the FPmu configuration, checks features, resets the PMU controller, and sets the component as ready.
 * @param {FPmu*} instance_p - Pointer to the FPmu instance to initialize.
 * @return {FError} - Status of the initialization process.
 */
FError FPmuCfgInitialize(FPmu *instance_p)
{
    FError ret;
    memset(instance_p, 0, sizeof(FPmu));
    /* feature check */
    ret = FPmuFeatureProbe(instance_p);
    if (ret != FPMU_SUCCESS)
    {
        FPMU_DEBUG_E("Soc not support pmu");
        return ret;
    }

    /* pmu controler reset */
    FPmuReset(instance_p);
    instance_p->is_ready = FT_COMPONENT_IS_READY;

    return FPMU_SUCCESS;
}

/**
 * @name: FPmuCounterConfig
 * @msg: Configures a counter in the FPmu instance for a specific event ID and period count.
 * @param {FPmu*} instance_p - Pointer to the FPmu instance to configure.
 * @param {u32} counter_id - ID of the counter to configure.
 * @param {u32} event_id - Event ID to associate with the counter.
 * @return {FError} - Status of the counter configuration operation.
 */
FError FPmuCounterConfig(FPmu *instance_p, u32 counter_id, u32 event_id,
                         FPmuEventCB irq_cb, void *args)
{
    FASSERT(instance_p != NULL);
    FASSERT(instance_p->is_ready == FT_COMPONENT_IS_READY);
    /* check counter is vaild */
    if ((counter_id >= instance_p->counter_num) && (counter_id != FPMU_CYCLE_COUNT_IDX))
    {

        return FPMU_COUNTER_NOT_SUPPORT;
    }

    if ((((1UL << event_id) & instance_p->event_id_bitmap) == 0) &&
        (((1UL << event_id) & instance_p->event_ext_id_bitmap) == 0))
    {
        return FPMU_EVENT_ID_NOT_SUPPORT;
    }

    instance_p->counter[counter_id].event_id = event_id;
    instance_p->counter[counter_id].event_type_config = 0;
    instance_p->counter[counter_id].event_cb = irq_cb;
    instance_p->counter[counter_id].args = args;

    return FPMU_SUCCESS;
}

/**
 * @name: FPmuCounterEnable
 * @msg: Enables a configured counter within the FPmu instance.
 * @param {FPmu*} instance_p - Pointer to the FPmu instance.
 * @param {u32} counter_id - ID of the counter to enable.
 * @return {FError} - Status of the counter enable operation.
 */
FError FPmuCounterEnable(FPmu *instance_p, u32 counter_id)
{
    FASSERT(instance_p != NULL);
    FASSERT(instance_p->is_ready == FT_COMPONENT_IS_READY);

    if ((counter_id >= instance_p->counter_num) && (counter_id != FPMU_CYCLE_COUNT_IDX))
    {
        return FPMU_COUNTER_NOT_SUPPORT;
    }

    if ((1UL << counter_id) & instance_p->counter_used_bitmap)
    {
        return FPMU_COUNTER_HAS_USED;
    }
    /*
    * Disable counter
    */

    FPmuDisableCounterMask(1 << counter_id);


    /*
    * Set event.
    */
    if (FPMU_CYCLE_COUNT_IDX == counter_id)
    {
        FPmuPmccfiltrSet(instance_p->counter[counter_id].event_type_config);
    }
    else
    {
        FPmuWriteEventType(counter_id, instance_p->counter[counter_id].event_type_config |
                                           (instance_p->counter[counter_id].event_id & FPMU_EVTYPE_EVENT));
    }

    /*
    * Enable interrupt for this counter
    */
    FPmuEnableEventIrq(1 << counter_id);

    /* Enable counter  */
    FPmuEnableCounter(1 << counter_id);

    FBitMapSet(&instance_p->counter_used_bitmap, counter_id);

    return FPMU_SUCCESS;
}

/**
 * @name: FPmuReadCounter
 * @msg: Reads the value of a specific counter from the FPmu instance.
 * @param {FPmu*} instance_p - Pointer to the FPmu instance.
 * @param {u32} counter_id - ID of the counter to read.
 * @param {u64*} value - Pointer to store the read counter value.
 * @return {FError} - Status of the counter read operation.
 */
FError FPmuReadCounter(FPmu *instance_p, u32 counter_id, u64 *value)
{

    FASSERT(instance_p != NULL);
    FASSERT(instance_p->is_ready == FT_COMPONENT_IS_READY);

    if ((counter_id >= instance_p->counter_num) && (counter_id != FPMU_CYCLE_COUNT_IDX))
    {
        return FPMU_COUNTER_NOT_SUPPORT;
    }

    if (((1UL << counter_id) & instance_p->counter_used_bitmap) == 0)
    {
        return FPMU_COUNTER_NOT_ENABLE;
    }

    if (counter_id == FPMU_CYCLE_COUNT_IDX)
    {
        *value = FPmuPmccntrGet();
    }
    else
    {
        *value = FPmuReadCycleCnt(counter_id);
    }

    return FPMU_SUCCESS;
}

/**
 * @name: FPmuWriteCounter
 * @msg: Writes a value to a specific counter within the FPmu instance.
 * @param {FPmu*} instance_p - Pointer to the FPmu instance.
 * @param {u32} counter_id - ID of the counter to write to.
 * @param {u64} value - Value to write to the counter.
 * @return {FError} - Status of the counter write operation.
 * @NOTE   In AArch32, when using the FPmuWriteCounter interface, if the counter_id is not FPMU_CYCLE_COUNT_IDX, then the value should be a 32-bit data. 
 */
FError FPmuWriteCounter(FPmu *instance_p, u32 counter_id, u64 value)
{

    FASSERT(instance_p != NULL);
    FASSERT(instance_p->is_ready == FT_COMPONENT_IS_READY);

    if ((counter_id >= instance_p->counter_num) && (counter_id != FPMU_CYCLE_COUNT_IDX))
    {
        return FPMU_COUNTER_NOT_SUPPORT;
    }

    if (((1UL << counter_id) & instance_p->counter_used_bitmap) == 0)
    {
        return FPMU_COUNTER_NOT_ENABLE;
    }

    if (counter_id == FPMU_CYCLE_COUNT_IDX)
    {
        FPmuPmccntrSet(value);
    }
    else
    {
        FPmuWriteCycleCnt(counter_id, value);
    }

    return FPMU_SUCCESS;
}


/**
 * @name: FPmuCounterDisable
 * @msg: Disables a counter within the FPmu instance to stop counting events.
 * @param {FPmu*} instance_p - Pointer to the FPmu instance.
 * @param {u32} counter_id - ID of the counter to disable.
 * @return {FError} - Status of the counter disable operation.
 */
FError FPmuCounterDisable(FPmu *instance_p, u32 counter_id)
{
    FASSERT(instance_p != NULL);
    FASSERT(instance_p->is_ready == FT_COMPONENT_IS_READY);

    if ((counter_id >= instance_p->counter_num) && (counter_id != FPMU_CYCLE_COUNT_IDX))
    {
        return FPMU_COUNTER_NOT_SUPPORT;
    }

    if (((1UL << counter_id) & instance_p->counter_used_bitmap) == 0)
    {
        return FPMU_COUNTER_HAS_USED;
    }

    /*
    * Disable counter
    */
    FPmuDisableCounterMask(1 << counter_id);


    /*
	 * Disable interrupt for this counter
	 */
    FPmuDisableIntens(1 << counter_id);

    return FPMU_SUCCESS;
}


/**
 * @name: FPmuStart
 * @msg: Starts the Performance Monitoring Unit by enabling all the counters.
 * @return {void}
 */
void FPmuStart(void)
{
    FPmuPmcrWrire(FPmuPmcrRead() | FPMU_PMCR_E);
}

/**
 * @name: FPmuStop
 * @msg: Stops the Performance Monitoring Unit by disabling all the counters.
 * @return {void}
 */
void FPmuStop(void)
{
    FPmuPmcrWrire(FPmuPmcrRead() & ~FPMU_PMCR_E);
}

/**
 * @name: FPmuIrqHandler
 * @msg: Handles interrupts from the Performance Monitoring Unit, checks for counter overflows, and invokes callbacks.
 * @param {s32} vector - The interrupt vector number.
 * @param {void*} args - Pointer to user-defined data, expected to be an FPmu instance.
 * @return {void}
 */
void FPmuIrqHandler(s32 vector, void *args)
{
    FPmu *instance_p = (FPmu *)args;
    FASSERT(instance_p != NULL);
    FASSERT(instance_p->is_ready == FT_COMPONENT_IS_READY);
    u32 overflowed_flg;
    /*
	* Get and reset the IRQ flags
	*/
    overflowed_flg = FPmuGetRestIrqFlags();

    /* 
     * Did an overflow occur?
    */
    if (!overflowed_flg)
    {
        FPMU_DEBUG_W("No overflow occur ");
        return;
    }

    /*
    * Stop the PMU while processing the counter overflows
    * to prevent skews in group events.
    */
    FPmuStop();
    if (overflowed_flg & (1 << FPMU_CYCLE_COUNT_IDX))
    {
        if (instance_p->counter[FPMU_CYCLE_COUNT_IDX].event_cb)
        {
            instance_p->counter[FPMU_CYCLE_COUNT_IDX]
                .event_cb(instance_p->counter[FPMU_CYCLE_COUNT_IDX].args);
        }
    }
    else
    {
        for (size_t i = 0; i < instance_p->counter_num; i++)
        {
            if (((1 << i) & instance_p->counter_used_bitmap) == 0)
            {
                continue;
            }

            if (((1 << i) & overflowed_flg) == 0)
            {
                continue;
            }

            if (instance_p->counter[i].event_cb)
            {
                instance_p->counter[i].event_cb(instance_p->counter[i].args);
            }
        }
    }


    FPmuStart();
}

/**
 * @name: FPmuDebugCounterIncrease
 * @msg: Increments the specified performance counter for the given PMU instance. It validates the PMU instance and counter index before attempting to increment the counter.
 * @param {FPmu *} instance_p - Pointer to the FPmu instance whose counter needs to be incremented.
 * @param {u32} counter_id - The ID of the counter to increment. This ID should be within the range of available counters for the PMU instance, or be the special cycle count index.
 * @return {FError} - Returns an error code if the instance pointer is NULL, the component is not ready, the counter ID is not supported, or the counter has already been used. Returns FPMU_SUCCESS on success.
 */
FError FPmuDebugCounterIncrease(FPmu *instance_p, u32 counter_id)
{
    FASSERT(instance_p != NULL);
    FASSERT(instance_p->is_ready == FT_COMPONENT_IS_READY);

    if ((counter_id >= instance_p->counter_num) && (counter_id != FPMU_CYCLE_COUNT_IDX))
    {
        return FPMU_COUNTER_NOT_SUPPORT;
    }

    if (((1UL << counter_id) & instance_p->counter_used_bitmap) == 0)
    {
        return FPMU_COUNTER_HAS_USED;
    }

    FPmuPmswincSet(1UL << counter_id);
    return FPMU_SUCCESS;
}

/**
 * @name: FPmuCheckCounterEventId
 * @msg: Checks and retrieves the event type associated with a specified counter in a given PMU instance. Validates the PMU instance and counter index before proceeding. The function handles special cases for cycle count indexes and regular counter indexes differently.
 * @param {FPmu *} instance_p - Pointer to the FPmu instance whose counter's event type is to be checked.
 * @param {u32} counter_id - The ID of the counter whose event type is being queried. This ID should be within the range of available counters for the PMU instance or be the special cycle count index.
 * @return {u32} - Returns the type of the event associated with the counter. Returns FPMU_COUNTER_NOT_SUPPORT if the counter ID is not supported, and FPMU_COUNTER_HAS_USED if the counter has already been used.
 */
u32 FPmuCheckCounterEventId(FPmu *instance_p, u32 counter_id)
{
    u32 type;
    FASSERT(instance_p != NULL);
    FASSERT(instance_p->is_ready == FT_COMPONENT_IS_READY);

    if ((counter_id >= instance_p->counter_num) && (counter_id != FPMU_CYCLE_COUNT_IDX))
    {
        return FPMU_COUNTER_NOT_SUPPORT;
    }

    if (((1UL << counter_id) & instance_p->counter_used_bitmap) == 0)
    {
        return FPMU_COUNTER_HAS_USED;
    }

    if (counter_id == FPMU_CYCLE_COUNT_IDX)
    {
        type = FPmuPmccfiltrGet();
    }
    else
    {
        type = FPmuReadEventType(counter_id) & FPMU_EVTYPE_EVENT;
    }

    return type;
}


u32 FPmuGetIrqFlags(void)
{
    return FPmuPmovsclrGet();
}
