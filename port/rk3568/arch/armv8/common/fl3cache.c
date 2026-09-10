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
 * FilePath: fl3cache.c
 * Date: 2022-03-08 21:56:42
 * LastEditTime: 2022-03-15 11:10:40
 * Description:  This file is for l3 cache-related operations
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe    2022/03/15       first release
 * 1.1   zhangyan   2024/08/15       fix misra_c_2012_rule_10_7
 */


#include "fl3cache.h"
#include "sdkconfig.h"


/***************************** Include Files *********************************/

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/

/***************** Macros (Inline Functions) Definitions *********************/
/* FLUSH L3 CASHE */
#ifdef CONFIG_USE_L3CACHE
#define HNF_BASE          (unsigned long)(0x3A200000)
#define HNF_COUNT         0x8
#define HNF_PSTATE_REQ    (HNF_BASE + 0x10)
#define HNF_PSTATE_STAT   (HNF_BASE + 0x18)
#define HNF_PSTATE_OFF    0x0
#define HNF_PSTATE_SFONLY 0x1
#define HNF_PSTATE_HALF   0x2
#define HNF_PSTATE_FULL   0x3
#define HNF_STRIDE        0x10000UL
#endif

/************************** Function Prototypes ******************************/

void FCacheL3CacheDisable(void)
{
#ifdef CONFIG_USE_L3CACHE
    int i, pstate;


    for (i = 0; i < 8; i++)
    {
        FtOut32(0x3A200010 + i * 0x10000, 1);
    }

    for (i = 0; i < 8; i++)
    {
        do
        {
            pstate = FtIn32(0x3A200018 + i * 0x10000);
        } while ((pstate & 0xf) != (0x1 << 2));
    }
#endif
}


void FCacheL3CacheFlush(void)
{
#ifdef CONFIG_USE_L3CACHE
    int i, pstate;

    for (i = 0; i < HNF_COUNT; i++)
    {
        FtOut64(HNF_PSTATE_REQ + i * HNF_STRIDE, HNF_PSTATE_SFONLY);
    }
    for (i = 0; i < HNF_COUNT; i++)
    {
        do
        {
            pstate = FtIn64(HNF_PSTATE_STAT + i * HNF_STRIDE);
        } while ((pstate & 0xf) != (HNF_PSTATE_SFONLY << 2));
    }

    for (i = 0; i < HNF_COUNT; i++)
    {
        FtOut64(HNF_PSTATE_REQ + i * HNF_STRIDE, HNF_PSTATE_FULL);
    }

#endif
    return;
}


void FCacheL3CacheInvalidate(void)
{
#ifdef CONFIG_USE_L3CACHE
    int i, pstate;

    for (i = 0; i < HNF_COUNT; i++)
    {
        FtOut64(HNF_PSTATE_REQ + i * HNF_STRIDE, HNF_PSTATE_SFONLY);
    }

    for (i = 0; i < HNF_COUNT; i++)
    {
        do
        {
            pstate = FtIn64(HNF_PSTATE_STAT + i * HNF_STRIDE);
        } while ((pstate & 0xf) != (HNF_PSTATE_SFONLY << 2));
    }

    for (i = 0; i < HNF_COUNT; i++)
    {
        FtOut64(HNF_PSTATE_REQ + i * HNF_STRIDE, HNF_PSTATE_FULL);
    }
#endif
    return;
}


void FCacheL3CacheEnable(void)
{
    return;
}