# net_80211

NetBSD net80211 wireless stack and chip drivers as a portable C library,
with per-OS "port" adapters. The protocol stack and the chip drivers are
imported verbatim from NetBSD (see `IMPORT-INFO.md`); all OS dependencies
are funneled through shadow headers in `compat/netbsd/` and through the
stable interface in `port/port.h`.

## Layout

```
net80211/        NetBSD sys/net80211 (verbatim import)
driver/          Chip drivers, one directory per chipset (see driver/README.md)
  urtwn/         Realtek RTL8188EU/8188EUS USB (NetBSD if_urtwn, verbatim import)
firmware/        Per-driver firmware blobs with their licenses (see firmware/README.md)
compat/netbsd/   Shadow headers and minimal shims for the NetBSD kernel API
port/
  port.h         The stable port interface (OSAL, USB bus, net attach, control,
                 and the wlan_chip_driver registry)
  embox/         Embox port: NetBSD kernel-API shims (locks, mbuf, ifnet,
                 firmware lookup), compiled as the net80211_embox_osal module
  cherryusb/     CherryUSB USB host stack (vendored) + embox OSAL + RK3568
                 EHCI glue + the net80211-over-cherryusb port (wlan command)
  lwip_cherryusb/  Reserved for a future port over lwIP + CherryUSB
scripts/         Helper scripts (firmware array generation, build integration)
docs/            Integration notes (see docs/embox-rk3568.md)
```

## Adding an OS port

Implement the surface described in `port/port.h` and `port/PORTING.md`
against your OS, provide the presentation layer your stack expects
(embox uses its netdev + cfg80211 API), and register your chip drivers.

## Adding a chip driver

Create `driver/<name>/`, import the OS-neutral driver sources, and expose
one `wlan_chip_driver` describing the bus match table and the attach
hooks. Register it from the driver's own translation unit; ports pick up
registered drivers without further changes. See `driver/README.md`.

## Licenses

- `net80211/`: BSD-2/3-Clause (Atsushi Onoe, Sam Leffler / Errno
  Consulting, and others) - original headers retained per file.
- `driver/urtwn/`: ISC (Damien Bergamini, Kevin Lo, Stefan Sperling,
  Nathanial Sloss) - original headers retained per file.
- `firmware/`: Realtek proprietary firmware, binary redistribution only;
  the license text ships next to each blob.
- Everything written for this repository: BSD-2-Clause, copyright the
  net_80211 authors.
