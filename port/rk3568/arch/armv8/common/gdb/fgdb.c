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
 * FilePath: fgdb.c
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
#include "faarch.h"
#include "fgdb.h"
#include "fgdb_def.h"
#include "fgdb_arch.h"

/************************** Constant Definitions *****************************/
#define EINVAL                  22 /* Invalid argument */
#define EIO                     5  /* I/O error */
/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
#define FGDB_DEBUG_TAG          "FGDB"
#define FGDB_ERROR(format, ...) FT_DEBUG_PRINT_E(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_WARN(format, ...)  FT_DEBUG_PRINT_W(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_INFO(format, ...)  FT_DEBUG_PRINT_I(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_DEBUG(format, ...) FT_DEBUG_PRINT_D(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_RAW(format, ...)   FT_RAW_PRINTF(format, ##__VA_ARGS__)

/************************** Function Prototypes ******************************/
void FGdbRegisterInstance(FGdb *const gdb);

int FGdbUartDbgPortInit(void *instance);
int FGdbUartDbgPortGetC(void *instance);
void FGdbUartDbgPortPutC(void *instance, int ch);
void FGdbUartDbgPortIntrCtrl(void *instance, boolean enable);
void FGdbUartDbgPortTerminate(void *instance);

u32 FGdbCurrentThreadId(void);
void FGdbSetThread(FGdb *const gdb);
void FGdbSystemQuery(FGdb *const gdb);

void dbg_wb_write_reg(int reg, int n, uint64_t val);
void gdb_handle_detach(FGdb *const gdb);
void gdb_z_remove(FGdb *const gdb);
void gdb_z_insert(FGdb *const gdb);

/*****************************************************************************/
FError FGdbInitialize(FGdb *const gdb, FGdbDebugPortType port, void *port_instance)
{
    FASSERT(gdb);
    FError ret = FGDB_SUCCESS;
    uint64_t aa64dfr0;
    u_int i;

    /* Clear OS lock, dbg_init(void) */
    WRITE_SPECIALREG(oslar_el1, 0);

    /* This permits DDB to use debug registers for watchpoints. 
		dbg_monitor_init(void) */

    /* Find out many breakpoints and watchpoints we can use */
    aa64dfr0 = READ_SPECIALREG(id_aa64dfr0_el1);
    gdb->dbg_watchpoint_num = ID_AA64DFR0_WRPs_VAL(aa64dfr0);
    gdb->dbg_breakpoint_num = ID_AA64DFR0_BRPs_VAL(aa64dfr0);
    FGDB_RAW("GDB stub: breakpoints: %d, watchpoints: %d\r\n", gdb->dbg_breakpoint_num,
             gdb->dbg_watchpoint_num);

    /*
	* We have limited number of {watch,break}points, each consists of
	* two registers:
	* - wcr/bcr regsiter configurates corresponding {watch,break}point
	*   behaviour
	* - wvr/bvr register keeps address we are hunting for
	*
	* Reset all breakpoints and watchpoints.
	*/
    for (i = 0; i < gdb->dbg_watchpoint_num; i++)
    {
        dbg_wb_write_reg(DBG_REG_BASE_WCR, i, 0);
        dbg_wb_write_reg(DBG_REG_BASE_WVR, i, 0);
    }

    for (i = 0; i < gdb->dbg_breakpoint_num; i++)
    {
        dbg_wb_write_reg(DBG_REG_BASE_BCR, i, 0);
        dbg_wb_write_reg(DBG_REG_BASE_BVR, i, 0);
    }

    dbg_enable();

    /* TODO: Eventually will need to initialize debug registers here. */

    /*
	* Initialize the kernel debugger interface. kdb_init(void)
	*/

    /* be->dbbe_init(), gdb_init(void) */
    if (port == DBG_PORT_UART)
    {
        gdb->dbg_port_priv = port_instance;
        gdb->dbg_port.gdb_port_init = FGdbUartDbgPortInit;
        gdb->dbg_port.gdb_port_getc = FGdbUartDbgPortGetC;
        gdb->dbg_port.gdb_port_putc = FGdbUartDbgPortPutC;
        gdb->dbg_port.gdb_port_intr_ctrl = FGdbUartDbgPortIntrCtrl;
        gdb->dbg_port.gdb_port_term = FGdbUartDbgPortTerminate;

        /* gdb_cur->gdb_init(), uart_dbg_init(void) */
        if (gdb->dbg_port.gdb_port_init(port_instance))
        {
            ret = FGDB_FAILED;
        }

        gdb->dbbe_active = 1;
    }
    else
    {
        return FGDB_FAILED;
    }

    FASSERT_MSG(((sizeof(gdb->gdb_tx_u.txu_midbuf) == sizeof(gdb->gdb_tx_u.txu_fullbuf)) &&
                 (offsetof(struct _midbuf, mb_buf) == 1)),
                "assertions necessary for correctness");
    FASSERT_MSG(GDB_BUFSZ >= (GDB_NREGS * 16), "buffer fits 'g' regs");

    gdb->monitor.dbg_flags = DBGMON_KERNEL;

    FGdbRegisterInstance(gdb);
    return ret;
}

