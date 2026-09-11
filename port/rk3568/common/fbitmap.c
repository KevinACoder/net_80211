/*
 * Copyright (C) 2023, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: fbitmap.c
 * Created Date: 2023-10-31 18:09:12
 * Last Modified: 2023-11-15 09:47:07
 * Description:  This file is for bitmap
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0      huanghe     2023/11/10      first release
 */
#include <stdio.h>
#include <string.h>
#include "fbitmap.h"
#include "fcompiler.h"


#define FBIT_MASK                      ((sizeof(FBitPerWordType) * 8) - 1)

#define FBIT_WORD_MASK                 ~0UL
#define FBITMAP_BITS_PER_WORD          (sizeof(FBitPerWordType) * 8)
#define FBITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % FBITMAP_BITS_PER_WORD))
#define FBITMAP_LAST_WORD_MASK(nbits) \
    (((nbits) % FBITMAP_BITS_PER_WORD) ? (1UL << ((nbits) % FBITMAP_BITS_PER_WORD)) - 1 : ~0UL)

#define FBITMAP_WORD(x)      ((x) / FBITMAP_BITS_PER_WORD)

#define FBITMAP_NUM_WORDS(x) (((x) + FBITMAP_BITS_PER_WORD - 1) / FBITMAP_BITS_PER_WORD)

#define DIV_ROUND_UP(n, d)   (((n) + (d)-1) / (d))


/* find first zero bit starting from LSB */
static INLINE u16 Ffz(FBitPerWordType x)
{
    return __builtin_ffs(~x) - 1;
}

/**
 * @name: FBitMapSet
 * @msg: Sets a bit at the specified position in a bitmap.
 * @param {FBitPerWordType *} bitmap - Pointer to the bitmap.
 * @param {u16} pos - The bit position to set.
 */
void FBitMapSet(FBitPerWordType *bitmap, u16 pos)
{
    if (bitmap == NULL)
    {
        return;
    }

    *bitmap |= 1U << (pos & FBIT_MASK);
}

/**
 * @name: FBitMapClear
 * @msg: Clears a bit at the specified position in a bitmap.
 * @param {FBitPerWordType*} bitmap - Pointer to the bitmap.
 * @param {u16} pos - The bit position to clear.
 */
void FBitMapClear(FBitPerWordType *bitmap, u16 pos)
{
    if (bitmap == NULL)
    {
        return;
    }

    *bitmap &= ~(1U << (pos & FBIT_MASK));
}

/**
 * @name: FBitMapHighGet
 * @msg: Gets the position of the highest set bit in a bitmap.
 * @param {FBitPerWordType} bitmap - The bitmap to check.
 * @return {u16} The position of the highest set bit or FBIT_INVALID_BIT_INDEX if none.
 */
u16 FBitMapHighGet(FBitPerWordType bitmap)
{
    if (bitmap == 0)
    {
        return FBIT_INVALID_BIT_INDEX;
    }

    return (FBIT_MASK - CLZL(bitmap));
}

/**
 * @name: FBitMapLowGet
 * @msg: Gets the position of the lowest set bit in a bitmap.
 * @param {FBitPerWordType} bitmap - The bitmap to check.
 * @return {u16} The position of the lowest set bit or FBIT_INVALID_BIT_INDEX if none.
 */
u16 FBitMapLowGet(FBitPerWordType bitmap)
{
    if (bitmap == 0)
    {
        return FBIT_INVALID_BIT_INDEX;
    }

    return CTZL(bitmap);
}

/**
 * @name: FBitMapSetNBits
 * @msg: Sets a number of consecutive bits starting from a given position in a bitmap.
 * @param {FBitPerWordType*} bitmap - Pointer to the bitmap.
 * @param {u32} start - The starting bit position.
 * @param {u32} nums_set - The number of bits to set.
 */
