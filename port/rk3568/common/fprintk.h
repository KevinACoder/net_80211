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
 * FilePath: fprintk.h
 * Date: 2021-08-23 16:24:02
 * LastEditTime: 2022-02-17 18:01:35
 * Description:  This file is for creating custom print interface for standlone sdk.
 *
 * Modify History:
 *  Ver     Who           Date                  Changes
 * -----   ------       --------     --------------------------------------
 *  1.0    huanghe      2022/7/23            first release
 */

#ifndef FPRINTK_H
#define FPRINTK_H

#ifdef __cplusplus
extern "C"
{
#endif

void f_printk(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
