/*
 * @file
 * @brief CherrySH wlan commands for the FreeRTOS test system.
 *
 * Everything dispatches through the bus-neutral port core; join sets
 * the desired SSID directly on the net80211 com and restarts the state
 * machine (the vendored stack auto-associates at end of scan in
 * station mode when a desired SSID is set).
 *
 * @date 10.09.2026
 * @author zhugengyu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <sys/mutex.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_proto.h>

#include <port/port.h>

#include "csh.h"
#include "csh_console.h"

/* usb shim trace control (bus/usb/cherryusb/usbdi_compat.c) */
void wlan_usbdi_trace_set(unsigned level);
void wlan_usbdi_trace_reset(void);

static void wlan_help(void) {
	console_printf(
	    "wlan scan [secs] - bring up and scan\r\n"
	    "wlan dump        - print the scan table\r\n"
	    "wlan status      - adapter and net80211 state\r\n"
	    "wlan up          - firmware load + interface init\r\n"
	    "wlan join <ssid> - set the desired SSID and rejoin (open)\r\n"
	    "wlan select <ad> - drive urtwn (usb) or iwm (pcie)\r\n"
	    "wlan trace <lvl> - usb shim log: 0 quiet, 1 async, 2 control\r\n");
}

static int cmd_wlan(int argc, char **argv) {
	int wait_s = 10;

	if (argc < 2) {
		wlan_help();
		return 0;
	}

	if (strcmp(argv[1], "status") == 0) {
		wlan_port_status_dump();
		return 0;
	}
	if (strcmp(argv[1], "select") == 0) {
		if (argc < 3) {
			console_printf( "wlan: usage: wlan select <urtwn|iwm>\r\n");
			return 1;
		}
		if (wlan_port_select(argv[2]) != 0) {
			console_printf( "wlan: no adapter named %s\r\n", argv[2]);
			return 1;
		}
		console_printf( "wlan: driving %s\r\n", argv[2]);
		return 0;
	}
	if (strcmp(argv[1], "dump") == 0) {
		wlan_port_scan_dump();
		return 0;
	}
	if (strcmp(argv[1], "trace") == 0) {
		unsigned lvl = argc > 2 ? (unsigned) atoi(argv[2]) : 1;

		wlan_usbdi_trace_reset();
		wlan_usbdi_trace_set(lvl);
		console_printf( "wlan: usb trace level %u\r\n", lvl);
		return 0;
	}
	if (strcmp(argv[1], "up") == 0) {
		console_printf( "wlan: up %s\r\n",
		    wlan_port_up() == 0 ? "ok" : "failed");
		return 0;
	}
	if (strcmp(argv[1], "scan") == 0) {
		if (argc > 2) {
			wait_s = atoi(argv[2]);
		}
		if (wait_s < 1) {
			wait_s = 1;
		}
		wlan_port_up();
		wlan_port_scan(NULL, 0);
		console_printf( "wlan: scanning for %ds...\r\n", wait_s);
		while (wait_s-- > 0) {
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
		wlan_port_scan_dump();
		return 0;
	}
	if (strcmp(argv[1], "join") == 0) {
		struct ieee80211com *ic = wlan_port_get_ic();
		const char *ssid;
		size_t len;

		if (argc < 3) {
			console_printf( "wlan: usage: wlan join <ssid>\r\n");
			return 1;
		}
		if (ic == NULL) {
			console_printf( "wlan: no adapter attached\r\n");
			return 1;
		}
		if (wlan_port_up() != 0) {
			console_printf( "wlan: bring-up failed\r\n");
			return 1;
		}
		ssid = argv[2];
		len = strlen(ssid);
		if (len > IEEE80211_NWID_LEN) {
			len = IEEE80211_NWID_LEN;
		}

		wlan_port_serializer_lock();
		memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
		memcpy(ic->ic_des_essid, ssid, len);
		ic->ic_des_esslen = (u_int) len;
		wlan_port_serializer_unlock();

		console_printf( "wlan: joining \"%s\"...\r\n", ssid);
		/* S_SCAN -> S_SCAN only probes the current channel (and
		 * cancels a running scan); route through INIT so the
		 * com state machine runs ieee80211_begin_scan */
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		return 0;
	}

	if (strcmp(argv[1], "debug") == 0) {
		struct ieee80211com *ic = wlan_port_get_ic();

		if (ic == NULL) {
			console_printf( "wlan: no adapter attached\r\n");
			return 1;
		}
		/* SCAN|STATE|AUTH|ASSOC tell the join story; OUTPUT|INPUT|
		 * DEBUG|DUMPPKTS print every managed/data frame */
		ic->ic_debug ^= IEEE80211_MSG_SCAN | IEEE80211_MSG_STATE |
		    IEEE80211_MSG_AUTH | IEEE80211_MSG_ASSOC |
		    IEEE80211_MSG_OUTPUT | IEEE80211_MSG_INPUT |
		    IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS;
		console_printf( "wlan: ic_debug=%#x\r\n", ic->ic_debug);
		return 0;
	}

	wlan_help();
	return 0;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_wlan, wlan, "wlan command", "wlan scan/dump/status/up/join <ssid>\r\n");

static int cmd_ccmtest(int argc, char **argv) {
	extern int aes_ccm_selftest(void);
	int ret;

	(void) argc;
	(void) argv;
	ret = aes_ccm_selftest();
	console_printf( "AES-CCM RFC3610: %s\r\n", ret ? "FAIL" : "PASS");

	return ret != 0;
}
CSH_SCMD_EXPORT_ALIAS_FULL(cmd_ccmtest, ccmtest, "AES-CCM selftest", "ccmtest\r\n");
