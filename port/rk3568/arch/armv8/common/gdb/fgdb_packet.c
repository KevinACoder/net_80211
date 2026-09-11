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
 * Modified from FreeBSD sys/gdb/gdb_packet.c with LICENSE
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
 * FilePath: fgdb_packet.c
 * Created Date: 2024-08-27 17:05:14
 * Last Modified: 2024-08-27 17:34:17
 * Description:  This file is for gdb packet function implmentation
 * 
 * Modify History:
 *   Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0   zhugengyu   2025/8/12    init commit
 */

/***************************** Include Files *********************************/
#include "fdebug.h"
#include "fassert.h"
#include "fcache.h"
#include "fgdb.h"
#include "fgdb_def.h"
#include "fgdb_arch.h"

/************************** Constant Definitions *****************************/
#define ENOSPC                  28 /* No space left on device */
/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
#define FGDB_DEBUG_TAG          "FGDB"
#define FGDB_ERROR(format, ...) FT_DEBUG_PRINT_E(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_WARN(format, ...)  FT_DEBUG_PRINT_W(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_INFO(format, ...)  FT_DEBUG_PRINT_I(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_DEBUG(format, ...) FT_DEBUG_PRINT_D(FGDB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FGDB_RAW(format, ...)   FT_RAW_PRINTF(format, ##__VA_ARGS__)

#define gdb_txbuf(gdb)          (gdb)->gdb_tx_u.txu_midbuf.mb_buf
#define gdb_tx_fullbuf(gdb)     (gdb)->gdb_tx_u.txu_fullbuf

#define C2N(c)                  (((c) < 'A') ? (c) - '0' : 10 + (((c) < 'a') ? (c) - 'A' : (c) - 'a'))
#define N2C(n)                  (((n) < 10) ? (n) + '0' : (n) + 'a' - 10)

/*
 * XXX: A lot of code uses lowercase characters, but control-character
 * conversion is actually only valid when applied to uppercase
 * characters. We just treat lowercase characters as if they were
 * inserted as uppercase.
 */
#define CTRL(x)                 ((x) >= 'a' && (x) <= 'z' ? ((x) - 'a' + 1) : (((x) - 'A' + 1) & 0x7f))
/************************** Function Prototypes ******************************/

/*****************************************************************************/
/*
 * Get a single character
 */

static int gdb_getc(FGdb *const gdb)
{
    int c;

    do
    {
        c = gdb->dbg_port.gdb_port_getc(gdb->dbg_port_priv);
    } while (c == -1);

    return (c);
}

/*
 * Functions to receive and extract from a packet.
 */

int gdb_rx_begin(FGdb *const gdb)
{
    int c, cksum;

    gdb->gdb_rxp = NULL;
    do
    {
        /*
		 * Wait for the start character, ignore all others.
		 * XXX needs a timeout.
		 */
        while ((c = gdb_getc(gdb)) != '$')
        {
            ;
        }

        /* Read until a # or end of buffer is found. */
        cksum = 0;
        gdb->gdb_rxsz = 0;
        while (gdb->gdb_rxsz < sizeof(gdb->gdb_rxbuf) - 1)
        {
            c = gdb_getc(gdb);
            if (c == '#')
            {
                break;
            }
            gdb->gdb_rxbuf[gdb->gdb_rxsz++] = c;
            cksum += c;
        }
        gdb->gdb_rxbuf[gdb->gdb_rxsz] = 0;
        cksum &= 0xff;

        /* Bail out on a buffer overflow. */
        if (c != '#')
        {
            gdb_nack(gdb);
            return (ENOSPC);
        }

        /*
		 * In Not-AckMode, we can assume reliable transport and neither
		 * need to verify checksums nor send Ack/Nack.
		 */
        if (!gdb->gdb_ackmode)
        {
            break;
        }

        c = gdb_getc(gdb);
        cksum -= (C2N(c) << 4) & 0xf0;
        c = gdb_getc(gdb);
        cksum -= C2N(c) & 0x0f;
        if (cksum == 0)
        {
            gdb_ack(gdb);
        }
        else
        {
            gdb_nack(gdb);
            FGDB_RAW("GDB: packet `%s' has invalid checksum\n", gdb->gdb_rxbuf);
        }
    } while (cksum != 0);

    gdb->gdb_rxp = gdb->gdb_rxbuf;
    return (0);
}

