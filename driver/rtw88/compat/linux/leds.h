/* $NetBSD$ */
/* LED class stub: rtw_led_init() is a no-op unless CONFIG_RTW88_LEDS. */
#ifndef _RTW88_LINUX_LEDS_H_
#define _RTW88_LINUX_LEDS_H_

#include "rtw88_compat.h"

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

#define led_classdev_register(parent, led)	(0)
#define led_classdev_unregister(led)		do { } while (0)

#endif
