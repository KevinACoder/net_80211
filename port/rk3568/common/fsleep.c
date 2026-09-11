/*
 * Copyright (C) 2021, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fsleep.c
 * Date: 2021-07-01 18:40:52
 * LastEditTime: 2022-02-17 18:02:45
 * Description:  This file is for creating custom sleep interface for standlone sdk.
 *
 * Modify History:
 *  Ver     Who           Date                  Changes
 * -----   ------       --------     --------------------------------------
 *  1.0    huanghe      2022/7/23            first release
 */


#include "fsleep.h"
#include "fgeneric_timer.h"
#include "fkernel.h"
#include "fparameters.h"
#include "ftypes.h"
#include "sdkconfig.h"

#define TIMER_COUNT_MAX -1ULL

#if defined(CONFIG_USE_PHYSICAL_GTIMER)

#define SLEEP_USE_TIMER_INDEX GENERIC_TIMER_ID0

#elif defined(CONFIG_USE_VIRTUAL_GTIMER)

#define SLEEP_USE_TIMER_INDEX GENERIC_TIMER_ID1

#else

#error "Sleep error select "

#endif

static u32 fsleep_general(u32 delay_time, u32 resolution)
{
    volatile u64 cur_tick;
    volatile u64 old_tick;
    volatile u64 pass_tick = 0;
    volatile u64 need_tick;

    need_tick = ((u64)delay_time * GenericTimerFrequecy() / resolution);
    old_tick = GenericTimerRead(SLEEP_USE_TIMER_INDEX);

    while (pass_tick < need_tick)
    {
        cur_tick = GenericTimerRead(SLEEP_USE_TIMER_INDEX);

        if (cur_tick == old_tick)
        {
            continue;
        }
        else if (cur_tick > old_tick)
        {
            pass_tick += cur_tick - old_tick;
        }
        else
        {
            pass_tick += cur_tick + TIMER_COUNT_MAX - old_tick;
        }

        old_tick = cur_tick;
    }

    return 0;
}

u32 fsleep_seconds(u32 seconds)
{
    return fsleep_general(seconds, 1);
}

u32 fsleep_millisec(u32 mseconds)
{
    return fsleep_general(mseconds, NANO_TO_MICRO);
}

u32 fsleep_microsec(u32 mseconds)
{
    return fsleep_general(mseconds, NANO_TO_KILO);
}