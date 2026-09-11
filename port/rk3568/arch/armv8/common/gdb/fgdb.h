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
 * FilePath: fgdb.h
 * Created Date: 2024-08-27 17:05:14
 * Last Modified: 2024-08-27 17:34:17
 * Description:  This file is for gdb stub function definition
 * 
 * Modify History:
 *   Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0   zhugengyu   2025/8/12    init
 */

#ifndef FGDB_H
#define FGDB_H

/***************************** Include Files *********************************/
#include "ftypes.h"

#ifdef __cplusplus
extern "C"
{
#endif
/************************** Constant Definitions *****************************/

/* GDB stub driver error code */
#define FGDB_SUCCESS FT_SUCCESS
#define FGDB_FAILED  ERR_GENERAL

typedef enum
{
    HW_BREAKPOINT_X = 0,
    HW_BREAKPOINT_R = 1,
    HW_BREAKPOINT_W = 2,
    HW_BREAKPOINT_RW = HW_BREAKPOINT_R | HW_BREAKPOINT_W,
} FGdbDebugAccessType;

typedef enum
{
    DBG_TYPE_BREAKPOINT = 0,
    DBG_TYPE_WATCHPOINT = 1,
} FGdbBreakType;

typedef enum
{
    DBG_PORT_UART = 0,
} FGdbDebugPortType;

/**************************** Type Definitions *******************************/
typedef struct FGdbTrapframe_ FGdbTrapframe;
typedef struct
{
    u32 dbg_enable_count;
    u32 dbg_flags;
#define DBGMON_ENABLED (1 << 0)
#define DBGMON_KERNEL  (1 << 1)
#define DBG_BRP_MAX    16
#define DBG_WRP_MAX    16
    u64 dbg_bcr[DBG_BRP_MAX];
    u64 dbg_bvr[DBG_BRP_MAX];
    u64 dbg_wcr[DBG_WRP_MAX];
    u64 dbg_wvr[DBG_WRP_MAX];
} FGdbDebugMonitorState;

typedef struct
{
    const char *gdb_name;
    int (*gdb_port_init)(void *);
    int (*gdb_port_getc)(void *);
    void (*gdb_port_putc)(void *, int);
    void (*gdb_port_intr_ctrl)(void *, boolean);
    void (*gdb_port_term)(void *);
    int gdb_active;
    void (*gdb_sendpacket)(const void *, size_t);
    int gdb_dbfeatures;
} FGdbDebugPort;

#define GDB_BUFSZ 4096

typedef union
{
    struct _midbuf
    {
        char mb_pad1;
        char mb_buf[GDB_BUFSZ];
        char mb_pad2[4];
    } __attribute__((__packed__)) txu_midbuf;
    /* sizeof includes trailing nul byte and this is intentional. */
    char txu_fullbuf[GDB_BUFSZ + sizeof("$#..")];
} FGdbTxBuffer;

typedef struct FGdb_
{
    int gdb_dbfeatures;

    FGdbDebugMonitorState monitor;
    int dbg_watchpoint_num;
    int dbg_breakpoint_num;

    FGdbDebugPort dbg_port;
    void *dbg_port_priv;
    int dbbe_active;

    FGdbTrapframe *gdb_frame;

    int gdb_listening;
    boolean gdb_ackmode;
    unsigned char gdb_bindata[64];
    char gdb_rxbuf[GDB_BUFSZ];
    char *gdb_rxp;
    size_t gdb_rxsz;

    FGdbTxBuffer gdb_tx_u;
    char *gdb_txp;

    void *kdb_jmpbufp;

    boolean compiled_brk_process;
} FGdb;

/************************** Variable Definitions *****************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/
FError FGdbInitialize(FGdb *const gdb, FGdbDebugPortType port, void *port_instance);
int FGdbTrap(FGdb *const gdb, int type, int code);

#ifdef __cplusplus
}
#endif

#endif /* FGDB_H */
