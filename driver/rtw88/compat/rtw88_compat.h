/*	$NetBSD$	*/

/*-
 * Copyright (c) 2026 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the RK3568 lab (NetBSD/RTL8821CU bring-up).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NetBSD compat layer for the imported rtw88 chip code (external/bsd/rtw88).
 *
 * The files in ../dist are the dual-licensed (GPL-2.0 OR BSD-3-Clause) Linux
 * driver sources and are used unmodified.  They include a pile of Linux
 * kernel headers; this directory provides replacements for exactly those
 * headers and nothing more.  Everything here is NetBSD-side glue, so it is
 * deliberately plain: the chip code only runs at PASSIVE level, driven from
 * a workqueue or the net80211 task context.
 *
 * The two things worth knowing when reading this file:
 *   - the Linux primitives the chip code relies on (sk_buff, workqueue,
 *     completion, jiffies) are thin wrappers over kernel(9) facilities;
 *   - the parts of mac80211 that the chip code merely *compiles against*
 *     live in rtw88_mac80211.h and are stubbed out in rtw88_compat.c.  The
 *     driver proper (if_rtw88.c) never enters those paths: it drives the
 *     chip through the rtw_core and rtw_hci_tx_write entry points and
 *     net80211.
 */

#ifndef _RTW88_COMPAT_H_
#define _RTW88_COMPAT_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/callout.h>
#include <sys/workqueue.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/endian.h>
#include <sys/bitops.h>
#include <sys/atomic.h>
#include <sys/queue.h>
#include <lib/libkern/libkern.h>

/*
 * The imported driver picks little-endian bitfield layouts (phy_cond
 * branches, tx power index diffs) with the Linux-style __LITTLE_ENDIAN
 * macro, which NetBSD never defines - NetBSD spells it _LITTLE_ENDIAN
 * (sys/endian.h).  Bridge the two, or every #ifdef __LITTLE_ENDIAN
 * struct lays out as big-endian: the phy table condition tiles then
 * parse as config pairs and get WRITTEN to the chip (e.g. entry
 * 0x40000000 truncates to vendor register 0x0000 = REG_SYS_CTRL = 0,
 * which kills the MAC clocks and with them the whole switchable
 * register domain).
 */
#if _BYTE_ORDER == _LITTLE_ENDIAN
#ifndef __LITTLE_ENDIAN
#define	__LITTLE_ENDIAN	1234
#endif
#ifndef __BIG_ENDIAN
#define	__BIG_ENDIAN	4321
#endif
#else
#ifndef __BIG_ENDIAN
#define	__BIG_ENDIAN	4321
#endif
#endif
#ifndef __BYTE_ORDER
#if defined(__LITTLE_ENDIAN) && _BYTE_ORDER == _LITTLE_ENDIAN
#define	__BYTE_ORDER	__LITTLE_ENDIAN
#else
#define	__BYTE_ORDER	__BIG_ENDIAN
#endif
#endif

/* ------------------------------------------------------------------ */
/* basic types                                                         */
/* ------------------------------------------------------------------ */

typedef uint8_t			u8;
typedef uint16_t		u16;
typedef uint32_t		u32;
typedef uint64_t		u64;
typedef int8_t			s8;
typedef int16_t			s16;
typedef int32_t			s32;
typedef int64_t			s64;

typedef uint8_t			__u8;
typedef uint16_t		__u16;
typedef uint32_t		__u32;
typedef uint64_t		__u64;
typedef int8_t			__s8;
typedef int16_t			__s16;
typedef int32_t			__s32;
typedef int64_t			__s64;

typedef uint16_t		__le16;
typedef uint16_t		__le32bogus;
typedef uint32_t		__le32;
typedef uint64_t		__le64;
typedef uint16_t		__be16;
typedef uint32_t		__be32;
typedef uint64_t		__be64;

typedef unsigned long		ulong;
typedef unsigned int		uint;
typedef int64_t			loff_t;
typedef uint64_t		dma_addr_t;
typedef unsigned int		gfp_t;

#define	__user
#define	__iomem
#define	__force
#define	__must_check
#define	__init
#define	__exit
#define	__acquires(x)
#define	__releases(x)
#define	__printf_like(a, b)	__printflike(a, b)
#ifndef __printf
#define	__printf(a, b)		__printflike(a, b)
#endif
#ifndef __always_inline
#define	__always_inline		inline __attribute__((__always_inline__))
#endif
#ifndef __noinline
#define	__noinline		__attribute__((__noinline__))
#endif
#ifndef __weak
#define	__weak			__attribute__((__weak__))
#endif
#ifndef __visible
#define	__visible		__attribute__((__visibility__("default")))
#endif
#ifndef __aligned
#define	__aligned(x)		__attribute__((__aligned__(x)))
#endif
#ifndef __section
#define	__section(x)		__attribute__((__section__(x)))
#endif
#ifndef __read_mostly
#define	__read_mostly
#endif
#ifndef fallthrough
#define	fallthrough		do {} while (0)
#endif

#define	U8_MAX		0xffU
#define	U16_MAX		0xffffU
#define	U32_MAX		0xffffffffU
#define	S8_MAX		0x7f
#define	S16_MAX		0x7fff
#define	U8_MIN		0
#define	U16_MIN		0
#define	U32_MIN		0

#define	ENOTSUPP	EOPNOTSUPP

/* C11 spells the keyword _Static_assert */
#ifndef static_assert
#define	static_assert	_Static_assert
#endif

#if __has_attribute(__nonstring__)
#define	__nonstring	__attribute__((__nonstring__))
#else
#define	__nonstring
#endif

#ifndef true
#define	true	1
#endif
#ifndef false
#define	false	0
#endif

#ifndef NULL
#define	NULL	((void *)0)
#endif

/* ------------------------------------------------------------------ */
/* constants and small helpers                                         */
/* ------------------------------------------------------------------ */

#define	BITS_PER_LONG		(8 * sizeof(unsigned long))
#define	BITS_PER_BYTE		8
#ifndef HZ
#define	HZ			hz
#endif
#ifndef PAGE_SIZE
#define	PAGE_SIZE		(1UL << PAGE_SHIFT)
#endif

#ifndef BIT
#define	BIT(n)			(1UL << (n))
#endif

#define	min3(a, b, c)		min(min((a), (b)), (c))
#define	max3(a, b, c)		max(max((a), (b)), (c))
#define	offsetofend(t, m)	(offsetof(t, m) + sizeof(((t *)0)->m))
#define	BIT_ULL(n)		(1ULL << (n))
#ifndef GENMASK
#define	GENMASK(h, l)		(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#endif
#define	GENMASK_ULL(h, l)	(((~0ULL) << (l)) & (~0ULL >> (64 - 1 - (h))))

