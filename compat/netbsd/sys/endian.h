/*
 * @file
 * @brief Byte order helpers.
 */

#ifndef _SYS_ENDIAN_H_
#define _SYS_ENDIAN_H_

#include <sys/cdefs.h>
#include <sys/types.h>

static inline uint16_t bswap16_impl(uint16_t v) {
	return __builtin_bswap16(v);
}
static inline uint32_t bswap32_impl(uint32_t v) {
	return __builtin_bswap32(v);
}
static inline uint64_t bswap64_impl(uint64_t v) {
	return __builtin_bswap64(v);
}

#define __bswap16(x) bswap16_impl(x)
#define __bswap32(x) bswap32_impl(x)
#define __bswap64(x) bswap64_impl(x)
#define __byte_swap_word(x) bswap16_impl(x)
#define __byte_swap_long(x) bswap32_impl(x)

#define _BYTE_ORDER _LITTLE_ENDIAN
#define BYTE_ORDER _BYTE_ORDER
#define _LITTLE_ENDIAN 1234
#define _BIG_ENDIAN 4321
#define _PDP_ENDIAN 3412

#define __swap16md(x) bswap16_impl(x)
#define __swap32md(x) bswap32_impl(x)
#define __swap64md(x) bswap64_impl(x)

#define be16toh(x) __bswap16(x)
#define le16toh(x) ((uint16_t)(x))
#define htobe16(x) __bswap16(x)
#define htole16(x) ((uint16_t)(x))

#define be32toh(x) __bswap32(x)
#define le32toh(x) ((uint32_t)(x))
#define htobe32(x) __bswap32(x)
#define htole32(x) ((uint32_t)(x))

#define be64toh(x) __bswap64(x)
#define le64toh(x) ((uint64_t)(x))
#define htobe64(x) __bswap64(x)
#define htole64(x) ((uint64_t)(x))

#define betoh16(x) be16toh(x)
#define letoh16(x) le16toh(x)
#define betoh32(x) be32toh(x)
#define letoh32(x) le32toh(x)
#define betoh64(x) be64toh(x)
#define letoh64(x) le64toh(x)

#define htons(x) htobe16(x)
#define htonl(x) htobe32(x)
#define ntohs(x) be16toh(x)
#define ntohl(x) be32toh(x)

#endif /* _SYS_ENDIAN_H_ */
