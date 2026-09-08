<!--
  @file
  @brief Provenance of the vendored CherryUSB host stack.
-->

# CherryUSB vendored copy

- **Upstream**: https://github.com/cherry-embedded/CherryUSB (lab reference
  checkout: `~/workspace/reference/cherryusb-upstream`, fork KevinACoder/CherryUSB)
- **Imported commit**: `1fd876d0986bb62f39abb0b4d2f8d6c0b884e5f6`
  (VERSION `1.6.1`, 2026-09-02, "fix usb_osal_freertos: sem_take/timer_stop in isr")
- **Imported verbatim** (host-mode source subset):
  - `core/usbh_core.c`, `core/usbh_core.h` (device-side usbd/usbotg cores excluded)
  - `common/` (headers)
  - `class/hub/` (other classes excluded)
  - `port/ehci/` (whole directory; Mybuild compiles only `usb_hc_ehci.c`,
    the vendor glue files stay as reference and are not linked)
  - `osal/` (reference OSALs, not compiled - the embox OSAL lives in this
    directory as `osal_usb_embox.c`)
  - `LICENSE` (Apache-2.0), `VERSION`, `README.md`, `cherryusb_config_template.h`
- **License**: Apache-2.0 (see `cherryusb/LICENSE`). This vendored tree lives
  in the net_80211 lab library and is not part of the embox upstream tree.

## Local patches (keep to the minimum, list every hunk here)

Applied on top of the verbatim import, all in service of the embox port
(see the port layer one directory up):

1. `port/ehci/usb_hc_ehci.c` `ehci_urb_waitup()`: force a dcache invalidate
   of the URB transfer buffer on completion when
   `CONFIG_USB_DCACHE_ENABLE` is set. The control-transfer direction lives
   in `setup[0]`, not in the endpoint descriptor, so it cannot be filtered
   by EP direction. Board-proven on RK3568; without it IN data stays
   stale in cache.
2. `port/ehci/usb_hc_ehci.c` `usb_hc_init()`: call a new weak
   `usb_hc_ehci_post_init(bus)` at the end of host-controller init (after
   RUN/CONFIGFLAG/port power). The RK3568 glue overrides it to handle
   devices that were already plugged at reset (no connect-change edge is
   generated for them; the glue forces a port power toggle and seeds the
   roothub change bitmap).
3. `core/usbh_core.h` + `core/usbh_core.c`: the class-info collector uses
   a leading-dot section `.usbh_class_info` on GNUC. Embox links with
   `--gc-sections` and a generated linker script without a KEEP for that
   section, so the section is renamed to the C-identifier
   `usbh_class_info` and the GNUC branch reads the auto-generated
   `__start_usbh_class_info` / `__stop_usbh_class_info` symbols instead.
   The variables also carry `used, retain` so garbage collection keeps them.
