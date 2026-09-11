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
 * FilePath: fswap.h
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:35:24
 * Description:  This files is for endian conversion.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0  zhugengyu  2021/06/15        first release
 */

#ifndef FSWAP_H
#define FSWAP_H

#include "ftypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define __swab16(x)    (uint16_t) __builtin_bswap16((uint16_t)(x))
#define __swab32(x)    (uint32_t) __builtin_bswap32((uint32_t)(x))
#define __swab64(x)    (uint64_t) __builtin_bswap64((uint64_t)(x))

#define cpu_to_le64(x) ((uint64_t)(x))
#define le64_to_cpu(x) ((uint64_t)(x))
#define cpu_to_le32(x) ((uint32_t)(x))
#define le32_to_cpu(x) ((uint32_t)(x))
#define cpu_to_le16(x) ((uint16_t)(x))
#define le16_to_cpu(x) ((uint16_t)(x))
#define cpu_to_be64(x) __swab64((x))
#define be64_to_cpu(x) __swab64((x))
#define cpu_to_be32(x) __swab32((x))
#define be32_to_cpu(x) __swab32((x))
#define cpu_to_be16(x) __swab16((x))
#define be16_to_cpu(x) __swab16((x))

#define ___htonl(x)    cpu_to_be32(x)
#define ___htons(x)    cpu_to_be16(x)
#define ___ntohl(x)    be32_to_cpu(x)
#define ___ntohs(x)    be16_to_cpu(x)

#ifndef htonl
#define htonl(x)       ___htonl(x)
#endif

#ifndef ntohl
#define ntohl(x)       ___ntohl(x)
#endif

#ifndef htons
#define htons(x)       ___htons(x)
#endif

#ifndef ntohs
#define ntohs(x)       ___ntohs(x)
#endif

#ifdef __cplusplus
}
#endif

#endif