#define	ARRAY_SIZE(a)		__arraycount(a)
#define	DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#define	DIV_ROUND_UP_ULL(n, d)	DIV_ROUND_UP(n, d)
#define	DIV_ROUND_CLOSEST(x, y)	((((x) < 0) ^ ((y) < 0)) ? \
				  (((x) - ((y) / 2)) / (y)) : \
				  (((x) + ((y) / 2)) / (y)))
#define	round_up(x, y)		((((x) - 1) | ((y) - 1)) + 1)
#define	round_down(x, y)	((x) & ~((y) - 1))
#define	PAGE_ALIGN(x)		round_up(x, PAGE_SIZE)
#ifndef ALIGN
#define	ALIGN(x, a)		round_up(x, a)
#endif
#define	IS_ALIGNED(x, a)	(((x) & ((a) - 1)) == 0)
#define	FIELD_SIZEOF(t, f)	(sizeof(((t *)0)->f))

#define	min(a, b)		((a) < (b) ? (a) : (b))
#define	max(a, b)		((a) > (b) ? (a) : (b))
#define	min_t(t, a, b)		((t)min((a), (b)))
#define	max_t(t, a, b)		((t)max((a), (b)))
#define	clamp_val(v, lo, hi)	max(min((v), (hi)), (lo))
#define	clamp_t(t, v, lo, hi)	((t)clamp_val((v), (lo), (hi)))
#define	swap(a, b)		do { typeof(a) __t = (a); (a) = (b); (b) = __t; } while (0)

/*
 * NetBSD's container_of() refuses array members, which the chip code uses
 * (struct ieee80211_txq in struct ieee80211_sta), so use the Linux form.
 */
#undef container_of
#define	container_of(ptr, type, member)	\
	((type *)(void *)((char *)(void *)(ptr) - offsetof(type, member)))

#define	VERIFY_OCTAL_PERMISSIONS(x)	(x)

#define	abs_diff(a, b)		((a) > (b) ? (a) - (b) : (b) - (a))

/* Linux's fls() is the 1-based position of the highest set bit. */
#define	fls(x)			fls32((uint32_t)(x))

static __always_inline __unused unsigned int
hweight8(u8 w)
{
	unsigned int res = w - ((w >> 1) & 0x55);

	res = (res & 0x33) + ((res >> 2) & 0x33);
	return (res + (res >> 4)) & 0x0f;
}

/* ------------------------------------------------------------------ */
/* Linux bitmap API over unsigned long[] (DECLARE_BITMAP)              */
/* ------------------------------------------------------------------ */

#define	DECLARE_BITMAP(name, bits)	\
	unsigned long name[BITS_TO_LONGS(bits)]
#define	BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_LONG)

static __always_inline __unused void
set_bit(unsigned int nr, volatile unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] |= BIT(nr % BITS_PER_LONG);
}

static __always_inline __unused void
clear_bit(unsigned int nr, volatile unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] &= ~BIT(nr % BITS_PER_LONG);
}

static __always_inline __unused int
test_bit(unsigned int nr, const volatile unsigned long *addr)
{
	return (addr[nr / BITS_PER_LONG] >> (nr % BITS_PER_LONG)) & 1;
}

static __always_inline __unused int
test_and_set_bit(unsigned int nr, volatile unsigned long *addr)
{
	int old = test_bit(nr, addr);
	set_bit(nr, addr);
	return old;
}

static __always_inline __unused int
test_and_clear_bit(unsigned int nr, volatile unsigned long *addr)
{
	int old = test_bit(nr, addr);
	clear_bit(nr, addr);
	return old;
}

static __always_inline __unused void
bitmap_zero(unsigned long *dst, unsigned int nbits)
{
	memset(dst, 0, BITS_TO_LONGS(nbits) * sizeof(unsigned long));
}

static __always_inline __unused unsigned long
find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long i;

	for (i = 0; i < size; i++)
		if (!test_bit(i, addr))
			return i;
	return size;
}

static __always_inline __unused unsigned long
find_next_bit(const unsigned long *addr, unsigned long size, unsigned long off)
{
	unsigned long i;

	for (i = off; i < size; i++)
		if (test_bit(i, addr))
			return i;
	return size;
}

static __always_inline __unused unsigned long
find_next_zero_bit(const unsigned long *addr, unsigned long size,
    unsigned long off)
{
	unsigned long i;

	for (i = off; i < size; i++)
		if (!test_bit(i, addr))
			return i;
	return size;
}

static __always_inline __unused unsigned long
find_first_bit(const unsigned long *addr, unsigned long size)
{

	return find_next_bit(addr, size, 0);
}

/* ------------------------------------------------------------------ */
/* Linux bitfield.h                                                    */
/* ------------------------------------------------------------------ */

#ifndef FIELD_GET
#define	FIELD_GET(_mask, _reg)	\
	((__typeof(_mask))(((_reg) & (_mask)) >> __builtin_ctzl(_mask)))
#endif
#ifndef FIELD_PREP
#define	FIELD_PREP(_mask, _val)	\
	((__typeof(_mask))(((_val) << __builtin_ctzl(_mask)) & (_mask)))
#endif
#define	FIELD_MAX(_mask)	((_mask) >> __builtin_ctzl(_mask))
#define	FIELD_FIT(_mask, _val)	(((typeof(_mask))(_val) << __builtin_ctzl(_mask)) >> __builtin_ctzl(_mask) == (typeof(_mask))(_val))

/*
 * The chip code uses the little-endian bitfield helpers heavily (155 uses of
 * le32p_replace_bits alone); the storage is always a __le32 in a descriptor
 * that is later handed to the hardware as-is, so all of these are plain
 * unaligned-safe shifts on the raw value.
 */
static __always_inline __unused u32
le32_get_bits(__le32 v, u32 mask)
{
	return (le32toh(v) & mask) >> __builtin_ctzl(mask);
}

static __always_inline __unused u32
le16_get_bits(__le16 v, u16 mask)
{
	return (le16toh(v) & mask) >> __builtin_ctzl(mask);
}

static __always_inline __unused u16
u16_encode_bits(u16 v, u16 mask)
{
	return (v << __builtin_ctz(mask)) & mask;
}

static __always_inline __unused __le32
le32_encode_bits(u32 v, u32 mask)
{
	return htole32((v << __builtin_ctzl(mask)) & mask);
}

static __always_inline __unused u64
le64_get_bits(__le64 v, u64 mask)
{
	return (le64toh(v) & mask) >> __builtin_ctzll(mask);
}

static __always_inline __unused u32
u8_get_bits(u8 v, u8 mask)
{
	return (v & mask) >> __builtin_ctz(mask);
}

static __always_inline __unused u32
u16_get_bits(u16 v, u16 mask)
{
	return (v & mask) >> __builtin_ctz(mask);
}

static __always_inline __unused u32
get_bits(u32 v, u32 mask)
{
	return (v & mask) >> __builtin_ctzl(mask);
}

static __always_inline __unused u32
u32_get_bits(u32 v, u32 mask)
{
	return get_bits(v, mask);
}

static __always_inline __unused u32
u32_encode_bits(u32 v, u32 mask)
{
	return (v << __builtin_ctzl(mask)) & mask;
}

