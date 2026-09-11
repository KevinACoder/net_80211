/*
 * @file
 * @brief Supplicant bring-up and the driver event queue for the
 * FreeRTOS test system.
 *
 * One FreeRTOS task runs wpa_supplicant_init/add_iface/run; driver
 * threads only queue deep-copied events (wpa_send_event), which
 * eloop_run() drains on the supplicant task. Control requests from
 * the shell (connect/status/...) are marshalled onto the eloop task
 * as jobs so the supplicant structures are only ever touched there.
 *
 * @date 11.09.2026
 * @author zhugengyu
 */

#include <includes.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "l2_packet/l2_packet.h"
#include "drivers/driver.h"
#include "wpa_supplicant_i.h"
#include "wpa_supplicant/config.h"
#include "wpa_supplicant/bss.h"

#include "wpa_port_glue.h"
#include "wpa_port_api.h"
#include "wpa_supplicant/wmm_ac.h"

#define WPA_QUEUE_LEN 32
#define WPA_IFNAME "wlan0"
#define WPA_TASK_STACK_WORDS (64 * 1024 / sizeof(StackType_t))
/* above the usbdi workers (priority 4); the scan state machine spins
 * on the worker tasks and would starve a default-priority supplicant */
#define WPA_TASK_PRIORITY 6

static struct wpa_global *wpa_global_h;
static struct wpa_supplicant *wpa_wpa_s;

/* the driver event queue */
static struct wpa_supplicant_event_msg *
    wpa_queue[WPA_QUEUE_LEN];
static int wpa_queue_head;
static int wpa_queue_tail;
static SemaphoreHandle_t wpa_queue_mtx;

/* marshalled control jobs */
struct wpa_job {
	void (*fn)(void *arg);
	void *arg;
	int done;
	int started;
	int abandoned;
};

static SemaphoreHandle_t wpa_job_mtx;
static struct wpa_job *wpa_job_cur;
static volatile int wpa_ready;

static void wpa_mtx_init(SemaphoreHandle_t *m) {
	*m = xSemaphoreCreateMutex();
}

static void wpa_mtx_lock(SemaphoreHandle_t *m) {
	xSemaphoreTake(*m, portMAX_DELAY);
}

static void wpa_mtx_unlock(SemaphoreHandle_t *m) {
	xSemaphoreGive(*m);
}

/* ------------------------------------------------------------------ */
/* driver event queue */

static void wpa_event_copy_free(int event,
    union wpa_event_data *data) {

	if (data == NULL) {
		return;
	}
	if (event == EVENT_EAPOL_RX) {
		os_free((void *) data->eapol_rx.src);
		os_free((void *) data->eapol_rx.data);
	} else if (event == EVENT_DEAUTH) {
		os_free((void *) data->deauth_info.addr);
		os_free((void *) data->deauth_info.ie);
	} else if (event == EVENT_ASSOC) {
		os_free((void *) data->assoc_info.addr);
	}
}

int wpa_send_event(void *ctx, int event, const void *data) {
	struct wpa_supplicant_event_msg *msg;
	union wpa_event_data *copy = NULL;

	msg = os_zalloc(sizeof(*msg));
	if (msg == NULL) {
		return -1;
	}
	msg->ctx = ctx;
	msg->event = event;

	if (data != NULL) {
		copy = os_zalloc(sizeof(*copy));
		if (copy == NULL) {
			os_free(msg);
			return -1;
		}
		os_memcpy(copy, data, sizeof(*copy));

		/* duplicate the pointer payloads the producer does not
		 * own after returning */
		if (event == EVENT_EAPOL_RX) {
			uint8_t *src = os_memdup(copy->eapol_rx.src,
			    ETH_ALEN);
			uint8_t *buf = os_memdup(copy->eapol_rx.data,
			    copy->eapol_rx.data_len);

			if (src == NULL || buf == NULL) {
				os_free(src);
				os_free(buf);
				os_free(copy);
				os_free(msg);
				return -1;
			}
			copy->eapol_rx.src = src;
			copy->eapol_rx.data = buf;
		} else if (event == EVENT_DEAUTH) {
			uint8_t *addr = os_memdup(copy->deauth_info.addr,
			    ETH_ALEN);

			if (addr == NULL) {
				os_free(copy);
				os_free(msg);
				return -1;
			}
			copy->deauth_info.addr = addr;
		} else if (event == EVENT_ASSOC) {
			uint8_t *addr = NULL;

			if (copy->assoc_info.addr != NULL) {
				addr = os_memdup(copy->assoc_info.addr,
				    ETH_ALEN);
				if (addr == NULL) {
					os_free(copy);
					os_free(msg);
					return -1;
				}
			}
			copy->assoc_info.addr = addr;
		} else if (event != EVENT_SCAN_RESULTS) {
			/* events with scalar-only payloads arrive with
			 * data == NULL from the driver */
			os_free(copy);
			copy = NULL;
		}
	}
	msg->data = copy;

	wpa_mtx_lock(&wpa_queue_mtx);
	if ((wpa_queue_tail + 1) % WPA_QUEUE_LEN ==
	    wpa_queue_head) {
		wpa_mtx_unlock(&wpa_queue_mtx);
		wpa_printf(MSG_ERROR,
		    "wpa: event queue full, dropping event %d",
		    event);
		wpa_event_copy_free(event, copy);
		os_free(copy);
		os_free(msg);
		return -1;
	}
	wpa_queue[wpa_queue_tail] = msg;
	wpa_queue_tail =
	    (wpa_queue_tail + 1) % WPA_QUEUE_LEN;
	wpa_mtx_unlock(&wpa_queue_mtx);

	wpa_wake_loop();
	return 0;
}