int gdb_rx_equal(FGdb *const gdb, const char *str)
{
    int len;

    len = strlen(str);
    if (len > gdb->gdb_rxsz || strncmp(str, gdb->gdb_rxp, len) != 0)
    {
        return (0);
    }
    gdb->gdb_rxp += len;
    gdb->gdb_rxsz -= len;
    return (1);
}

int gdb_rx_mem(FGdb *const gdb, unsigned char *addr, size_t size)
{
    unsigned char *p;
    void *wctx;
    size_t cnt;
    unsigned char c;

    if (size * 2 != gdb->gdb_rxsz)
    {
        return (-1);
    }

    wctx = gdb_begin_write();
    p = addr;
    cnt = size;
    while (cnt-- > 0)
    {
        c = (C2N(gdb->gdb_rxp[0]) << 4) & 0xf0;
        c |= C2N(gdb->gdb_rxp[1]) & 0x0f;
        *p++ = c;
        gdb->gdb_rxsz -= 2;
        gdb->gdb_rxp += 2;
    }
    /* kdb_cpu_sync_icache(addr, size); */
    FCacheICacheInvalidate();
    FCacheDCacheInvalidate();
    gdb_end_write(wctx);
    return 1;
}

static inline int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

static inline int isxdigit(int c)
{
    return (isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'));
}

int gdb_rx_varhex(FGdb *const gdb, uintmax_t *vp)
{
    uintmax_t v;
    int c, neg;

    c = gdb_rx_char(gdb);
    neg = (c == '-') ? 1 : 0;
    if (neg == 1)
    {
        c = gdb_rx_char(gdb);
    }
    if (!isxdigit(c))
    {
        gdb->gdb_rxp -= ((c == -1) ? 0 : 1) + neg;
        gdb->gdb_rxsz += ((c == -1) ? 0 : 1) + neg;
        return (-1);
    }
    v = 0;
    do
    {
        v <<= 4;
        v += C2N(c);
        c = gdb_rx_char(gdb);
    } while (isxdigit(c));
    if (c != EOF)
    {
        gdb->gdb_rxp--;
        gdb->gdb_rxsz++;
    }
    *vp = (neg) ? -v : v;
    return (0);
}

/*
 * Function to build and send a package.
 */

void gdb_tx_begin(FGdb *const gdb, char tp)
{

    gdb->gdb_txp = gdb_txbuf(gdb);
    if (tp != '\0')
    {
        gdb_tx_char(gdb, tp);
    }
}

/*
 * Take raw packet buffer and perform typical GDB packet framing, but not run-
 * length encoding, before forwarding to driver ::gdb_sendpacket() routine.
 */
static void gdb_tx_sendpacket(FGdb *const gdb)
{
    size_t msglen, i;
    unsigned char csum;

    msglen = gdb->gdb_txp - gdb_txbuf(gdb);

    /* Add GDB packet framing */
    gdb_tx_fullbuf(gdb)[0] = '$';

    csum = 0;
    for (i = 0; i < msglen; i++)
    {
        csum += (unsigned char)gdb_txbuf(gdb)[i];
    }
    snprintf(&gdb_tx_fullbuf(gdb)[1 + msglen], 4, "#%02x", (unsigned)csum);

    gdb->dbg_port.gdb_sendpacket(gdb_tx_fullbuf(gdb), msglen + 4);
}

int gdb_tx_end(FGdb *const gdb)
{
    const char *p;
    int runlen;
    unsigned char c, cksum;

    FGDB_DEBUG("GDB: put '%s'", gdb_txbuf(gdb));
    do
    {
        if (gdb->dbg_port.gdb_sendpacket != NULL)
        {
            gdb_tx_sendpacket(gdb);
            goto getack;
        }

        gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, '$');

        cksum = 0;
        p = gdb_txbuf(gdb);
        while (p < gdb->gdb_txp)
        {
            /* Send a character and start run-length encoding. */
            c = *p++;
            gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, c);
            cksum += c;
            runlen = 0;
            /* Determine run-length and update checksum. */
            while (p < gdb->gdb_txp && *p == c)
            {
                runlen++;
                p++;
            }
            /* Emit the run-length encoded string. */
            while (runlen >= 97)
            {
                gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, '*');
                cksum += '*';
                gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, 97 + 29);
                cksum += 97 + 29;
                runlen -= 97;
                if (runlen > 0)
                {
                    gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, c);
                    cksum += c;
                    runlen--;
                }
            }
            /* Don't emit '$', '#', '+', '-' or a run length below 3. */
            while (runlen == 1 || runlen == 2 || runlen + 29 == '$' ||
                   runlen + 29 == '#' || runlen + 29 == '+' || runlen + 29 == '-')
            {
                gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, c);
                cksum += c;
                runlen--;
            }
            if (runlen == 0)
            {
                continue;
            }
            gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, '*');
            cksum += '*';
            gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, runlen + 29);
            cksum += runlen + 29;
        }

        gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, '#');
        c = cksum >> 4;
        gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, N2C(c));
        c = cksum & 0x0f;
        gdb->dbg_port.gdb_port_putc(gdb->dbg_port_priv, N2C(c));

    getack:
        /*
		 * In NoAckMode, it is assumed that the underlying transport is
		 * reliable and thus neither conservant sends acknowledgements;
		 * there is nothing to wait for here.
		 */
        if (!gdb->gdb_ackmode)
        {
            break;
        }

        c = gdb_getc(gdb);
    } while (c != '+');

    return (0);
}

