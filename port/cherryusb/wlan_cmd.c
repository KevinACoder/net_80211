/*
 * @file
 * @brief wlan command: bring the device up, trigger a scan and print
 * the candidates net80211 collected.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/time/ktime.h>

extern void wlan_urtwn_up(void);
extern void wlan_urtwn_dump(void);
extern void wlan_urtwn_scan_dump(void);
extern void wlan_usbdi_trace_reset(void);
extern void wlan_usbdi_trace_set(unsigned level);
extern int aes_ccm_selftest(void);

int main(int argc, char **argv) {
	int wait_s = 10;

	if (argc > 1 && strcmp(argv[1], "status") == 0) {
		wlan_urtwn_dump();
		return 0;
	}
	if (argc > 1 && strcmp(argv[1], "ccmtest") == 0) {
		int ret = aes_ccm_selftest();
		printf("AES-CCM RFC3610: %s\n", ret ? "FAIL" : "PASS");
		return ret != 0;
	}

	if (argc > 1 && strcmp(argv[1], "scan") == 0) {
		if (argc > 2) {
			wait_s = atoi(argv[2]);
		}
		if (wait_s < 1) {
			wait_s = 1;
		}
		wlan_usbdi_trace_reset();
		wlan_urtwn_up();
		printf("scanning for %d s...\n", wait_s);
		ksleep((unsigned) wait_s * 1000);
		printf("scan results:\n");
		wlan_urtwn_scan_dump();
		return 0;
	}
	if (argc > 1 && strcmp(argv[1], "trace") == 0) {
		/* 0 = quiet (default), 1 = async events, 2 = + control xfers */
		wlan_usbdi_trace_set((argc > 2) ? (unsigned) atoi(argv[2]) : 0);
		return 0;
	}

	printf("usage: wlan scan [seconds] | wlan status | wlan ccmtest | wlan trace [0|1|2]\n");
	return 0;
}
