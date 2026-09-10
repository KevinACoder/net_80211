/*
 * Copyright (C) 2026, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: freboot.c
 * Created Date: 2026-04-08
 * Description: Watchdog-based system restart implementation
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 * 1.0    LiuSM        2026-04-08    first release
 */

#include "fparameters.h"
#include "fgeneric_timer.h"
#include "fdebug.h"
#include "freboot.h"

#define WDT_INIT_TAG "FREBOOT"

#ifdef CONFIG_ENABLE_FREBOOT

#include "fwdt.h"
#define MAX_WDT_TIMEOUT_MS 87000
/* WDT control handle - global for early boot */
static FWdtCtrl g_wdt_ctrl;
static int g_wdt_inited = 0;

/**
 * @name: FWdtInit
 * @msg: Initialize WDT controller with given instance ID.
 * @param instance_id: WDT instance ID
 * @return: 0 on success, negative error code on failure
 */
static int FWdtInit(u32 instance_id)
{
    FError ret;
    FWdtConfig wdt_config;

    wdt_config = *FWdtLookupConfig(instance_id);
    if (wdt_config.instance_id >= FWDT_NUM)
    {
        FT_DEBUG_PRINT_E(WDT_INIT_TAG, "Invalid WDT instance ID: %d\r\n", instance_id);
        return -1;
    }

    ret = FWdtCfgInitialize(&g_wdt_ctrl, &wdt_config);
    if (FWDT_SUCCESS != ret)
    {
        FT_DEBUG_PRINT_E(WDT_INIT_TAG, "WDT initialize failed: %d\r\n", ret);
        return ret;
    }

    g_wdt_inited = 1;
    FT_DEBUG_PRINT_I(WDT_INIT_TAG, "WDT controller initialized, instance=%d\r\n", instance_id);
    return 0;
}

/**
 * @name: FWdtSetAndStart
 * @msg: Set watchdog timeout and start the timer.
 * @param timeout_ms: Watchdog timeout in milliseconds (0 = immediate restart, minimum 1)
 * @return: 0 on success, negative error code on failure
 */
static int FWdtSetAndStart(u32 timeout_ms)
{
    FError ret;
    u32 timeout;

    /* Calculate timeout value */
    if (timeout_ms == 0)
    {
        timeout = 1;
    }
    else
    {
        timeout = (u32)(GenericTimerFrequecy() * timeout_ms / 1000);
    }

    /* Set watchdog timeout and start */
    ret = FWdtSetTimeout(&g_wdt_ctrl, timeout);
    if (FWDT_SUCCESS != ret)
    {
        FT_DEBUG_PRINT_E(WDT_INIT_TAG, "WDT set timeout failed: %d\r\n", ret);
        return ret;
    }

    ret = FWdtStart(&g_wdt_ctrl);
    if (FWDT_SUCCESS != ret)
    {
        FT_DEBUG_PRINT_E(WDT_INIT_TAG, "WDT start failed: %d\r\n", ret);
        return ret;
    }

    FT_DEBUG_PRINT_I(WDT_INIT_TAG, "WDT started, timeout=%d ms\r\n",
                     timeout_ms == 0 ? 1000 : timeout_ms);
    return 0;
}

/**
 * @name: FEarlyReboot
 * @msg: Initialize and start watchdog timer during boot process.
 *        Uses FREBOOT_INSTANCE_ID and FREBOOT_TIMEOUT_MS from config.
 *        If not fed within the timeout, system will reset.
 * @return: 0 on success, negative error code on failure
 */
int FEarlyReboot(void)
{
    int ret;

    ret = FWdtInit(CONFIG_FREBOOT_INSTANCE_ID);
    if (ret != 0)
    {
        return ret;
    }

    return FWdtSetAndStart(CONFIG_FREBOOT_TIMEOUT_MS);
}

/**
 * @name: FRebootSetDelayMs
 * @msg: Set watchdog timeout for system restart.
 *        If delay_ms is 0, the watchdog timer will be refreshed.
 * @param delay_ms: Watchdog timeout in milliseconds (0 = immediate restart, minimum 1)
 * @return: 0 on success, negative error code on failure
 */
int FRebootSetDelayMs(u32 delay_ms)
{
    int ret;
    u32 timeout;
    if (g_wdt_inited == 0)
    {
        FT_DEBUG_PRINT_E(WDT_INIT_TAG, "FRebootSetDelayMs: WDT not initialized\r\n");
        return -1;
    }
    if (delay_ms > MAX_WDT_TIMEOUT_MS)
    {
        FT_DEBUG_PRINT_E(WDT_INIT_TAG, "FRebootSetDelayMs: delay_ms out of range\r\n");
        return -1;
    }
    else if (delay_ms == 0) /* default delay_ms*/
    {
        FWdtRefresh(&g_wdt_ctrl);
        return 0;
    }

    timeout = (u32)(GenericTimerFrequecy() * delay_ms / 1000);
    ret = FWdtSetTimeout(&g_wdt_ctrl, timeout);
    if (FWDT_SUCCESS != ret)
    {
        FT_DEBUG_PRINT_E(WDT_INIT_TAG, "WDT set timeout failed: %d\r\n", ret);
        return ret;
    }

    FT_DEBUG_PRINT_I(WDT_INIT_TAG, "FRebootSetDelayMs: delay_ms=%d ms\r\n", delay_ms);

    return 0;
}

/**
 * @name: FEndReboot
 * @msg: Trigger immediate system restart.
 *        Calls FRebootSetDelayMs with timeout_ms=1000 to wait 1s before restart.
 *        This function does not return.
 */
void FEndReboot(void)
{
    FRebootSetDelayMs(1000); /*等待1s标志打印后再重启*/
    while (1)
    {
        ;
    }
}
#endif