void FBitMapSetNBits(FBitPerWordType *bitmap, u32 start, u32 nums_set)
{
    FBitPerWordType *p = bitmap + FBITMAP_WORD(start); /* 向下取整 */
    const u32 size = start + nums_set;
    u16 bits_toset = FBITMAP_BITS_PER_WORD - (start % FBITMAP_BITS_PER_WORD);
    FBitPerWordType mask_toset = FBITMAP_FIRST_WORD_MASK(start);

    while (nums_set > bits_toset)
    {
        *p |= mask_toset;
        nums_set -= bits_toset;
        bits_toset = FBITMAP_BITS_PER_WORD;
        mask_toset = FBIT_WORD_MASK;
        p++;
    }
    if (nums_set)
    {
        mask_toset &= FBITMAP_LAST_WORD_MASK(size);
        *p |= mask_toset;
    }
}

/**
 * @name: FBitMapClrNBits
 * @msg: Clears a number of consecutive bits starting from a given position in a bitmap.
 * @param {FBitPerWordType*} bitmap - Pointer to the bitmap.
 * @param {u32} start - The starting bit position.
 * @param {u32} nums_clear - The number of bits to clear.
 */
void FBitMapClrNBits(FBitPerWordType *bitmap, u32 start, u32 nums_clear)
{
    FBitPerWordType *p = bitmap + FBITMAP_WORD(start);
    const u32 size = start + nums_clear;
    u16 bits_toclear = FBITMAP_BITS_PER_WORD - (start % FBITMAP_BITS_PER_WORD);
    FBitPerWordType mask_toclear = FBITMAP_FIRST_WORD_MASK(start);

    while (nums_clear >= bits_toclear)
    {
        *p &= ~mask_toclear;
        nums_clear -= bits_toclear;
        bits_toclear = FBITMAP_BITS_PER_WORD;
        mask_toclear = FBIT_WORD_MASK;
        p++;
    }
    if (nums_clear)
    {
        mask_toclear &= FBITMAP_LAST_WORD_MASK(size);
        *p &= ~mask_toclear;
    }
}


/**
 * @name: FBitMapFfz
 * @msg: Finds the first zero bit in a bitmap within a given number of bits.
 * @param {FBitPerWordType*} bitmap - Pointer to the bitmap.
 * @param {u32} num_bits - The number of bits to check.
 * @return {s32} The position of the first zero bit or -1 if not found.
 */
s32 FBitMapFfz(FBitPerWordType *bitmap, u32 num_bits)
{
    s32 bit, i;

    for (i = 0; i < FBITMAP_NUM_WORDS(num_bits); i++)
    {
        if (bitmap[i] == FBIT_WORD_MASK)
        {
            continue;
        }
        bit = i * FBITMAP_BITS_PER_WORD + Ffz(bitmap[i]);
        if (bit < num_bits)
        {
            return bit;
        }
        return -1;
    }
    return -1;
}


/**
 * @name: FBitMapCopy
 * @msg: Copies the content of one bitmap to another.
 * @param {FBitPerWordType*} dst - Pointer to the destination bitmap.
 * @param {const FBitPerWordType*} src - Pointer to the source bitmap.
 * @param {u32} nbits - The number of bits to copy.
 * @return {void} 
 */
static void FBitMapCopy(FBitPerWordType *dst, const FBitPerWordType *src, u32 nbits)
{
    unsigned int len = DIV_ROUND_UP(nbits, FBITMAP_BITS_PER_WORD) * sizeof(FBitPerWordType);
    memcpy(dst, src, len);
}

/**
 * @name: FBitMapCopyClearTail
 * @msg: Copies the content of one bitmap to another and clears the trailing bits of the last word if they are outside the specified range.
 * @param {FBitPerWordType*} dst - Pointer to the destination bitmap.
 * @param {const FBitPerWordType*} src - Pointer to the source bitmap.
 * @param {u32} nbits - The number of bits to copy.
 * @return {void} 
 */
void FBitMapCopyClearTail(FBitPerWordType *dst, const FBitPerWordType *src, u32 nbits)
{
    FBitMapCopy(dst, src, nbits);

    if (nbits % FBITMAP_BITS_PER_WORD)
    {
        dst[nbits / FBITMAP_BITS_PER_WORD] &= FBITMAP_LAST_WORD_MASK(nbits);
    }
}