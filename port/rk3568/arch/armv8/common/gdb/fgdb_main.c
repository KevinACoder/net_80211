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
 * Modified from FreeBSD sys/gdb/gdb_main.c with LICENSE
 * 
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * FilePath: fgdb_main.c
 * Created Date: 2024-08-27 17:05:14
 * Last Modified: 2024-08-27 17:34:17
 * Description:  This file is for gdb packet function implmentation
 * 
 * Modify History:
 *   Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0   zhugengyu   2025/8/12    init
 */

/***************************** Include Files *********************************/
#include "fdebug.h"
#include "fassert.h"
#include "fkernel.h"
#include "fcache.h"
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

/*****************************************************************************/
void gdb_do_mem_search(FGdb *const gdb)
{
    size_t patlen;
    uintmax_t addr, size;
    const unsigned char *found;

    if (gdb_rx_varhex(gdb, &addr) || gdb_rx_char(gdb) != ';' ||
        gdb_rx_varhex(gdb, &size) || gdb_rx_char(gdb) != ';' ||
        gdb_rx_bindata(gdb, gdb->gdb_bindata, sizeof(gdb->gdb_bindata), &patlen))
    {
        gdb_tx_err(gdb, EINVAL);
        return;
    }
    if (gdb_search_mem(gdb, (const unsigned char *)(uintptr_t)addr, size,
                       gdb->gdb_bindata, patlen, &found))
    {
        if (found == 0ULL)
        {
            gdb_tx_begin(gdb, '0');
        }
        else
        {
            gdb_tx_begin(gdb, '1');
            gdb_tx_char(gdb, ',');
            gdb_tx_hex(gdb, (uintmax_t)(uintptr_t)found, 8);
        }
        gdb_tx_end(gdb);
    }
    else
    {
        gdb_tx_err(gdb, EIO);
    }
}

static const char *const gdb_feature_names[] = {
    [GDB_MULTIPROCESS] = "multiprocess",
    [GDB_SWBREAK] = "swbreak",
    [GDB_HWBREAK] = "hwbreak",
    /* not supported yet */
    [GDB_QRELOCINSN] = "qRelocInsn",
    [GDB_FORK_EVENTS] = "fork-events",
    [GDB_VFORK_EVENTS] = "vfork-events",
    [GDB_EXEC_EVENTS] = "exec-events",
    [GDB_VCONT_SUPPORTED] = "vContSupported",
    [GDB_QTHREADEVENTS] = "QThreadEvents",
    [GDB_NO_RESUMED] = "no-resumed",
};

char *strchrnul(const char *p, int ch)
{

    for (; *p != 0 && *p != ch; p++)
    {
        ;
    }
    return (__DECONST(char *, p));
}

void gdb_do_qsupported(FGdb *const gdb, uint32_t *feat)
{
    char *tok, *delim, ok;
    size_t i, toklen;

    /* Parse supported host features */
    *feat = 0;
    switch (gdb_rx_char(gdb))
    {
        case ':':
            break;
        case EOF:
            goto nofeatures;
        default:
            goto error;
    }

    while (gdb->gdb_rxsz > 0)
    {
        tok = gdb->gdb_rxp;
        delim = strchrnul(gdb->gdb_rxp, ';');
        toklen = (delim - tok);

        gdb->gdb_rxp += toklen;
        gdb->gdb_rxsz -= toklen;
        if (*delim != '\0')
        {
            *delim = '\0';
            gdb->gdb_rxp += 1;
            gdb->gdb_rxsz -= 1;
        }

        if (toklen < 2)
        {
            goto error;
        }

        ok = tok[toklen - 1];
        if (ok != '-' && ok != '+')
        {
            /*
			 * GDB only has one KV-pair feature, and we don't
			 * support it, so ignore and move on.
			 */
            if (strchr(tok, '=') != NULL)
            {
                continue;
            }
            /* Not a KV-pair, and not a +/- flag?  Malformed. */
            goto error;
        }
        if (ok != '+')
        {
            continue;
        }
        tok[toklen - 1] = '\0';

        for (i = 0; i < ARRAY_SIZE(gdb_feature_names); i++)
        {
            if (strcmp(gdb_feature_names[i], tok) == 0)
            {
                FGDB_DEBUG("%s supported", tok);
                break;
            }
        }

        if (i == ARRAY_SIZE(gdb_feature_names))
        {
            /* Unknown GDB feature. */
            continue;
        }

        *feat |= BIT(i);
    }

nofeatures:
    /* Send a supported feature list back */
    gdb_tx_begin(gdb, 0);

    gdb_tx_str(gdb, "PacketSize");
    gdb_tx_char(gdb, '=');
    /*
	 * We don't buffer framing bytes, but we do need to retain a byte for a
	 * trailing nul.
	 */
    gdb_tx_varhex(gdb, GDB_BUFSZ + strlen("$#nn") - 1);

    gdb_tx_str(gdb, ";qXfer:threads:read+");

    /*
	 * If the debugport is a reliable transport, request No Ack mode from
	 * the server.  The server may or may not choose to enter No Ack mode.
	 * https://sourceware.org/gdb/onlinedocs/gdb/Packet-Acknowledgment.html
	 */
    if (gdb->gdb_dbfeatures & GDB_DBGP_FEAT_RELIABLE)
    {
        gdb_tx_str(gdb, ";QStartNoAckMode+");
    }

    /*
	 * Future consideration:
	 *   - vCont
	 *   - multiprocess
	 */
    gdb_tx_end(gdb);
    return;

error:
    *feat = 0;
    gdb_tx_err(gdb, EINVAL);
}


