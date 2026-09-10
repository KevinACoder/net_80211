/*
 * Copyright (C) 2024, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fend_print.c
 * Created Date: 2024-02-19 10:39:53
 * Last Modified: 2024-02-19 10:40:30
 * Description:  This file is for print end flag.
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0  liqiaozhong  2024/2/19  add end print message
 */

#include "sdkconfig.h"
#include "fprintk.h"

#ifdef CONFIG_USE_END_PRINT
void FEndPrint(void)
{
    f_printk("[system_shutdown]");
    while (1)
    {
        ;
    }
}
#endif