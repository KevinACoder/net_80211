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
 * Modified from FreeBSD sys/gdb/gdb.h with LICENSE
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * FilePath: fgdb_def.h
 * Created Date: 2024-08-27 17:05:14
 * Last Modified: 2024-08-27 17:34:17
 * Description:  This file is for gdb stub function definition
 * 
 * Modify History:
 *   Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0   zhugengyu   2025/8/12    init
 */

#ifndef FGDB_DEF_H
#define FGDB_DEF_H

/***************************** Include Files *********************************/
#include "ftypes.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

/************************** Constant Definitions *****************************/
#define __STRING(x)                  #x
#define __XSTRING(x)                 __STRING(x) /* expand x, then stringify */

#define GDB_EXCEPTION_BREAKPOINT     5

#define GDB_STOPREASON_NONE          0x00
#define GDB_STOPREASON_WATCHPOINT_RO 0x01
#define GDB_STOPREASON_WATCHPOINT_WO 0x02
#define GDB_STOPREASON_WATCHPOINT_RW 0x03
#define GDB_STOPREASON_BREAKPOINT    0x04
#define GDB_STOPREASON_STEPPOINT     0x05
#define GDB_STOPREASON_CTRLC         0x06

/* Debug breakpoint/watchpoint access types */
#define KDB_DBG_ACCESS_EXEC          0
#define KDB_DBG_ACCESS_R             1
#define KDB_DBG_ACCESS_W             2
#define KDB_DBG_ACCESS_RW            3

enum
{
    GDB_MULTIPROCESS,
    GDB_SWBREAK,
    GDB_HWBREAK,
    GDB_QRELOCINSN,
    GDB_FORK_EVENTS,
    GDB_VFORK_EVENTS,
    GDB_EXEC_EVENTS,
    GDB_VCONT_SUPPORTED,
    GDB_QTHREADEVENTS,
    GDB_NO_RESUMED,
}; /* GDB feature supported */

#define GDB_DBGP_FEAT_WANTTERM \
    0x1 /* Want gdb_term() invocation when
					   leaving GDB.  gdb_term has been
					   deadcode and never invoked for so
					   long I don't want to just blindly
					   start invoking it without opt-in. */
#define GDB_DBGP_FEAT_RELIABLE \
    0x2 /* The debugport promises it is a
					   reliable transport, which allows GDB
					   acks to be turned off. */
/************************** Function Prototypes ******************************/
int gdb_rx_begin(FGdb *const);
int gdb_rx_equal(FGdb *const, const char *);
int gdb_rx_mem(FGdb *const, unsigned char *, size_t);
int gdb_rx_varhex(FGdb *const, uintmax_t *);

static __inline int gdb_rx_char(FGdb *const gdb)
{
    int c;

    if (gdb->gdb_rxsz > 0)
    {
        c = *gdb->gdb_rxp++;
        gdb->gdb_rxsz--;
    }
    else
    {
        c = EOF;
    }
    return (c);
}

void gdb_tx_begin(FGdb *const, char);
int gdb_tx_end(FGdb *const);
int gdb_tx_mem(FGdb *const, const unsigned char *, size_t);
void gdb_tx_reg(FGdb *const, int);
boolean gdb_txbuf_has_capacity(FGdb *const, size_t);
int gdb_rx_bindata(FGdb *const, unsigned char *data, size_t datalen, size_t *amt);
int gdb_search_mem(FGdb *const, const unsigned char *addr, size_t size,
                   const unsigned char *pat, size_t patlen, const unsigned char **found);

static inline void gdb_tx_char(FGdb *const gdb, char c)
{
    *gdb->gdb_txp++ = c;
}

static inline int gdb_tx_empty(FGdb *const gdb)
{
    gdb_tx_begin(gdb, '\0');
    return (gdb_tx_end(gdb));
}

#if defined(CONFIG_USE_NEWLIB)
static void uintmax_to_hex(uintmax_t n, char *buf, int sz)
{
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = sz - 1; i >= 0; i--)
    {
        buf[i] = hex_chars[n & 0xF];
        n >>= 4;
    }
    buf[sz] = '\0';
}
#endif

static inline void gdb_tx_hex(FGdb *const gdb, uintmax_t n, int sz)
{
#if defined(CONFIG_USE_NEWLIB)
    char hex_buf[64];
    uintmax_to_hex(n, hex_buf, sz);
    gdb->gdb_txp += sprintf(gdb->gdb_txp, "%s", hex_buf);
#else
    gdb->gdb_txp += sprintf(gdb->gdb_txp, "%0*jx", sz, n);
#endif
}

static inline int gdb_tx_err(FGdb *const gdb, int err)
{
    gdb_tx_begin(gdb, 'E');
    gdb_tx_hex(gdb, err, 2);
    return (gdb_tx_end(gdb));
}

static inline int gdb_tx_ok(FGdb *const gdb)
{
    gdb_tx_begin(gdb, 'O');
    gdb_tx_char(gdb, 'K');
    return (gdb_tx_end(gdb));
}

static inline void gdb_tx_str(FGdb *const gdb, const char *s)
{
    while (*s)
    {
        *gdb->gdb_txp++ = *s++;
    }
}

static inline void gdb_tx_varhex(FGdb *const gdb, uintmax_t n)
{
#if defined(CONFIG_USE_NEWLIB)
    char hex_buf[64];
    int sz = sizeof(uintmax_t) * 2;
    uintmax_to_hex(n, hex_buf, sz);
    char *start = hex_buf;
    while (*start == '0' && *(start + 1) != '\0')
    {
        start++;
    }
    gdb->gdb_txp += sprintf(gdb->gdb_txp, "%s", start);
#else
    gdb->gdb_txp += sprintf(gdb->gdb_txp, "%jx", n);
#endif
}

static inline void gdb_nack(FGdb *const gdb)
{
    if (gdb->gdb_ackmode)
    {
        gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, '-');
    }
}

static inline void gdb_ack(FGdb *const gdb)
{
    if (gdb->gdb_ackmode)
    {
        gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, '+');
    }
}


#ifdef __cplusplus
}
#endif

#endif /* FGDB_H */