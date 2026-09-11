/*
 * Copyright (C) 2021, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: felf.h
 * Date: 2021-08-31 11:16:49
 * LastEditTime: 2022-02-17 18:05:22
 * Description:  This file is for showing elf api.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0  zhugengyu  2022/10/27   rename file name
 */

#ifndef FELF_H
#define FELF_H

#include "ftypes.h"
#include "ferror_code.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define FELF_SUCCESS FT_SUCCESS /* SUCCESS */
#define FELF_SECTION_NO_STRTAB \
    FT_MAKE_ERRCODE(ErrorModGeneral, ErrElf, 1) /* There is no string table */
#define FELF_SECTION_NO_SPACE \
    FT_MAKE_ERRCODE(ErrorModGeneral, ErrElf, 2) /* There is no space section */
#define FELF_SECTION_NOT_FIT \
    FT_MAKE_ERRCODE(ErrorModGeneral, ErrElf, 3) /* No corresponding section was matched */
#define FELF_SECTION_GET_ERROR FT_MAKE_ERRCODE(ErrorModGeneral, ErrElf, 3)

unsigned long ElfLoadElfImagePhdr(unsigned long addr);
unsigned long ElfLoadElfImageShdr(unsigned long addr);
int ElfIsImageValid(unsigned long addr);
unsigned long ElfExecBootElf(unsigned long (*entry)(int, char *const[]), int argc,
                             char *const argv[]);

FError ElfGetSection(unsigned long addr, char *section_name, u8 *data_get, u32 *length_p);

#ifdef __cplusplus
}
#endif

#endif // !