int wpa_send_dummy_event(void) {
	return wpa_send_event(NULL, -1, NULL);
}

void wpa_process_events(void) {
	unsigned budget = WPA_QUEUE_LEN;

	while (budget--) {
		struct wpa_supplicant_event_msg *msg;

		wpa_mtx_lock(&wpa_queue_mtx);
		if (wpa_queue_head == wpa_queue_tail) {
			wpa_mtx_unlock(&wpa_queue_mtx);
			return;
		}
		msg = wpa_queue[wpa_queue_head];
		wpa_queue_head =
		    (wpa_queue_head + 1) % WPA_QUEUE_LEN;
		wpa_mtx_unlock(&wpa_queue_mtx);

		if (msg->event >= 0 && msg->ctx != NULL) {
			wpa_supplicant_event(msg->ctx, msg->event,
			    msg->data);
		}
		wpa_event_copy_free(msg->event, msg->data);
		os_free(msg->data);
		os_free(msg);
	}
}

/* ------------------------------------------------------------------ */
/* marshalled control jobs */

static void wpa_job_free(struct wpa_job *job) {
	if (job->arg != NULL) {
		free(job->arg);
	}
	free(job);
}

void wpa_process_jobs(void) {
	struct wpa_job *job;
	int abandoned;

	wpa_mtx_lock(&wpa_job_mtx);
	job = wpa_job_cur;
	if (job == NULL) {
		wpa_mtx_unlock(&wpa_job_mtx);
		return;
	}
	job->started = 1;
	wpa_mtx_unlock(&wpa_job_mtx);

	job->fn(job->arg);

	wpa_mtx_lock(&wpa_job_mtx);
	job->done = 1;
	wpa_job_cur = NULL;
	abandoned = job->abandoned;
	wpa_mtx_unlock(&wpa_job_mtx);
	if (abandoned) {
		wpa_job_free(job);
	}
}

static int wpa_job_run(void (*fn)(void *), const void *arg, size_t arg_len,
    unsigned timeout_ms) {
	struct wpa_job *job = calloc(1, sizeof(*job));
	unsigned waited = 0;

	if (job == NULL) {
		return -1;
	}
	job->fn = fn;
	if (arg_len != 0) {
		job->arg = malloc(arg_len);
		if (job->arg == NULL) {
			free(job);
			return -1;
		}
		memcpy(job->arg, arg, arg_len);
	}

	wpa_mtx_lock(&wpa_job_mtx);
	if (wpa_job_cur != NULL) {
		wpa_mtx_unlock(&wpa_job_mtx);
		wpa_job_free(job);
		return -1;
	}
	wpa_job_cur = job;
	wpa_mtx_unlock(&wpa_job_mtx);

	wpa_wake_loop();
	for (;;) {
		wpa_mtx_lock(&wpa_job_mtx);
		if (job->done) {
			wpa_mtx_unlock(&wpa_job_mtx);
			wpa_job_free(job);
			return 0;
		}
		if (waited >= timeout_ms) {
			int started = job->started;

			job->abandoned = 1;
			if (!started) {
				wpa_job_cur = NULL;
			}
			wpa_mtx_unlock(&wpa_job_mtx);
			if (!started) {
				wpa_job_free(job);
			}
			return -1;
		}
		wpa_mtx_unlock(&wpa_job_mtx);
		ksleep(10);
		waited += 10;
	}
}

