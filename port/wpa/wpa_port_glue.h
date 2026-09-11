/*
 * @file
 * @brief Internal glue between the FreeRTOS wpa port pieces: the
 * eloop event queue lives in supp_main_freertos.c and the eloop only
 * wakes on it.
 *
 * @date 11.09.2026
 * @author zhugengyu
 */

#ifndef WPA_PORT_GLUE_H_
#define WPA_PORT_GLUE_H_

/* one queued driver event; the producer copies the pointer payloads
 * (supp_main_freertos.c), the eloop thread hands it to
 * wpa_supplicant_event() and frees the copy */
struct wpa_supplicant_event_msg {
	void *ctx;
	int event; /* enum wpa_event_type; -1 = wake-up only */
	void *data;
};

/* driver events are queued for the eloop thread */
int wpa_send_event(void *ctx, int event, const void *data);
int wpa_send_dummy_event(void);
void wpa_process_events(void);
void wpa_process_jobs(void);

/* the queue wakeup used by eloop_register_timeout */
void wpa_wake_loop(void);

#endif /* WPA_PORT_GLUE_H_ */
