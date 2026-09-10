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
 * FilePath: fgdb_thread.c
 * Created Date: 2024-08-27 17:05:14
 * Last Modified: 2024-08-27 17:34:17
 * Description:  This file is for gdb function implmentation
 * 
 * Modify History:
 *   Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0   zhugengyu   2025/8/12    init
 */

/***************************** Include Files *********************************/
#include "fdebug.h"
#include "fassert.h"
#include "fgdb.h"
#include "fgdb_arch.h"
#include "fgdb_def.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
#define FGDB_DEBUG_TAG          "FGDB"
#define FGDB_ERROR(format, ...) FT_DEBUG_PRINT_E(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_WARN(format, ...)  FT_DEBUG_PRINT_W(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_INFO(format, ...)  FT_DEBUG_PRINT_I(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_DEBUG(format, ...) FT_DEBUG_PRINT_D(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)

/************************** Function Prototypes ******************************/
void gdb_do_qsupported(FGdb *const gdb, uint32_t *feat);
void gdb_do_mem_search(FGdb *const gdb);
/*****************************************************************************/
_WEAK u32 FGdbCurrentThreadId(void)
{
    return 1U;
}

_WEAK void FGdbSetThread(FGdb *const gdb)
{
    gdb_tx_ok(gdb);
}

_WEAK void FGdbAckThreadInfo(FGdb *const gdb)
{
    gdb_tx_empty(gdb);
}

_WEAK void FGdbSystemQuery(FGdb *const gdb)
{
    uint32_t host_features;

    if (gdb_rx_equal(gdb, "C"))
    {
        gdb_tx_begin(gdb, 'Q');
        gdb_tx_char(gdb, 'C');
        gdb_tx_varhex(gdb, (uintmax_t)FGdbCurrentThreadId());
        gdb_tx_end(gdb);
    }
    else if (gdb_rx_equal(gdb, "Supported"))
    {
        gdb_do_qsupported(gdb, &host_features);
    }
    else if (gdb_rx_equal(gdb, "fThreadInfo"))
    {
        gdb_tx_empty(gdb);
    }
    else if (gdb_rx_equal(gdb, "sThreadInfo"))
    {
        gdb_tx_empty(gdb);
    }
    else if (gdb_rx_equal(gdb, "Xfer:"))
    {
        gdb_tx_err(gdb, 0);
    }
    else if (gdb_rx_equal(gdb, "Search:memory:"))
    {
        gdb_do_mem_search(gdb);
    }
    else if (!gdb_cpu_query())
    {
        gdb_tx_empty(gdb);
    }
}