static __always_inline __unused u64
u64_encode_bits(u64 v, u64 mask)
{
	return (v << __builtin_ctzll(mask)) & mask;
}

static __always_inline __unused u16
le16_encode_bits(u16 v, u16 mask)
{
	return (u16)((v << __builtin_ctzl(mask)) & mask);
}

static __always_inline __unused void
__le32p_replace_bits(__le32 *p, u32 v, u32 mask)
{
	u32 val = le32toh(*p);

	val = (val & ~mask) | ((v << __builtin_ctzl(mask)) & mask);
	*p = htole32(val);
}

static __always_inline __unused void
le32p_replace_bits(__le32 *p, u32 v, u32 mask)
{
	__le32p_replace_bits(p, v, mask);
}

static __always_inline __unused void
u8p_replace_bits(u8 *p, u8 v, u8 mask)
{
	*p = (*p & ~mask) | ((v << __builtin_ctzl(mask)) & mask);
}

static __always_inline __unused void
u32p_replace_bits(u32 *p, u32 v, u32 mask)
{
	*p = (*p & ~mask) | ((v << __builtin_ctzl(mask)) & mask);
}

/* ------------------------------------------------------------------ */
/* byte order (asm/byteorder.h)                                        */
/* ------------------------------------------------------------------ */

#define	cpu_to_le16(x)		htole16(x)
#define	cpu_to_le32(x)		htole32(x)
#define	cpu_to_le64(x)		htole64(x)
#define	le16_to_cpu(x)		le16toh(x)
#define	le32_to_cpu(x)		le32toh(x)
#define	le64_to_cpu(x)		le64toh(x)
#define	cpu_to_be16(x)		htobe16(x)
#define	cpu_to_be32(x)		htobe32(x)
#define	be16_to_cpu(x)		be16toh(x)
#define	be32_to_cpu(x)		be32toh(x)
#define	__le16_to_cpu(x)	le16toh(x)
#define	__le32_to_cpu(x)	le32toh(x)
#define	__cpu_to_le16(x)	htole16(x)
#define	__cpu_to_le32(x)	htole32(x)
#define	get_unaligned_le32(p)	le32toh(*(const __le32 *)(const void *)(p))
#define	put_unaligned_le32(v, p) (*(__le32 *)(void *)(p) = htole32(v))

/* ------------------------------------------------------------------ */
/* memory allocation                                                   */
/* ------------------------------------------------------------------ */

/*
 * GFP_* only selects wait-or-not here: kmem_alloc() either sleeps or fails.
 */
#define	GFP_KERNEL		KM_SLEEP
#define	GFP_ATOMIC		KM_NOSLEEP
#define	GFP_DMA			KM_SLEEP
#define	GFP_NOWAIT		KM_NOSLEEP
#define	__GFP_ZERO		0x80000000

/*
 * Linux allocations carry no size at free time, but kmem_free() requires it,
 * so every block gets a size header that kfree()/vfree() peel off again.
 */
static __always_inline __unused void *
rtw88_kmem_alloc(size_t size, int flags, bool zero)
{
	size_t *p;

	p = kmem_alloc(size + sizeof(size_t), flags);
	if (p == NULL)
		return NULL;
	if (zero)
		memset(p, 0, size + sizeof(size_t));
	*p = size;
	return p + 1;
}

static __always_inline __unused void
rtw88_kmem_free(const void *ptr)
{
	size_t *p;

	if (ptr == NULL)
		return;
	p = (size_t *)(uintptr_t)ptr - 1;
	kmem_free(p, *p + sizeof(size_t));
}

static __always_inline __unused void *
kmalloc(size_t size, gfp_t flags)
{
	return rtw88_kmem_alloc(size, (int)flags, false);
}

static __always_inline __unused void *
kzalloc(size_t size, gfp_t flags)
{
	return rtw88_kmem_alloc(size, (int)flags, true);
}

static __always_inline __unused void *
kcalloc(size_t n, size_t size, gfp_t flags)
{
	return rtw88_kmem_alloc(n * size, (int)flags, true);
}

static __always_inline __unused void
kfree(const void *p)
{
	rtw88_kmem_free(p);
}

static __always_inline __unused void *
kmemdup(const void *src, size_t len, gfp_t flags)
{
	void *p = kmalloc(len, flags);

	if (p != NULL)
		memcpy(p, src, len);
	return p;
}

static __always_inline __unused void *
vmalloc(size_t size)
{
	return rtw88_kmem_alloc(size, KM_SLEEP, false);
}

static __always_inline __unused void *
vzalloc(size_t size)
{
	return rtw88_kmem_alloc(size, KM_SLEEP, true);
}

#define	kvmalloc(size, flags)	kmalloc((size), (flags))

static __always_inline __unused void
vfree(const void *p)
{
	rtw88_kmem_free(p);
}

/*
 * "devm" allocations are not tracked: the driver lives as long as the kernel
 * and each boot starts from a clean slate, so they are plain allocations.
 */
static __always_inline __unused void *
devm_kmalloc(struct device *dev, size_t size, gfp_t flags)
{
	(void)dev;
	return kmalloc(size, flags);
}

static __always_inline __unused void *
devm_kzalloc(struct device *dev, size_t size, gfp_t flags)
{
	(void)dev;
	return kzalloc(size, flags);
}

static __always_inline __unused void *
devm_kmemdup(struct device *dev, const void *src, size_t len, gfp_t flags)
{
	(void)dev;
	return kmemdup(src, len, flags);
}

static __always_inline __unused void *
devm_kmemdup_array(struct device *dev, const void *src, size_t n, size_t size,
    gfp_t flags)
{
	(void)dev;
	return kmemdup(src, n * size, flags);
}

/* Used by the 6.14+ allocation API the chip code was written against. */
#define	kzalloc_obj(p)		kzalloc(sizeof(p), GFP_KERNEL)

/* ------------------------------------------------------------------ */
/* lists (linux/list.h)                                                */
/* ------------------------------------------------------------------ */

struct list_head {
	struct list_head *next, *prev;
};

#define	LIST_HEAD_INIT(name)	{ &(name), &(name) }

static __always_inline __unused void
INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static __always_inline __unused void
__list_add(struct list_head *n, struct list_head *prev,
    struct list_head *next)
{
	next->prev = n;
	n->next = next;
	n->prev = prev;
	prev->next = n;
}

static __always_inline __unused void
list_add(struct list_head *n, struct list_head *head)
{
	__list_add(n, head, head->next);
}

static __always_inline __unused void
list_add_tail(struct list_head *n, struct list_head *head)
{
	__list_add(n, head->prev, head);
}

static __always_inline __unused void
__list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static __always_inline __unused void
list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static __always_inline __unused void
list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

static __always_inline __unused int
list_empty(const struct list_head *head)
{
	return head->next == head;
}

static __always_inline __unused void
list_move_tail(struct list_head *list, struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add_tail(list, head);
}

