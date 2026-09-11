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
 * FilePath: fmacro.h
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:35:17
 * Description:  This files is for the current execution state selects the macro definition
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 *  1.0  huanghe	2021/11/06		first release
 */


#ifndef FMACRO_H
#define FMACRO_H

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Branch according to exception level
 */
/*
 * Branch according to exception level
 */
.macro  switch_el, xreg, el3_label, el2_label, el1_label
mrs \xreg, CurrentEL
cmp \xreg, 0xc
b.eq    \el3_label
cmp \xreg, 0x8
b.eq    \el2_label
cmp \xreg, 0x4
b.eq    \el1_label
.endm

#ifdef __cplusplus
}
#endif

#endif /* __ASM_ARM_MACRO_H__ */
