/*
 * @file
 * @brief wpa command: drive the supplicant from the cherrysh console.
 *
 * The PSK is only ever a command line argument, nothing is stored.
 *
 * @date 11.09.2026
 * @author zhugengyu
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "wpa_port_api.h"

#include "csh.h"
#include "../utility/cherrysh/csh_console.h"

static void usage(void) {
	console_printf(
	    "usage: wpa start | status | connect <ssid> <psk> [bssid] | "
	    "disconnect\r\n");
}

static int cmd_wpa(int argc, char **argv) {
	int ret;

	if (argc < 2) {
		usage();
		return 0;
	}

	if (strcmp(argv[1], "start") == 0) {
		ret = wpa_port_start();
		console_printf("wpa: start %s\r\n",
		    ret == 0 ? "ok" : "failed");
		return 0;
	}
	if (strcmp(argv[1], "status") == 0) {
		ret = wpa_port_status();
		if (ret != 0) {
			console_printf("wpa: supplicant busy\r\n");
		}
		return 0;
	}
	if (strcmp(argv[1], "connect") == 0 && (argc == 4 || argc == 5)) {
		if (!wpa_port_started()) {
			/* interface up happens on this (console) thread:
			 * if_init blocks on the USB workers and must not
			 * run on the supplicant thread */
			wlan_supp_ensure_up();
			wpa_port_start();
		}
		if (argc == 5) {
			unsigned b[6];

			if (sscanf(argv[4],
			    "%x:%x:%x:%x:%x:%x",
			    &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
				unsigned char mac[6];

				for (int i = 0; i < 6; i++) {
					mac[i] = (unsigned char) b[i];
				}
				ret = wpa_port_connect_bssid(argv[2],
				    argv[3], mac);
			} else {
				console_printf("wpa: bad bssid %s\r\n", argv[4]);
				return 1;
			}
		} else {
			ret = wpa_port_connect(argv[2], argv[3]);
		}
		if (ret == 0) {
			console_printf("wpa: connecting to \"%s\"\r\n", argv[2]);
		} else {
			console_printf("wpa: connect failed (%d)\r\n", ret);
		}
		return 0;
	}
	if (strcmp(argv[1], "disconnect") == 0) {
		ret = wpa_port_disconnect();
		console_printf("wpa: disconnect %s\r\n",
		    ret == 0 ? "ok" : "failed/busy");
		return 0;
	}

	usage();
	return 0;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_wpa, wpa, "wpa supplicant control",
	"wpa start/connect <ssid> <psk>/status/disconnect\r\n");
