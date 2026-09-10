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
 * FilePath: freboot.h
 * Created Date: 2026-04-08
 * Description: Watchdog-based system restart header
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 * 1.0    LiuSM        2026-04-08    first release
 */

#ifndef FREBOOT_H
#define FREBOOT_H

#include "ftypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @name: FEarlyReboot
 * @msg: Initialize and start watchdog timer during boot process.
 *        Uses FREBOOT_INSTANCE_ID and FREBOOT_TIMEOUT_MS from config.
 *        If not fed within the timeout, system will reset.
 * @return: 0 on success, negative error code on failure
 * @note: If CONFIG_ENABLE_FREBOOT is not defined, this
 *        function does nothing.
 */
int FEarlyReboot(void);

/**
 * @name: FRebootSetDelayMs
 * @msg: Set watchdog timeout for system restart in milliseconds.
 *        If delay_ms is 0, the watchdog timer will be refreshed.
 * @param delay_ms: Watchdog timeout in milliseconds (0 = refresh, minimum 1ms)
 * @return: 0 on success, negative error code on failure
 */
int FRebootSetDelayMs(u32 delay_ms);

/**
 * @name: FEndReboot
 * @msg: Trigger immediate system restart.
 *        Calls FRebootSetDelayMs with timeout_ms=1000 to wait 1s before restart.
 * @note: This function does not return.
 */
void FEndReboot(void);

#ifdef __cplusplus
}
#endif

#endif /* FREBOOT_H */