void gdb_handle_detach(FGdb *const gdb)
{
    kdb_cpu_clear_singlestep(gdb);
    gdb->gdb_listening = 0;

    if (gdb->gdb_dbfeatures & GDB_DBGP_FEAT_WANTTERM)
    {
        gdb->dbg_port.gdb_port_term(gdb->dbg_port_priv);
    }
}

/*
 * Handle a 'Z' packet: set a breakpoint or watchpoint.
 *
 * Currently, only watchpoints are supported.
 */
void gdb_z_insert(FGdb *const gdb)
{
    uintmax_t addr, length;
    char ztype;
    int error;

    ztype = gdb_rx_char(gdb);
    if (gdb_rx_char(gdb) != ',' || gdb_rx_varhex(gdb, &addr) ||
        gdb_rx_char(gdb) != ',' || gdb_rx_varhex(gdb, &length))
    {
        error = EINVAL;
        goto fail;
    }

    FGDB_DEBUG("insert ztype = %c", ztype);
    switch (ztype)
    {
        case '2': /* write watchpoint */
            error = kdb_cpu_set_watchpoint(gdb, (uintptr_t)addr, (size_t)length, KDB_DBG_ACCESS_W);
            break;
        case '3': /* read watchpoint */
            error = kdb_cpu_set_watchpoint(gdb, (uintptr_t)addr, (size_t)length, KDB_DBG_ACCESS_R);
            break;
        case '4': /* access (RW) watchpoint */
            error = kdb_cpu_set_watchpoint(gdb, (uintptr_t)addr, (size_t)length, KDB_DBG_ACCESS_RW);
            break;
        case '0': /* software breakpoint (just use as hardware breakpoint) */
                  /* Not implemented. */
                  /* gdb_tx_empty(gdb);
		return; */
        case '1': /* hardware breakpoint */
            error = kdb_cpu_set_hwbreakpoint(gdb, (uintptr_t)addr, (size_t)length);
            break;
        default:
            error = EINVAL;
            break;
    }
    if (error != 0)
    {
        goto fail;
    }
    gdb_tx_ok(gdb);
    return;
fail:
    gdb_tx_err(gdb, error);
    return;
}

/*
 * Handle a 'z' packet; clear a breakpoint or watchpoint.
 *
 * Currently, only watchpoints are supported.
 */
void gdb_z_remove(FGdb *const gdb)
{
    uintmax_t addr, length;
    char ztype;
    int error;

    ztype = gdb_rx_char(gdb);
    if (gdb_rx_char(gdb) != ',' || gdb_rx_varhex(gdb, &addr) ||
        gdb_rx_char(gdb) != ',' || gdb_rx_varhex(gdb, &length))
    {
        error = EINVAL;
        goto fail;
    }

    FGDB_DEBUG("remove ztype = %c", ztype);
    switch (ztype)
    {
        case '2': /* write watchpoint */
        case '3': /* read watchpoint */
        case '4': /* access (RW) watchpoint */
            error = kdb_cpu_clr_watchpoint(gdb, (uintptr_t)addr, (size_t)length);
            break;
        case '0': /* software breakpoint (just use as hardware breakpoint) */
                  /* Not implemented. */
                  /* gdb_tx_empty(gdb);
		return; */
        case '1': /* hardware breakpoint */
            error = kdb_cpu_clr_hwbreakpoint(gdb, (uintptr_t)addr, (size_t)length);
            break;
        default:
            error = EINVAL;
            break;
    }
    if (error != 0)
    {
        goto fail;
    }
    gdb_tx_ok(gdb);
    return;
fail:
    gdb_tx_err(gdb, error);
    return;
}