int gdb_tx_mem(FGdb *const gdb, const unsigned char *addr, size_t size)
{
    while (size-- > 0)
    {
        *gdb->gdb_txp++ = N2C(*addr >> 4);
        *gdb->gdb_txp++ = N2C(*addr & 0x0f);
        addr++;
    }
    return 1;
}

void gdb_tx_reg(FGdb *const gdb, int regnum)
{
    unsigned char *regp;
    size_t regsz;

    regp = gdb_cpu_getreg(gdb, regnum, &regsz);
    if (regp == NULL)
    {
        /* Register unavailable. */
        while (regsz--)
        {
            gdb_tx_char(gdb, 'x');
            gdb_tx_char(gdb, 'x');
        }
    }
    else
    {
        gdb_tx_mem(gdb, regp, regsz);
    }
}

boolean gdb_txbuf_has_capacity(FGdb *const gdb, size_t req)
{
    return (((char *)gdb_txbuf(gdb) + sizeof(gdb_txbuf(gdb)) - gdb->gdb_txp) >= req);
}

/* Read binary data up until the end of the packet or until we have datalen decoded bytes */
int gdb_rx_bindata(FGdb *const gdb, unsigned char *data, size_t datalen, size_t *amt)
{
    int c;

    *amt = 0;

    while (*amt < datalen)
    {
        c = gdb_rx_char(gdb);
        if (c == EOF)
        {
            break;
        }
        /* Escaped character up next */
        if (c == '}')
        {
            /* Malformed packet. */
            if ((c = gdb_rx_char(gdb)) == EOF)
            {
                return (1);
            }
            c ^= 0x20;
        }
        *(data++) = c & 0xff;
        (*amt)++;
    }

    return (0);
}


void *memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
    char *cur, *last;
    const char *cl = (const char *)l;
    const char *cs = (const char *)s;

    /* we need something to compare */
    if (l_len == 0 || s_len == 0)
    {
        return NULL;
    }

    /* "s" must be smaller or equal to "l" */
    if (l_len < s_len)
    {
        return NULL;
    }

    /* special case where s_len == 1 */
    if (s_len == 1)
    {
        return memchr(l, (int)*cs, l_len);
    }

    /* the last position where its possible to find "s" in "l" */
    last = __DECONST(char *, cl) + l_len - s_len;

    for (cur = __DECONST(char *, cl); cur <= last; cur++)
    {
        if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
        {
            return cur;
        }
    }

    return NULL;
}

int gdb_search_mem(FGdb *const gdb, const unsigned char *addr, size_t size,
                   const unsigned char *pat, size_t patlen, const unsigned char **found)
{
    *found = memmem(addr, size, pat, patlen);
    return 1;
}