/* ------------------------------------------------------------------ */
/* control jobs (run on the eloop task) */

struct wpa_connect_req {
	char ssid[33];
	char psk[65];
	unsigned char bssid[6];
	int bssid_set;
};

static void wpa_do_connect(void *arg) {
	struct wpa_connect_req *req = arg;
	struct wpa_ssid *ssid;

	if (wpa_wpa_s == NULL) {
		wpa_printf(MSG_ERROR, "wpa: not started");
		return;
	}

	/* one active network: drop previous selections */
	wpas_request_disconnection(wpa_wpa_s);

	ssid = wpa_supplicant_add_network(wpa_wpa_s);
	if (ssid == NULL) {
		wpa_printf(MSG_ERROR, "wpa: add_network failed");
		return;
	}
	ssid->ssid = (u8 *) os_strdup(req->ssid);
	if (ssid->ssid == NULL) {
		wpa_supplicant_remove_network(wpa_wpa_s, ssid->id);
		return;
	}
	ssid->ssid_len = strlen(req->ssid);
	ssid->scan_ssid = 1;
	if (req->bssid_set) {
		os_memcpy(ssid->bssid, req->bssid, 6);
		ssid->bssid_set = 1;
	}
	ssid->key_mgmt = WPA_KEY_MGMT_PSK;
	ssid->proto = WPA_PROTO_RSN;
	ssid->pairwise_cipher = WPA_CIPHER_CCMP;
	ssid->group_cipher = WPA_CIPHER_CCMP;
	ssid->passphrase = os_strdup(req->psk);
	if (ssid->passphrase == NULL) {
		wpa_printf(MSG_ERROR, "wpa: psk alloc failed");
		wpa_supplicant_remove_network(wpa_wpa_s, ssid->id);
		return;
	}
	wpa_config_update_psk(ssid);
	ssid->disabled = 0;

	wpa_config_update_prio_list(wpa_wpa_s->conf);
	wpa_supplicant_select_network(wpa_wpa_s, ssid);
	wpa_printf(MSG_INFO, "wpa: connecting to \"%s\"",
	    req->ssid);
}

static void wpa_do_disconnect(void *arg) {
	(void) arg;

	if (wpa_wpa_s == NULL) {
		return;
	}
	wpas_request_disconnection(wpa_wpa_s);
	wpa_printf(MSG_INFO, "wpa: disconnected");
}

static void wpa_do_status(void *arg) {
	struct wpa_supplicant *wpa_s = wpa_wpa_s;
	const char *state_name;

	(void) arg;

	if (wpa_s == NULL) {
		printf("wpa: supplicant not started\n");
		return;
	}
	state_name = wpa_supplicant_state_txt(wpa_s->wpa_state);
	printf("wpa: state=%s", state_name);
	if (wpa_s->current_bss != NULL && wpa_s->current_ssid != NULL) {
		printf(" ssid=\"%.*s\" bssid=" MACSTR " freq=%d",
		    wpa_s->current_ssid->ssid_len,
		    (const char *) wpa_s->current_ssid->ssid,
		    MAC2STR(wpa_s->current_bss->bssid),
		    wpa_s->current_bss->freq);
	}
	printf("\n");
}

/* ------------------------------------------------------------------ */
/* public API (any thread) */

int wpa_port_connect(const char *ssid, const char *psk) {
	return wpa_port_connect_bssid(ssid, psk, NULL);
}

int wpa_port_connect_bssid(const char *ssid, const char *psk,
    const unsigned char *bssid) {
	struct wpa_connect_req req;

	if (wpa_wpa_s == NULL || ssid == NULL || psk == NULL ||
	    strlen(ssid) == 0 || strlen(ssid) >= sizeof(req.ssid) ||
	    (strlen(psk) != 0 && strlen(psk) != 64 &&
		strlen(psk) < 8)) {
		return -1;
	}
	memset(&req, 0, sizeof(req));
	os_strlcpy(req.ssid, ssid, sizeof(req.ssid));
	os_strlcpy(req.psk, psk, sizeof(req.psk));
	if (bssid != NULL) {
		os_memcpy(req.bssid, bssid, 6);
		req.bssid_set = 1;
	}

	return wpa_job_run(wpa_do_connect,
	    &req, sizeof(req), 5000);
}

