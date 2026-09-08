# Embox port on RK3568

The embox port drives a USB 802.11 dongle (Realtek RTL8188EU / urtwn)
behind the board's USB2 host controller. The USB host stack is CherryUSB
(vendored, see `port/cherryusb/IMPORT-INFO.md`); the embox USB stack is
not used. The network stack is net80211; the interface is presented to
embox through its `net` + `cfg80211` modules.

## Board wiring

| Resource | Value |
|---|---|
| USB host controller | usb2host1, base `0xFD880000`, GIC SPI 133 -> INTID 165 (level) |
| Downstream topology | on-board CH334P hub; dongle on a hub port |
| 5 V rail | GPIO3_A0/A1 (VBUS switch) |
| Clock/power domain | CRU + PMUCRU + usb2phy GRF (see `usb_glue_rk3568.c`) |

## Build

The library builds as an embox external project. From the embox source
tree (with this repo at the path embox expects):

```
./scripts/build-embox.sh     # regenerates conf/, links ext_project,
                             # enables the cherryusb modules, builds
```

The modules are:

- `ext_project.port.cherryusb.cherryusb_host` — CherryUSB core + EHCI +
  embox OSAL + RK3568 glue
- `ext_project.port.cherryusb.net80211_port_cherryusb` — the net80211
  port over CherryUSB (depends on the embox OSAL module
  `ext_project.port.embox.net80211_embox_osal` for the NetBSD shims)
- `ext_project.port.cherryusb.net80211_wlan_cmd_cherryusb` — the `wlan`
  shell command
- plus embox `net.core`, `net.wifi.cfg80211` and the skbuff options
  sized for this workload

## Porting notes (embox-specific)

- **Embox semaphores count held tokens**, not free ones: `enter` blocks
  while `value == max_value`, so a counting semaphore must be seeded to
  `max_value - initial`. See `osal_usb_embox.c`.
- **Interrupt discipline**: embox runs IRQ handlers holding the scheduler
  lock, so the EHCI IRQ handler only launches a bottom-half lthread; all
  CherryUSB processing and completions run in that soft context. Never
  touch a semaphore from the hard IRQ.
- **Device memory**: the MMIO regions the glue touches (CRU, PMUCRU, PMU,
  usb2phy GRF, VBUS GPIO, the EHCI) must be registered with
  `PERIPH_MEMORY_DEFINE` or the first access faults.
- **Cache**: with `CONFIG_USB_DCACHE_ENABLE` the stack maintains the DMA
  buffers; the glue maps flush to clean+invalidate (CIVAC) and invalidate
  to invalidate-only (IVAC).
- Register accesses and the RX pump are synchronous control transfers and
  asynchronous bulk URBs respectively; completions are marshalled to the
  driver on a per-device worker thread.

See `port/port.h` for the port interface contract and `driver/README.md`
for adding chip drivers.
