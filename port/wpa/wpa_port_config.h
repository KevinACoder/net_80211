/*
 * @file
 * @brief Build-time configuration for the FreeRTOS wpa port.
 *
 * Forced-include on every translation unit of the port (-include).
 * It must be usable before includes.h: CONFIG_OS_EMBOX steers the
 * NXP/Zephyr conditionals there to the plain libc paths (FreeRTOS +
 * newlib wants the same), and the hostapd/EAP/WPS/P2P features this
 * port does not build stay off.
 *
 * @date 11.09.2026
 * @author zhugengyu
 */

#ifndef WPA_PORT_CONFIG_H_
#define WPA_PORT_CONFIG_H_

/* steers includes.h / build_config.h away from the NXP SDK headers */
#define CONFIG_OS_EMBOX 1

#define CONFIG_WPA_SUPP 1
#define CONFIG_HOSTAPD 0
#define CONFIG_IPV6 0

/* internal crypto only: PSK/WPA2-CCMP needs no TLS engine */
#define CONFIG_WPA_SUPP_CRYPTO 0

#define CONFIG_WPA_SUPP_DEBUG_LEVEL 3 /* MSG_INFO */

/* PSK-only: no supplicant-controlled file backend, no blobs, no
 * ctrl-iface, no SME (the net80211 layer owns MLME), no MBO/WNM. */
#define CONFIG_NO_CONFIG_WRITE 1
#define CONFIG_NO_CONFIG_BLOBS 1
#define CONFIG_NO_RANDOM_POOL 1
#define CONFIG_BACKEND_NONE 1

/* our driver in src/drivers/drivers.c */
#define CONFIG_DRIVER_EMBOX 1

/* the SDK headers used to provide this on the NXP builds */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#endif

/* openssl-style entropy unavailable; nonces come from os_get_random */
#define CONFIG_CRYPTO_INTERNAL 1
#define CONFIG_INTERNAL_AES 1
#define CONFIG_INTERNAL_SHA1 1
#define CONFIG_INTERNAL_MD5 1
#define CONFIG_INTERNAL_SHA256 1
#define CONFIG_INTERNAL_RC4 1

#endif /* WPA_PORT_CONFIG_H_ */
