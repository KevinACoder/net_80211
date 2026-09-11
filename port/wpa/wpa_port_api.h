/*
 * @file
 * @brief Public control API of the wpa_supplicant port (any thread).
 *
 * @date 11.09.2026
 * @author zhugengyu
 */

#ifndef WPA_PORT_API_H_
#define WPA_PORT_API_H_

/* spawn the supplicant thread; idempotent */
int wpa_port_start(void);
int wpa_port_started(void);

/* WPA2-PSK: the request is marshalled to the supplicant thread and
 * this returns once it was picked up (or timed out). */
int wpa_port_connect(const char *ssid, const char *psk);
/* same, with a locked BSSID (6 bytes); NULL bssid == unconstrained */
int wpa_port_connect_bssid(const char *ssid, const char *psk,
	const unsigned char *bssid);
int wpa_port_disconnect(void);
int wpa_port_status(void);

/* bring the wlan interface up on the calling thread (driver glue) */
int wlan_supp_ensure_up(void);

#endif /* WPA_PORT_API_H_ */
