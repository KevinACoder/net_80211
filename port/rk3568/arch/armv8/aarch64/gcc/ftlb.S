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
 * FilePath: ftlb.S
 * Date: 2022-02-10 14:53:41
 * LastEditTime: 2022-02-17 17:31:30
 * Description:  This file is for tlb invalidation
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe     2021/7/3     first release
 */

#include "flinkage.h"
#include "fmacro.h"

/*
 * void AsmInvalidateTlbAll(void)
 *
 * invalidate all tlb entries.
*/
.pushsection .text.AsmInvalidateTlbAll, "ax"
ENTRY(AsmInvalidateTlbAll)
	switch_el x9, 3f, 2f, 1f
3:	tlbi	alle3
	dsb	sy
	isb
	b	0f
2:	tlbi	alle2
	dsb	sy
	isb
	b	0f
1:	tlbi	vmalle1
	dsb	sy
	isb
0:
	ret
ENDPROC(AsmInvalidateTlbAll)
.popsection
