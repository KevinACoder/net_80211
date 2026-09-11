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
 * FilePath: fassert.h
 * Date: 2021-04-07 09:53:07
 * LastEditTime: 2022-02-17 18:04:35
 * Description:  This file is for assertion defintion.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe    2021/4/5       init commit
 * 1.1   zhugengyu  2022/3/7       re-define assert macro
 */

#ifndef FASSERT_H
#define FASSERT_H

/***************************** Include Files *********************************/

#include "ferror_code.h"
#include "ftypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef FASSERT_PRINT
#include <stdio.h>
#define FASSERT_PRINT printf
#endif

/**************************** Type Definitions *******************************/
typedef enum
{
    FASSERT_NONE = 0,
    FASSERT_OCCURRED
} FAssertStatus; /* 断言状态 */

/* 断言处理回调函数 */
typedef void (*FAssertCB)(const char *file, s32 line, int ret);

/************************** Variable Definitions *****************************/

/***************** Macros (Inline Functions) Definitions *********************/
#define FASSERT_MSG(expression, fmt, ...)       \
    {                                           \
        if (expression)                         \
        {                                       \
            FAssertSetStatus(FASSERT_NONE);     \
        }                                       \
        else                                    \
        {                                       \
            FAssertSetStatus(FASSERT_OCCURRED); \
            FASSERT_PRINT(fmt, ##__VA_ARGS__);  \
            FAssert(__FILE__, __LINE__, 0xff);  \
        }                                       \
    }

#define FASSERT(expression)                     \
    {                                           \
        if (expression)                         \
        {                                       \
            FAssertSetStatus(FASSERT_NONE);     \
        }                                       \
        else                                    \
        {                                       \
            FAssertSetStatus(FASSERT_OCCURRED); \
            FAssert(__FILE__, __LINE__, 0xff);  \
        }                                       \
    }

/* 检查静态断言状态 */
#define FASSERT_STATIC(expression) extern int assert_static[(expression) ? 1 : -1]

/************************** Function Prototypes ******************************/
/* 设置断言状态 */
void FAssertSetStatus(FAssertStatus status);

/* 获取当前断言状态 */
FAssertStatus FAssertGetStatus(void);

/* 设置断言回调函数 */
void FAssertSetCB(FAssertCB cb);

/* 断言实现 */
void FAssert(const char *file, s32 line, int code);

#ifdef __cplusplus
}
#endif

#endif // !