#define	list_entry(ptr, type, member)	container_of(ptr, type, member)
#define	list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)
#define	list_first_entry_or_null(ptr, type, member) \
	(!list_empty(ptr) ? list_first_entry(ptr, type, member) : NULL)
#define	list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

#define	list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)
#define	list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_next_entry(pos, member))
#define	list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_first_entry(head, typeof(*pos), member),	\
	     n = list_next_entry(pos, member);				\
	     &pos->member != (head);					\
	     pos = n, n = list_next_entry(n, member))

/* ------------------------------------------------------------------ */
/* LED class (only the type: rtw_led_init() is a no-op without          */
/* CONFIG_RTW88_LEDS)                                                   */
/* ------------------------------------------------------------------ */

enum led_brightness {
	LED_OFF		= 0,
	LED_ON		= 1,
	LED_HALF	= 127,
	LED_FULL	= 255,
};

struct led_classdev {
	const char		*name;
	enum led_brightness	 brightness;
	enum led_brightness	 max_brightness;
	int			 flags;
	void			(*brightness_set)(struct led_classdev *,
					    enum led_brightness);
	int			(*brightness_set_blocking)(struct led_classdev *,
					    enum led_brightness);
};

/* ------------------------------------------------------------------ */
/* logging                                                             */
/* ------------------------------------------------------------------ */

/*
 * struct device is only a name carrier here: the chip code passes it to
 * dev_*() and to request_firmware_nowait(), nothing else.  The port's
 * autoconf(9) shell already provides the type (dv_xname/dv_private), so
 * the Linux view reuses it instead of declaring a second struct device.
 */

struct seq_file;
struct file;

#define	KERN_ERR	""
#define	KERN_WARNING	""
#define	KERN_INFO	""
#define	KERN_DEBUG	""

struct va_format {
	const char	*fmt;
	va_list		*va;
};

/*
 * The dist print helpers funnel everything through dev_printk(); the debug
 * paths (rtw_dbg et. al.) use the Linux "%pV" wrapped-format convention,
 * which NetBSD's printf does not understand -- without the special case it
 * prints the pointer instead of the message.
 */
static __unused void
rtw88_vdev_printk(struct device *dev, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	printf("%s: ", device_xname(dev));
	if (strstr(fmt, "%pV") != NULL) {
		const struct va_format *vaf = va_arg(ap,
		    const struct va_format *);

		vprintf(vaf->fmt, *vaf->va);
	} else {
		vprintf(fmt, ap);
	}
	va_end(ap);
}

#define	dev_printk(level, dev, fmt, ...)				\
	rtw88_vdev_printk((dev), (fmt), ##__VA_ARGS__)
#define	dev_err(dev, fmt, ...)	dev_printk(KERN_ERR, dev, fmt, ##__VA_ARGS__)
#define	dev_warn(dev, fmt, ...)	dev_printk(KERN_WARNING, dev, fmt, ##__VA_ARGS__)
#define	dev_info(dev, fmt, ...)	dev_printk(KERN_INFO, dev, fmt, ##__VA_ARGS__)
#define	dev_notice(dev, fmt, ...) dev_info(dev, fmt, ##__VA_ARGS__)
#define	dev_dbg(dev, fmt, ...)	dev_printk(KERN_DEBUG, dev, fmt, ##__VA_ARGS__)
#define	dev_warn_once(dev, fmt, ...) dev_warn(dev, fmt, ##__VA_ARGS__)
#define	dev_err_once(dev, fmt, ...) dev_err(dev, fmt, ##__VA_ARGS__)
#define	dev_dbg_ratelimited(dev, fmt, ...) do { } while (0)
#define	pr_err(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#define	pr_warn(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#define	pr_info(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#define	pr_debug(fmt, ...)	do { } while (0)

static __always_inline __unused const char *
dev_name(const struct device *dev)
{
	return device_xname(dev);
}

#define	dev_coredumpv(dev, data, size, gfp)	do { (void)(dev); (void)(data); (void)(size); } while (0)
#define	dev_coredump(dev, data, size)		do { (void)(dev); (void)(data); (void)(size); } while (0)

/* ------------------------------------------------------------------ */
/* module plumbing (the chip code is built into the kernel)            */
/* ------------------------------------------------------------------ */

#define	THIS_MODULE		NULL
#define	EXPORT_SYMBOL(x)
#define	EXPORT_SYMBOL_GPL(x)
#define	MODULE_AUTHOR(x)
#define	MODULE_DESCRIPTION(x)
#define	MODULE_LICENSE(x)
#define	MODULE_FIRMWARE(x)
#define	MODULE_PARM_DESC(x, y)
#define	module_param_named(a, b, c, d)
#define	module_param(a, b, c)
#define	MODULE_DEVICE_TABLE(t, n)

/* ------------------------------------------------------------------ */
/* bugs and assertions                                                 */
/* ------------------------------------------------------------------ */

#define	WARN_ON(cond)		({					\
	bool __w = (cond);						\
	if (__w)							\
		printf("%s:%d: WARN_ON(%s)\n", __FILE__, __LINE__, #cond); \
	__w;								\
})
#define	WARN_ON_ONCE(cond)	WARN_ON(cond)
#define	WARN(cond, fmt, ...)	WARN_ON(cond)
#define	BUG_ON(cond)		do { if (cond) panic("%s:%d: BUG_ON(%s)", \
				    __FILE__, __LINE__, #cond); } while (0)
#define	BUILD_BUG_ON(cond)	do { } while (0)
#define	BUILD_BUG_ON_ZERO(cond)	0
#define	might_sleep()		do { } while (0)
#define	might_sleep_if(cond)	do { } while (0)
#define	lockdep_assert_held(l)	do { } while (0)
#define	lockdep_assert_held_read(l) do { } while (0)
#define	DEBUG_LOCKS_WARN_ON(c)	0

static __always_inline __unused void
dump_stack(void)
{
}

/* ------------------------------------------------------------------ */
/* locking: mutex (linux/mutex.h)                                      */
/* ------------------------------------------------------------------ */

struct mutex {
	kmutex_t	mtx;
};

/*
 * The Linux names are shadowed at the end of this file, so the NetBSD
 * entry points have to stay reachable under their own names here.
 */
static __always_inline __unused void
netbsd_mutex_init(kmutex_t *m)
{
	mutex_init(m, MUTEX_DEFAULT, IPL_NONE);
}

static __always_inline __unused void
netbsd_mutex_destroy(kmutex_t *m)
{
	mutex_destroy(m);
}

/* spin mutex: safe to take from softint context */
static __always_inline __unused void
netbsd_spin_mutex_init(kmutex_t *m)
{
	mutex_init(m, MUTEX_SPIN, IPL_VM);
}

static __always_inline __unused void
mutex_init_rtw(struct mutex *m)
{
	netbsd_mutex_init(&m->mtx);
}

