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
 * FilePath: fpmu_perf.h
 * Created Date: 2023-11-06 11:33:18
 * Last Modified: 2023-11-13 19:33:06
 * Description:  This file is for pmu uses a specific implementation of the api
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 * 1.0      huanghe     2023-11-10      first release
 */
#ifndef FPMU_PERF_H
#define FPMU_PERF_H

#include "ftypes.h"
#include "fbitmap.h"
#include "ferror_code.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define FPMU_SUCCESS FT_SUCCESS
#define FPMU_SOC_NOT_SURPPORT \
    FT_MAKE_ERRCODE(ErrorModGeneral, ErrPmu, 1) /* 错误选择CTLR 寄存器 */
#define FPMU_EVENT_ID_NOT_SUPPORT      FT_MAKE_ERRCODE(ErrorModGeneral, ErrPmu, 2)
#define FPMU_COUNTER_NOT_SUPPORT       FT_MAKE_ERRCODE(ErrorModGeneral, ErrPmu, 3)
#define FPMU_COUNTER_HAS_USED          FT_MAKE_ERRCODE(ErrorModGeneral, ErrPmu, 4)
#define FPMU_COUNTER_NOT_ENABLE        FT_MAKE_ERRCODE(ErrorModGeneral, ErrPmu, 5)

#define FPMU_SW_INCR_ID                0x0
#define FPMU_L1I_CACHE_REFILL_ID       0x1
#define FPMU_L1I_TLB_REFILL_ID         0x2
#define FPMU_L1D_CACHE_REFILL_ID       0x3
#define FPMU_L1D_CACHE_ID              0x4
#define FPMU_L1D_TLB_REFILL_ID         0x5
#define FPMU_LD_RETIRED_ID             0x6
#define FPMU_ST_RETIRED_ID             0x7
#define FPMU_INST_RETIRED_ID           0x8
#define FPMU_EXC_TAKEN_ID              0x9
#define FPMU_EXC_RETURN_ID             0xA
#define FPMU_CID_WRITE_RETIRED_ID      0xB
#define FPMU_PC_WRITE_RETIRED_ID       0xC
#define FPMU_BR_IMMED_RETIRED_ID       0xD
#define FPMU_BR_RETURN_RETIRED_ID      0xE
#define FPMU_UNALIGNED_LDST_RETIRED_ID 0xF
#define FPMU_BR_MIS_PRED_ID            0x10
#define FPMU_CPU_CYCLES_ID             0x11
#define FPMU_BR_PRED_ID                0x12
#define FPMU_MEM_ACCESS_ID             0x13
#define FPMU_L1I_CACHE_ID              0x14
#define FPMU_L1D_CACHE_WB_ID           0x15
#define FPMU_L2D_CACHE_ID              0x16
#define FPMU_L2D_CACHE_REFILL_ID       0x17
#define FPMU_L2D_CACHE_WB_ID           0x18
#define FPMU_BUS_ACCESS_ID             0x19
#define FPMU_MEMORY_ERROR_ID           0x1A
#define FPMU_INST_SPEC_ID              0x1B
#define FPMU_TTBR_WRITE_RETIRED_ID     0x1C
#define FPMU_BUS_CYCLES_ID             0x1D
#define FPMU_CHAIN_ID                  0x1E
#define FPMU_L1D_CACHE_ALLOCATE_ID     0x1F
#define FPMU_L2D_CACHE_ALLOCATE_ID     0x20
#define FPMU_BR_RETIRED_ID             0x21
#define FPMU_BR_MIS_PRED_RETIRED_ID    0x22
#define FPMU_STALL_FRONTEND_ID         0x23
#define FPMU_STALL_BACKEND_ID          0x24
#define FPMU_L1D_TLB_ID                0x25
#define FPMU_L1I_TLB_ID                0x26
#define FPMU_L2I_CACHE_ID              0x27
#define FPMU_L2I_CACHE_REFILL_ID       0x28
#define FPMU_L3D_CACHE_ALLOCATE_ID     0x29
#define FPMU_L3D_CACHE_REFILL_ID       0x2A
#define FPMU_L3D_CACHE_ID              0x2B
#define FPMU_L3D_CACHE_WB_ID           0x2C
#define FPMU_L2D_TLB_REFILL_ID         0x2D
#define FPMU_L2I_TLB_REFILL_ID         0x2E
#define FPMU_L2D_TLB_ID                0x2F
#define FPMU_L2I_TLB_ID                0x30


#define FPMU_CYCLE_COUNT_IDX           31 /* Performance Monitors Cycle Count index ,the last  */
#define FPMU_EVENT_COUNT0_IDX          0
#define FPMU_EVENT_COUNT1_IDX          1
#define FPMU_EVENT_COUNT2_IDX          2
#define FPMU_EVENT_COUNT3_IDX          3
#define FPMU_EVENT_COUNT4_IDX          4
#define FPMU_EVENT_COUNT5_IDX          5


#define FPMU_EVENT_MAX_NUM             (32)
#define FPMU_EVENTID_MASK_LENGTH       64

#define CCNT_FULL                      0xFFFFFFFFU
#define FPMU_PERIOD_CALC_32BIT(period) (CCNT_FULL - period)

#define CCNT_FULL_64BIT                (-1ULL)
#define FPMU_PERIOD_CALC_64BIT(period) (CCNT_FULL_64BIT - period)


#define PMUVER_SHIFT                   24
#define PMUVER_MASK                    0x0F000000
#define GET_PMUVER(x)                  ((x & PMUVER_MASK) >> PMUVER_SHIFT)


typedef void (*FPmuEventCB)(void *args);

struct FPmuCounter
{
    u32 event_id;
    u32 event_type_config;
    FPmuEventCB event_cb;
    void *args;
};

typedef struct
{
    u32 is_ready;

    FBitPerWordType event_id_bitmap;
    FBitPerWordType event_ext_id_bitmap;

    u32 counter_num;
    FBitPerWordType counter_used_bitmap;
    struct FPmuCounter counter[FPMU_EVENT_MAX_NUM];
} FPmu;

void FPmuReset(FPmu *instance_p);

FError FPmuCfgInitialize(FPmu *instance_p);

FError FPmuCounterConfig(FPmu *instance_p, u32 counter_id, u32 event_id,
                         FPmuEventCB irq_cb, void *args);

FError FPmuCounterEnable(FPmu *instance_p, u32 counter_id);

FError FPmuReadCounter(FPmu *instance_p, u32 counter_id, u64 *value);

FError FPmuWriteCounter(FPmu *instance_p, u32 counter_id, u64 value);

FError FPmuCounterDisable(FPmu *instance_p, u32 counter_id);

void FPmuStart(void);

void FPmuStop(void);

void FPmuIrqHandler(s32 vector, void *args);

FError FPmuDebugCounterIncrease(FPmu *instance_p, u32 counter_id);

void FPmuFeatureProbeDebugPrint(const FPmu *instance_p);

u32 FPmuCheckCounterEventId(FPmu *instance_p, u32 counter_id);

u32 FPmuGetIrqFlags(void);

#ifdef __cplusplus
}
#endif

#endif
