/* $NetBSD$ */
/*
 * firmware(9) bridge.  request_firmware_nowait() loads the image straight
 * away and hands it to the callback; the chip code only waits on its own
 * completion afterwards, so the Linux "nowait" staging is not needed.
 */
#ifndef _RTW88_LINUX_FIRMWARE_H_
#define _RTW88_LINUX_FIRMWARE_H_

#include "rtw88_compat.h"

struct firmware {
	size_t		size;
	const u8	*data;
	void		*priv;		/* allocation to release */
};

int	rtw88_request_firmware_nowait(const char *name, struct device *dev,
	    struct firmware **fw_out, void (*cont)(const struct firmware *,
	    void *), void *context);
void	rtw88_release_firmware(const struct firmware *fw);

#define	request_firmware_nowait(module, uevent, name, dev, gfp, context, cont) \
	rtw88_request_firmware_nowait((name), (dev), NULL, (cont), (context))
#define	request_firmware(fw, name, dev)					\
	rtw88_request_firmware_nowait((name), (dev), (fw), NULL, NULL)
#define	release_firmware(fw)	rtw88_release_firmware(fw)

#endif