int FGdbTrap(FGdb *const gdb, int type, int code)
{
    FASSERT(gdb);
    FASSERT(gdb->dbbe_active == 1);
    int ret = 0;
    int req;

    gdb->dbg_port.gdb_port_intr_ctrl(gdb->dbg_port_priv, FALSE);

    gdb->gdb_listening = 0;
    gdb->gdb_ackmode = TRUE;

    /*
	 * Send a T packet. We currently do not support watchpoints (the
	 * awatch, rwatch or watch elements).
	 */
    gdb_tx_begin(gdb, 'T');
    gdb_tx_hex(gdb, gdb_cpu_signal(gdb, type, code), 2);
    gdb_tx_varhex(gdb, GDB_REG_PC);
    gdb_tx_char(gdb, ':');
    gdb_tx_reg(gdb, GDB_REG_PC);
    gdb_tx_char(gdb, ';');
    gdb_cpu_stop_reason(gdb, type, code);
    gdb_tx_str(gdb, "thread:");
    gdb_tx_varhex(gdb, (uintmax_t)FGdbCurrentThreadId());
    gdb_tx_char(gdb, ';');
    gdb_tx_end(gdb); /* XXX check error condition. */

    while (gdb_rx_begin(gdb) == 0)
    {
        FGDB_DEBUG("GDB: got '%s'", gdb->gdb_rxp);
        req = gdb_rx_char(gdb);
        switch (req)
        {
            case '?': /* Last signal. */
                {
                    /* Send a exception packet "T <value>", what cause the pause */
                    gdb_tx_begin(gdb, 'T');
                    gdb_tx_hex(gdb, gdb_cpu_signal(gdb, type, code), 2);
                    gdb_tx_end(gdb);
                }
                break;
            case 'c': /* Continue. */
                {
                    uintmax_t addr;
                    u64 pc;
                    if (!gdb_rx_varhex(gdb, &addr))
                    {
                        pc = addr;
                        gdb_cpu_setreg(gdb, GDB_REG_PC, &pc);
                    }

                    kdb_cpu_clear_singlestep(gdb);
                    gdb->gdb_listening = 1;
                    ret = (1);
                    goto exit;
                }
                break;
            case 'D': /* Detach */
                {
                    gdb_tx_ok(gdb);
                    gdb_handle_detach(gdb);
                    ret = (1);
                    goto exit;
                }
                break;
            case 'g': /* Read registers. */
                {
                    size_t r;
                    gdb_tx_begin(gdb, 0);
                    for (r = 0; r < GDB_NREGS; r++)
                    {
                        gdb_tx_reg(gdb, r);
                    }
                    gdb_tx_end(gdb);
                }
                break;
            case 'G': /* Write registers. */
                {
                    char *val;
                    boolean success;
                    size_t r;
                    for (success = TRUE, r = 0; r < GDB_NREGS; r++)
                    {
                        val = gdb->gdb_rxp;
                        if (!gdb_rx_mem(gdb, (unsigned char *)val, gdb_cpu_regsz(r)))
                        {
                            gdb_tx_err(gdb, EINVAL);
                            success = FALSE;
                            break;
                        }
                        gdb_cpu_setreg(gdb, r, val);
                    }
                    if (success)
                    {
                        gdb_tx_ok(gdb);
                    }
                }
                break;
            case 'H': /* Set thread. */
                {
                    FGdbSetThread(gdb); /* TODO: handle in RTOS */
                }
                break;
            case 'k': /* Kill request. */
                {
                    gdb_handle_detach(gdb);
                    ret = (1);
                    goto exit;
                }
                break;
            case 'm': /* Read memory. */
                {
                    uintmax_t addr, size;
                    if (gdb_rx_varhex(gdb, &addr) || gdb_rx_char(gdb) != ',' ||
                        gdb_rx_varhex(gdb, &size))
                    {
                        gdb_tx_err(gdb, EINVAL);
                        break;
                    }
                    gdb_tx_begin(gdb, 0);
                    if (gdb_tx_mem(gdb, (const unsigned char *)(uintptr_t)addr, size))
                    {
                        gdb_tx_end(gdb);
                    }
                    else
                    {
                        gdb_tx_err(gdb, EIO);
                    }
                }
                break;
            case 'M': /* Write memory. */
                {
                    uintmax_t addr, size;
                    if (gdb_rx_varhex(gdb, &addr) || gdb_rx_char(gdb) != ',' ||
                        gdb_rx_varhex(gdb, &size) || gdb_rx_char(gdb) != ':')
                    {
                        gdb_tx_err(gdb, EINVAL);
                        break;
                    }
                    if (gdb_rx_mem(gdb, (unsigned char *)(uintptr_t)addr, size) == 0)
                    {
                        gdb_tx_err(gdb, EIO);
                    }
                    else
                    {
                        gdb_tx_ok(gdb);
                    }
                }
                break;
            case 'p': /* Read register. */
                {
                    uintmax_t reg;
                    if (gdb_rx_varhex(gdb, &reg))
                    {
                        gdb_tx_err(gdb, EINVAL);
                        break;
                    }
                    gdb_tx_begin(gdb, 0);
                    gdb_tx_reg(gdb, reg);
                    gdb_tx_end(gdb);
                }
                break;
            case 'P': /* Write register. */
                {
                    char *val;
                    uintmax_t reg;
                    val = gdb->gdb_rxp;
                    if (gdb_rx_varhex(gdb, &reg) || gdb_rx_char(gdb) != '=' ||
                        !gdb_rx_mem(gdb, (unsigned char *)val, gdb_cpu_regsz(reg)))
                    {
                        gdb_tx_err(gdb, EINVAL);
                        break;
                    }
                    gdb_cpu_setreg(gdb, reg, val);
                    gdb_tx_ok(gdb);
                }
                break;
            case 'q': /* General query. */
                {
                    FGdbSystemQuery(gdb);
                }
                break;
            case 's': /* Step. */
                {
                    uintmax_t addr;
                    u64 pc;
                    if (!gdb_rx_varhex(gdb, &addr))
                    {
                        pc = addr;
                        gdb_cpu_setreg(gdb, GDB_REG_PC, &pc);
                    }
                    kdb_cpu_set_singlestep(gdb);
                    gdb->gdb_listening = 1;
                    ret = (1);
                    goto exit;
                }
                break;
            case 'z': /* Remove watchpoint. */
                {
                    gdb_z_remove(gdb);
                }
                break;
            case 'Z': /* Set watchpoint. */
                {
                    gdb_z_insert(gdb);
                }
                break;
            case EOF:
                /* Empty command. Treat as unknown command. */
                /* FALLTHROUGH */
            default:
                if (req != EOF)
                {
                    FGDB_WARN("unhandled '%c'", (char)req);
                }
                /* Unknown command. Send empty response. */
                gdb_tx_empty(gdb);
                break;
        }
    }

exit:
    gdb->dbg_port.gdb_port_intr_ctrl(gdb->dbg_port_priv, TRUE);
    return ret;
}