static __always_inline __unused void
mutex_destroy_rtw(struct mutex *m)
{
	netbsd_mutex_destroy(&m->mtx);
}

/* ------------------------------------------------------------------ */
/* locking: spinlock (linux/spinlock.h)                                */
/* ------------------------------------------------------------------ */

/*
 * The chip code takes these from workqueue context only (TX queue lists,
 * the USB register scratch buffer), never from an interrupt handler, so a
 * passive-level mutex is enough.
 */
typedef struct mutex	spinlock_t;

#define	spin_lock_init(l)	mutex_init(l)
#define	spin_lock(l)		mutex_lock(l)
#define	spin_unlock(l)		mutex_unlock(l)
#define	spin_lock_bh(l)		mutex_lock(l)
#define	spin_unlock_bh(l)	mutex_unlock(l)
#define	spin_lock_irqsave(l, flags)					\
	do { (flags) = 0; mutex_lock(l); } while (0)
#define	spin_unlock_irqrestore(l, flags)				\
	do { (void)(flags); mutex_unlock(l); } while (0)
#define	spin_lock_irq(l)	mutex_lock(l)
#define	spin_unlock_irq(l)	mutex_unlock(l)
#define	spin_trylock(l)		mutex_trylock(l)
#define	spin_is_locked(l)	mutex_is_locked(l)


/* ------------------------------------------------------------------ */
/* RCU: only used on the mac80211 txq path, which is not driven here   */
/* ------------------------------------------------------------------ */

#define	rcu_read_lock()		do { } while (0)
#define	rcu_read_unlock()	do { } while (0)
#define	rcu_dereference(p)	(p)
#define	rcu_access_pointer(p)	(p)
#define	rcu_assign_pointer(p, v) do { (p) = (v); } while (0)
#define	synchronize_rcu()

/* ------------------------------------------------------------------ */
/* atomics (linux/atomic.h)                                            */
/* ------------------------------------------------------------------ */

typedef struct {
	volatile int counter;
} atomic_t;

#define	ATOMIC_INIT(i)		{ .counter = (i) }
#define	atomic_read(v)		((v)->counter)
#define	atomic_set(v, i)	do { (v)->counter = (i); } while (0)
#define	atomic_inc(v)		atomic_inc_32((volatile unsigned int *)&(v)->counter)
#define	atomic_dec(v)		atomic_dec_32((volatile unsigned int *)&(v)->counter)
#define	atomic_inc_return(v)	((int)atomic_inc_32_nv((volatile unsigned int *)&(v)->counter))
#define	atomic_dec_and_test(v)	(atomic_dec_32_nv((volatile unsigned int *)&(v)->counter) == 0)

/* ------------------------------------------------------------------ */
/* time (linux/{jiffies,delay,ktime}.h)                                */
/* ------------------------------------------------------------------ */

/*
 * jiffies is getticks(): both count hardclock ticks, so HZ maps to hz and
 * msecs_to_jiffies() keeps the arithmetic the chip code expects.
 */
#define	jiffies			((unsigned long)getticks())
#define	MAX_JIFFY_OFFSET	((long)(~0UL >> 1) - 1)
#define	time_after(a, b)	((long)((b) - (a)) < 0)
#define	time_before(a, b)	time_after(b, a)
#define	time_after_eq(a, b)	((long)((a) - (b)) >= 0)
#define	time_before_eq(a, b)	time_after_eq(b, a)
#define	msecs_to_jiffies(m)	((unsigned long)(((m) * HZ + 999) / 1000))
#define	usecs_to_jiffies(u)	msecs_to_jiffies((u) / 1000)
#define	jiffies_to_msecs(j)	((unsigned int)(((j) * 1000) / HZ))
#define	jiffies_to_usecs(j)	((unsigned int)(((j) * 1000000) / HZ))
#define	round_jiffies_relative(j) ((unsigned long)(j))

typedef int64_t	ktime_t;

#define	KTIME_MAX	((ktime_t)INT64_MAX)

static __always_inline __unused ktime_t
ktime_get(void)
{
	/*
	 * The port has no wall clock; the chip code only ever compares two
	 * of these (elapsed time), so a monotonic tick timeline serves and
	 * keeps this layer free of an OS time API that neither OSAL has.
	 */
	return (ktime_t)getticks() * (1000000000LL / hz);
}

static __always_inline __unused ktime_t
ktime_add_us(ktime_t kt, u64 us)
{
	return kt + (ktime_t)us * 1000;
}

static __always_inline __unused ktime_t
ktime_sub(ktime_t a, ktime_t b)
{
	return a - b;
}

static __always_inline __unused int
ktime_compare(ktime_t a, ktime_t b)
{
	return a < b ? -1 : (a > b ? 1 : 0);
}

static __always_inline __unused s64
ktime_to_us(ktime_t kt)
{
	return kt / 1000;
}

static __always_inline __unused void
udelay(unsigned long us)
{
	delay((unsigned int)(us < 1 ? 1 : us));
}

static __always_inline __unused void
ndelay(unsigned long ns)
{
	delay(1);
}

static __always_inline __unused void
mdelay(unsigned long ms)
{
	delay((unsigned int)(ms * 1000));
}

/* sleep ident for the pure timed sleeps below: nothing ever wakes it, so
 * each translation unit having its own copy of the address is harmless */
static __unused int rtw88_sleep_ident;

static __always_inline __unused void
usleep_range(unsigned long lo, unsigned long hi)
{
	(void)hi;
	tsleep(&rtw88_sleep_ident, 0, "rtw88slp",
	    MAX(1, (int)mstohz((unsigned int)lo)));
}

static __always_inline __unused void
msleep(unsigned int ms)
{
	tsleep(&rtw88_sleep_ident, 0, "rtw88slp",
	    MAX(1, (int)mstohz(ms)));
}

static __always_inline __unused void
msleep_interruptible(unsigned int ms)
{
	tsleep(&rtw88_sleep_ident, 0, "rtw88slp",
	    MAX(1, (int)mstohz(ms)));
}

static __always_inline __unused void
fsleep(unsigned long us)
{
	if (us <= 10)
		udelay(us);
	else if (us <= 20000)
		usleep_range(us, 2 * us);
	else
		msleep(DIV_ROUND_UP((unsigned int)us, 1000));
}

static __always_inline __unused void
sleep_before_read(void)
{
}

/* ------------------------------------------------------------------ */
/* completions (linux/completion.h)                                    */
/* ------------------------------------------------------------------ */

struct completion {
	kcondvar_t	cv;
	kmutex_t	mtx;
	bool		done;
};

static __always_inline __unused void
init_completion(struct completion *c)
{
	cv_init(&c->cv, "rtw88cmp");
	mutex_init(&c->mtx, MUTEX_DEFAULT, IPL_NONE);
	c->done = false;
}

static __always_inline __unused void
reinit_completion(struct completion *c)
{
	mutex_enter(&c->mtx);
	c->done = false;
	mutex_exit(&c->mtx);
}

