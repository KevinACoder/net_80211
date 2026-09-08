/*
 * @file
 * @brief CherryUSB configuration for the embox port (RK3568 EHCI host).
 *
 * Values validated on the RK3568 host (see
 * docs/embox-rk3568.md); cherryusb/cherryusb_config_template.h carries
 * the full macro catalogue.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#ifndef CHERRYUSB_EMBOX_CONFIG_H_
#define CHERRYUSB_EMBOX_CONFIG_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ================ USB common configuration ================ */

/* log threshold: WARNING hides the per-device INFO enumeration chatter
 * and keeps warnings/errors; raise to USB_DBG_INFO when debugging */
#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_WARNING
#endif
#define CONFIG_USB_PRINTF(...) printf(__VA_ARGS__)

/* aarch64 cache line: every DMA buffer must be aligned to this so dcache
 * sync never straddles a line boundary */
#define CONFIG_USB_ALIGN_SIZE 64

/* the EHCI descriptor pools and the urb buffers live in cacheable embox
 * memory (there is no noncacheable section): explicit maintenance is
 * mandatory, otherwise the async list never advances */
#define CONFIG_USB_DCACHE_ENABLE 1
#define CONFIG_USB_EHCI_DESC_DCACHE_ENABLE 1

/* embox libc memcpy beats the byte-twiddling fallback */
#define CONFIG_USB_MEMCPY_DISABLE

/* no noncacheable section on embox: explicit cache maintenance instead */
#define USB_NOCACHE_RAM_SECTION

/* allocation macros (see also common/usb_def.h) */
#define usb_malloc(size)       malloc(size)
#define usb_calloc(num, size)  malloc((num) * (size))
#define usb_free(ptr)          free(ptr)
#define usb_align(align, size) memalign((align), (size))

/* ================ USB host stack configuration ================ */

#define CONFIG_USBHOST_MAX_BUS             1
#define CONFIG_USBHOST_MAX_RHPORTS         8
#define CONFIG_USBHOST_MAX_EXTHUBS         4
#define CONFIG_USBHOST_MAX_EHPORTS         8
#define CONFIG_USBHOST_MAX_INTERFACES      4
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 16
#define CONFIG_USBHOST_MAX_ENDPOINTS       8
#define CONFIG_USBHOST_DEV_NAMELEN         16

/* hub event thread (prio 0 = highest in cherryusb convention) */
#define CONFIG_USBHOST_PSC_PRIO            0
#define CONFIG_USBHOST_PSC_STACKSIZE       8192

#define CONFIG_USBHOST_REQUEST_BUFFER_LEN           2048
#define CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT     500
#define CONFIG_USBHOST_MSC_TIMEOUT                  0xffffffffU

#ifndef CONFIG_USBHOST_MSOS_VENDOR_CODE
#define CONFIG_USBHOST_MSOS_VENDOR_CODE 0x00
#endif

/* ================ EHCI (rk3568 usb2host1) ================ */

/* capability registers sit at the controller base (no vendor offset) */
#define CONFIG_USB_EHCI_HCCR_OFFSET        0
#define CONFIG_USB_EHCI_FRAME_LIST_SIZE    1024
/* one ctrl urb (3 qtds) + rx pump + up to 2 tx pipes in flight */
#define CONFIG_USB_EHCI_QH_NUM             16
#define CONFIG_USB_EHCI_QTD_NUM            (CONFIG_USB_EHCI_QH_NUM * 3)
#define CONFIG_USB_EHCI_ITD_NUM            4
/* standard EHCI 1.0: software sets CONFIGFLAG to take over the root ports */
#define CONFIG_USB_EHCI_CONFIGFLAG         1

#endif /* CHERRYUSB_EMBOX_CONFIG_H_ */