int wpa_port_disconnect(void) {
	return wpa_job_run(wpa_do_disconnect, NULL, 0, 5000);
}

int wpa_port_status(void) {
	return wpa_job_run(wpa_do_status, NULL, 0, 5000);
}


/* ---- hostap references not pulled by the PSK-only build set ---- */

struct l2_packet_data *l2_packet_init_bridge(const char *br_ifname,
    const char *ifname, const u8 *own_addr, unsigned short protocol,
    void (*rx_callback)(void *ctx, const u8 *src_addr, const u8 *buf,
	size_t len),
    void *rx_callback_ctx, int l2_hdr)
{
	(void) br_ifname;
	return l2_packet_init(ifname, own_addr, protocol, rx_callback,
	    rx_callback_ctx, l2_hdr);
}

void wmm_ac_notify_assoc(struct wpa_supplicant *wpa_s, const u8 *ies,
    size_t ies_len, const struct wmm_params *wmm_params)
{
	(void) wpa_s;
	(void) ies;
	(void) ies_len;
	(void) wmm_params;
}

void wmm_ac_notify_disassoc(struct wpa_supplicant *wpa_s)
{
	(void) wpa_s;
}

void wmm_ac_clear_saved_tspecs(struct wpa_supplicant *wpa_s)
{
	(void) wpa_s;
}

int wmm_ac_restore_tspecs(struct wpa_supplicant *wpa_s)
{
	(void) wpa_s;
	return 0;
}

void wmm_ac_save_tspecs(struct wpa_supplicant *wpa_s)
{
	(void) wpa_s;
}

void wmm_ac_rx_action(struct wpa_supplicant *wpa_s, const u8 *da,
    const u8 *sa, const u8 *data, size_t len)
{
	(void) wpa_s;
	(void) da;
	(void) sa;
	(void) data;
	(void) len;
}

int wpa_port_started(void) {
	return wpa_wpa_s != NULL;
}

/* ------------------------------------------------------------------ */
/* supplicant task */

static void wpa_supplicant_task(void *arg) {
	struct wpa_params params;
	struct wpa_interface iface;

	(void) arg;

	memset(&params, 0, sizeof(params));
	params.wpa_debug_level = CONFIG_WPA_SUPP_DEBUG_LEVEL;

	wpa_global_h = wpa_supplicant_init(&params);
	if (wpa_global_h == NULL) {
		wpa_printf(MSG_ERROR, "wpa: init failed");
		return;
	}

	memset(&iface, 0, sizeof(iface));
	iface.ifname = WPA_IFNAME;
	iface.driver = "embox";

	wpa_wpa_s = wpa_supplicant_add_iface(wpa_global_h,
	    &iface, NULL);
	if (wpa_wpa_s == NULL) {
		wpa_printf(MSG_ERROR, "wpa: add_iface failed");
		wpa_supplicant_deinit(wpa_global_h);
		wpa_global_h = NULL;
		return;
	}
	wpa_ready = 1;

	wpa_printf(MSG_INFO, "wpa: supplicant running");
	wpa_supplicant_run(wpa_global_h);

	wpa_supplicant_remove_iface(wpa_global_h, wpa_wpa_s, 0);
	wpa_supplicant_deinit(wpa_global_h);
	wpa_wpa_s = NULL;
	wpa_global_h = NULL;
}

int wpa_port_start(void) {
	BaseType_t ok;

	if (wpa_global_h != NULL) {
		return 0;
	}
	if (wlan_supp_ensure_up() != 0) {
		return -1;
	}
	/* one-time init of the port locks/queues */
	wpa_mtx_init(&wpa_queue_mtx);
	wpa_mtx_init(&wpa_job_mtx);
	wpa_queue_head = 0;
	wpa_queue_tail = 0;
	wpa_ready = 0;

	ok = xTaskCreate(wpa_supplicant_task, "wpa_supp",
	    WPA_TASK_STACK_WORDS, NULL, WPA_TASK_PRIORITY, NULL);
	if (ok != pdPASS) {
		return -1;
	}

	/* poll until add_iface finished (plain flag, no locks) */
	{
		int spin = 0;

		while (!wpa_ready && spin < 3000) {
			ksleep(10);
			spin++;
		}
	}
	if (!wpa_ready) {
		return -1;
	}
	return 0;
}