static __always_inline __unused void
complete(struct completion *c)
{
	mutex_enter(&c->mtx);
	c->done = true;
	cv_broadcast(&c->cv);
	mutex_exit(&c->mtx);
}

static __always_inline __unused void
complete_all(struct completion *c)
{
	complete(c);
}

static __always_inline __unused void
wait_for_completion(struct completion *c)
{
	mutex_enter(&c->mtx);
	while (!c->done)
		cv_wait(&c->cv, &c->mtx);
	mutex_exit(&c->mtx);
}

static __always_inline __unused int
wait_for_completion_timeout(struct completion *c, unsigned long jiff)
{
	int error = 0;

	mutex_enter(&c->mtx);
	while (!c->done && error == 0)
		error = cv_timedwait(&c->cv, &c->mtx,
		    MAX(1, (int)mstohz((unsigned int)jiffies_to_msecs(jiff))));
	mutex_exit(&c->mtx);
	return error == 0 ? 1 : 0;
}

static __always_inline __unused bool
try_wait_for_completion(struct completion *c)
{
	bool done;

	mutex_enter(&c->mtx);
	done = c->done;
	mutex_exit(&c->mtx);
	return done;
}

static __always_inline __unused bool
completion_done(const struct completion *c)
{
	return c->done;
}

/* ------------------------------------------------------------------ */
/* workqueues (linux/workqueue.h)                                      */
/* ------------------------------------------------------------------ */

struct work_struct;
typedef void (*work_func_t)(struct work_struct *);

struct work_struct {
	struct work		wk_work;
	work_func_t		wk_func;
	struct workqueue	*wk_wq;
	volatile unsigned int	wk_queued;
	volatile unsigned int	wk_running;
};

struct delayed_work {
	struct work_struct	work;
	struct callout		dw_callout;
	volatile bool		dw_scheduled;
	volatile bool		dw_cancel;
	unsigned long		dw_delay;
};

struct workqueue_struct {
	struct workqueue	*wq_wq;
};


/*
 * Work items are queued on the rtw88 workqueue; delayed work uses a callout
 * that enqueues the (non-delayed) work, which is the usual NetBSD spelling
 * of the same construct.
 */
struct workqueue *rtw88_workqueue_alloc(const char *name);
void	rtw88_workqueue_free(struct workqueue *);
void	rtw88_work_flush(struct work_struct *);

#define	INIT_WORK(w, f)		do {					\
	(w)->wk_func = (f);						\
	(w)->wk_queued = 0;						\
} while (0)

#define	INIT_DELAYED_WORK(w, f)	do {					\
	INIT_WORK(&(w)->work, (f));					\
	callout_init(&(w)->dw_callout, 0);				\
	(w)->dw_scheduled = false;					\
	(w)->dw_cancel = false;						\
	(w)->dw_delay = 0;						\
} while (0)

#define	DECLARE_WORK(w, f)	struct work_struct w = { .wk_func = (f) }
#define	DECLARE_DELAYED_WORK(w, f)	struct delayed_work w = {	\
					.work = { .wk_func = (f) } }

/*
 * NetBSD's workqueue panics if the same work item is queued twice (it has no
 * per-item pending state of its own), so queueing is made idempotent here:
 * an item that has not run yet is not queued again.
 */
void	rtw88_work_enqueue_safe(struct work_struct *);

static __always_inline __unused void
rtw88_work_enqueue(struct work_struct *w)
{
	if (w->wk_wq == NULL)
		w->wk_wq = rtw88_workqueue_alloc("rtw88");
	rtw88_work_enqueue_safe(w);
}

#define	queue_work(wq, w)						\
	({								\
		(w)->wk_wq = (wq)->wq_wq;				\
		rtw88_work_enqueue(w);					\
		true;							\
	})

#define	schedule_work(w)		rtw88_work_enqueue(w)

void	rtw88_delayed_work_callout(void *);

static __always_inline __unused bool
schedule_delayed_work(struct delayed_work *dw, unsigned long ticks)
{

	dw->dw_delay = ticks;
	dw->dw_cancel = false;
	dw->dw_scheduled = true;
	callout_reset(&dw->dw_callout, MAX(1, (int)ticks),
	    rtw88_delayed_work_callout, dw);
	return true;
}

#define	queue_delayed_work(wq, dw, ticks)				\
	({								\
		(dw)->work.wk_wq = (wq)->wq_wq;				\
		schedule_delayed_work((dw), (ticks));			\
	})

#define	mod_delayed_work(wq, dw, ticks)	queue_delayed_work(wq, dw, ticks)
#define	schedule_delayed_work_on(cpu, dw, ticks) schedule_delayed_work(dw, ticks)

static __always_inline __unused bool
cancel_delayed_work(struct delayed_work *dw)
{
	bool was;

	was = callout_stop(&dw->dw_callout) != 0;
	dw->dw_scheduled &= !was;
	return was;
}

static __always_inline __unused bool
cancel_delayed_work_sync(struct delayed_work *dw)
{
	bool was = cancel_delayed_work(dw);

	callout_halt(&dw->dw_callout, NULL);
	rtw88_work_flush(&dw->work);
	return was;
}

#define	cancel_work(w)		((void)0)
#define	cancel_work_sync(w)	rtw88_work_flush(w)
#define	flush_workqueue(wq)	do { } while (0)
#define	flush_work(w)		rtw88_work_flush(w)
#define	flush_delayed_work(w)	rtw88_work_flush(&(w)->work)
#define	destroy_workqueue(wq)	rtw88_workqueue_free((wq)->wq_wq)

static __always_inline __unused struct workqueue_struct *
alloc_workqueue(const char *fmt, unsigned int flags, int max_active, ...)
{
	struct workqueue_struct *wq;

	(void)flags;
	(void)max_active;
	wq = kmem_zalloc(sizeof(*wq), KM_SLEEP);
	wq->wq_wq = rtw88_workqueue_alloc(fmt);
	return wq;
}

#define	create_singlethread_workqueue(name)	alloc_workqueue(name, 0, 1)
#define	WQ_HIGHPRI	0
#define	WQ_MEM_RECLAIM	0
#define	WQ_UNBOUND	0
#define	system_wq	NULL

static __always_inline __unused bool
schedule_work_on(int cpu, struct work_struct *w)
{
	(void)cpu;
	rtw88_work_enqueue(w);
	return true;
}

/* ------------------------------------------------------------------ */
/* timers (linux/timer.h)                                              */
/* ------------------------------------------------------------------ */

struct timer_list {
	struct callout		tl_callout;
	void			(*tl_func)(struct timer_list *);
	volatile bool		tl_pending;
};

#define	timer_setup(t, f, flags)	do {				\
	(t)->tl_func = (f);						\
	callout_init(&(t)->tl_callout, 0);				\
	(t)->tl_pending = false;					\
} while (0)

void	rtw88_timer_callout(void *);

#define	mod_timer(t, expires)						\
	do {								\
		unsigned long __now = jiffies;				\
		int __ticks = (int)((long)((expires) - __now));		\
		(t)->tl_pending = true;					\
		callout_reset(&(t)->tl_callout, MAX(1, __ticks),	\
		    rtw88_timer_callout, (t));				\
	} while (0)

#define	del_timer(t)		(((t)->tl_pending = false),		\
				 callout_stop(&(t)->tl_callout) != 0)
#define	del_timer_sync(t)	do {					\
	(t)->tl_pending = false;					\
	callout_halt(&(t)->tl_callout, NULL);				\
} while (0)
#define	timer_delete_sync(t)	del_timer_sync(t)
#define	timer_pending(t)	((t)->tl_pending)
#define	from_timer(var, cb, field)	\
	container_of(cb, typeof(*var), field)
#define	timer_shutdown_sync(t)	del_timer_sync(t)

/* ------------------------------------------------------------------ */
/* sk_buff (linux/skbuff.h)                                            */
/* ------------------------------------------------------------------ */

struct sk_buff_head {
	struct sk_buff	*next;
	struct sk_buff	*prev;
	u32		qlen;
};

struct sk_buff {
	struct sk_buff	*next;
	struct sk_buff	*prev;
	struct sk_buff_head *list;

	unsigned char	*head;
	unsigned char	*data;
	unsigned int	tail;
	unsigned int	end;
	unsigned int	len;
	unsigned int	data_len;
	unsigned int	truesize;

	u16		protocol;
	u16		queue_mapping;
	u32		priority;
	u8		pkt_type;
	u8		ip_summed;

	void		*dev;		/* struct device * */
	void		*sk;

	/* mac80211 control/status block */
	unsigned long	cb[48 / sizeof(unsigned long)];
} __aligned(4);

#define	skb_queue_empty(q)	((q)->qlen == 0)
#define	skb_queue_len(q)	((q)->qlen)
#define	skb_headroom(skb)	((unsigned int)((skb)->data - (skb)->head))
#define	skb_tailroom(skb)	((int)((skb)->end - (skb)->tail))
#define	skb_priv(skb)		((void *)(skb)->cb)

void	*rtw88_skb_alloc(unsigned int size, gfp_t gfp);
void	rtw88_skb_free(struct sk_buff *);
void	rtw88_skb_queue_init(struct sk_buff_head *);
void	rtw88_skb_queue_tail(struct sk_buff_head *, struct sk_buff *);
void	rtw88_skb_queue_head(struct sk_buff_head *, struct sk_buff *);
struct sk_buff *rtw88_skb_dequeue(struct sk_buff_head *);
void	rtw88_skb_unlink(struct sk_buff *, struct sk_buff_head *);
void	rtw88_skb_queue_purge(struct sk_buff_head *);

#define	alloc_skb(size, gfp)	rtw88_skb_alloc((size), (gfp))
#define	dev_alloc_skb(size)	rtw88_skb_alloc((size), GFP_ATOMIC)
#define	kfree_skb(skb)		rtw88_skb_free(skb)
#define	kfree_skb_any(skb)	rtw88_skb_free(skb)
#define	dev_kfree_skb(skb)	rtw88_skb_free(skb)
#define	dev_kfree_skb_any(skb)	rtw88_skb_free(skb)
#define	consume_skb(skb)	rtw88_skb_free(skb)
#define	skb_new(size, gfp)	rtw88_skb_alloc((size), (gfp))

#define	skb_queue_head_init(q)	rtw88_skb_queue_init(q)
#define	skb_queue_tail(q, skb)	rtw88_skb_queue_tail((q), (skb))
#define	__skb_queue_tail(q, skb) rtw88_skb_queue_tail((q), (skb))
#define	skb_queue_head(q, skb)	rtw88_skb_queue_head((q), (skb))
#define	skb_dequeue(q)		rtw88_skb_dequeue(q)
#define	__skb_dequeue(q)	rtw88_skb_dequeue(q)
#define	skb_unlink(skb, q)	rtw88_skb_unlink((skb), (q))
#define	__skb_unlink(skb, q)	rtw88_skb_unlink((skb), (q))
#define	skb_queue_purge(q)	rtw88_skb_queue_purge(q)
#define	skb_peek(q)		((q)->next)
#define	skb_queue_walk(q, skb)						\
	for ((skb) = (q)->next;						\
	     (skb) != (struct sk_buff *)(q);				\
	     (skb) = (skb)->next)
/*
 * The queue head terminates the list (rtw88_skb_queue_init sets q->next
 * = (struct sk_buff *)q), so the walk must stop on the head - testing
 * for NULL instead iterates the head itself and the caller frees or
 * dereferences a struct sk_buff_head as if it were a packet.
 */
#define	skb_queue_walk_safe(q, skb, tmp)				\
	for ((skb) = (q)->next, (tmp) = (skb)->next;			\
	     (skb) != (struct sk_buff *)(q);				\
	     (skb) = (tmp), (tmp) = (skb)->next)

static __always_inline __unused unsigned int
skb_headlen(const struct sk_buff *skb)
{
	return skb->len - skb->data_len;
}

static __always_inline __unused void
skb_reset_tail_pointer(struct sk_buff *skb)
{
	skb->tail = (unsigned int)(skb->data - skb->head);
}

static __always_inline __unused void
skb_reserve(struct sk_buff *skb, unsigned int len)
{
	skb->data += len;
	skb->tail += len;
}

static __always_inline __unused unsigned char *
skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp = skb->head + skb->tail;

	skb->tail += len;
	skb->len += len;
	if (skb->tail > skb->end)
		panic("%s: skb overrun", __func__);
	return tmp;
}

/* Linux returns void * here since 6.4, which the chip code relies on */
static __always_inline __unused void *
skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data -= len;
	skb->len += len;
	if (skb->data < skb->head)
		panic("%s: skb underrun", __func__);
	return skb->data;
}

static __always_inline __unused unsigned char *
skb_pull(struct sk_buff *skb, unsigned int len)
{
	skb->len -= len;
	skb->data += len;
	return skb->data;
}

static __always_inline __unused unsigned char *
__skb_put(struct sk_buff *skb, unsigned int len)
{
	return skb_put(skb, len);
}

static __always_inline __unused void *
skb_put_zero(struct sk_buff *skb, unsigned int len)
{
	unsigned char *p = skb_put(skb, len);

	memset(p, 0, len);
	return p;
}

static __always_inline __unused void *
skb_put_data(struct sk_buff *skb, const void *data, unsigned int len)
{
	unsigned char *p = skb_put(skb, len);

	memcpy(p, data, len);
	return p;
}

static __always_inline __unused void
skb_copy_to_linear_data(struct sk_buff *skb, const void *from,
    unsigned int len)
{
	memcpy(skb->data, from, len);
}

static __always_inline __unused struct sk_buff *
skb_copy(const struct sk_buff *skb, gfp_t gfp)
{
	struct sk_buff *n;

	n = rtw88_skb_alloc(skb->end, gfp);
	if (n == NULL)
		return NULL;
	memcpy(n->data, skb->data, skb->len);
	n->len = skb->len;
	n->tail = skb->tail;
	n->protocol = skb->protocol;
	n->priority = skb->priority;
	memcpy(n->cb, skb->cb, sizeof(n->cb));
	return n;
}

static __always_inline __unused void
skb_reset_mac_header(struct sk_buff *skb)
{
}

static __always_inline __unused u16
skb_get_queue_mapping(const struct sk_buff *skb)
{
	return skb->queue_mapping;
}

static __always_inline __unused void
skb_set_queue_mapping(struct sk_buff *skb, u16 q)
{
	skb->queue_mapping = q;
}

/* ------------------------------------------------------------------ */
/* ethernet helpers (linux/etherdevice.h)                              */
/* ------------------------------------------------------------------ */

#ifndef ETH_ALEN
#define	ETH_ALEN	6
#endif
#ifndef ETH_P_PAE
#define	ETH_P_PAE	0x888E
#endif
#ifndef ETH_P_IP
#define	ETH_P_IP	0x0800
#endif

static __always_inline __unused void
ether_addr_copy(u8 *dst, const u8 *src)
{
	memcpy(dst, src, ETH_ALEN);
}

static __always_inline __unused bool
ether_addr_equal(const u8 *a, const u8 *b)
{

	return memcmp(a, b, ETH_ALEN) == 0;
}

static __always_inline __unused bool
is_multicast_ether_addr(const u8 *addr)
{
	return (addr[0] & 0x01) != 0;
}

static __always_inline __unused bool
is_broadcast_ether_addr(const u8 *addr)
{
	return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] &
	    addr[5]) == 0xff;
}

static __always_inline __unused bool
is_zero_ether_addr(const u8 *addr)
{
	return (addr[0] | addr[1] | addr[2] | addr[3] | addr[4] |
	    addr[5]) == 0;
}

static __always_inline __unused bool
is_valid_ether_addr(const u8 *addr)
{
	return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

static __always_inline __unused bool
is_unicast_ether_addr(const u8 *addr)
{
	return !is_multicast_ether_addr(addr);
}

static __always_inline __unused void
eth_zero_addr(u8 *addr)
{
	memset(addr, 0, ETH_ALEN);
}

static __always_inline __unused void
eth_broadcast_addr(u8 *addr)
{
	memset(addr, 0xff, ETH_ALEN);
}

static __always_inline __unused void
eth_random_addr(u8 *addr)
{
	uint32_t r = (uint32_t)random();

	addr[0] = (u8)(r >> 24) & 0xfe;	/* unicast, locally administered */
	addr[1] = (u8)(r >> 16);
	addr[2] = (u8)(r >> 8);
	addr[3] = (u8)r;
	addr[4] = (u8)(r >> 24);
	addr[5] = (u8)(r >> 16);
}

/* ------------------------------------------------------------------ */
/* wait queues (only the coexistence code waits on one)                */
/* ------------------------------------------------------------------ */

struct wait_queue_head {
	kmutex_t	wq_mtx;
	kcondvar_t	wq_cv;
};

typedef struct wait_queue_head wait_queue_head_t;

static __always_inline __unused void
init_waitqueue_head(struct wait_queue_head *wq)
{
	mutex_init(&wq->wq_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&wq->wq_cv, "rtw88wq");
}

static __always_inline __unused void
wake_up(struct wait_queue_head *wq)
{
	mutex_enter(&wq->wq_mtx);
	cv_broadcast(&wq->wq_cv);
	mutex_exit(&wq->wq_mtx);
}

#define	wake_up_interruptible(wq)	wake_up(wq)

static __always_inline __unused void
rtw88_waitq_lock(struct wait_queue_head *wq)
{
	mutex_enter(&wq->wq_mtx);
}

static __always_inline __unused void
rtw88_waitq_unlock(struct wait_queue_head *wq)
{
	mutex_exit(&wq->wq_mtx);
}

static __always_inline __unused int
rtw88_waitq_timedwait(struct wait_queue_head *wq, int ms)
{
	return cv_timedwait(&wq->wq_cv, &wq->wq_mtx,
	    MAX(1, (int)mstohz((unsigned int)ms)));
}

/* Returns 1 if the condition became true, 0 on timeout (Linux semantics). */
#define	wait_event_timeout(wq, cond, timeout)				\
({									\
	int __ret = 0;							\
	int __ms = jiffies_to_msecs(timeout);				\
	rtw88_waitq_lock(&(wq));					\
	while (!(cond) && __ret == 0)					\
		__ret = rtw88_waitq_timedwait(&(wq), __ms);		\
	rtw88_waitq_unlock(&(wq));					\
	__ret == 0 ? 1 : 0;						\
})

/* ------------------------------------------------------------------ */
/* misc kernel helpers                                                 */
/* ------------------------------------------------------------------ */

static __always_inline __unused unsigned long
__ffs(unsigned long word)
{
	return (unsigned long)(ffs64((uint64_t)word) - 1);
}

static __always_inline __unused unsigned long
__fls(unsigned long word)
{
	return (unsigned long)(fls64((uint64_t)word) - 1);
}

static __always_inline __unused int
scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return (size_t)n < size ? n : (int)size - 1;
}

static __always_inline __unused char *
strscpy(char *dst, const char *src, size_t len)
{
	(void)strlcpy(dst, src, len);
	return dst;
}

static __always_inline __unused void *
memset_io(void *addr, int val, size_t len)
{
	return memset(addr, val, len);
}

static __always_inline __unused void
memcpy_fromio(void *dst, const volatile void *src, size_t len)
{
	memcpy(dst, (const void *)(uintptr_t)src, len);
}

static __always_inline __unused void
memcpy_toio(volatile void *dst, const void *src, size_t len)
{
	memcpy((void *)(uintptr_t)dst, src, len);
}

#define	likely(x)		__predict_true(x)
#define	unlikely(x)		__predict_false(x)

#define	IRQ_NONE		0
#define	IRQ_HANDLED		1

#define	__stringify(x)		#x

/* Linux mutex API (see the note at the struct mutex definition); the
 * port's NetBSD-shell macros take the same names, so drop them first */
#undef mutex_init
#undef mutex_destroy
#define	mutex_init(m)		mutex_init_rtw(m)
#define	mutex_destroy(m)	mutex_destroy_rtw(m)
#define	mutex_lock(m)		mutex_enter(&(m)->mtx)
#define	mutex_unlock(m)		mutex_exit(&(m)->mtx)
#define	mutex_trylock(m)	mutex_tryenter(&(m)->mtx)
#define	mutex_is_locked(m)	mutex_owned(&(m)->mtx)

#define	do_div(n, base)		({ unsigned long __r = (n) % (base);	\
				   (n) = (n) / (base); __r; })

#endif /* _RTW88_COMPAT_H_ */
