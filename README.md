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
  embox/         NetBSD kernel-API shims for embox (locks/threads, mbuf, ifnet,
                 firmware lookup)
  cherryusb/     CherryUSB USB host stack (vendored) + embox OSAL + RK3568
                 EHCI glue + the usbd_* shim
  lwip_cherryusb/  Reserved for a future port over lwIP + CherryUSB
scripts/         Helper scripts (firmware array generation)
```

This repository holds the portable library only. Building it for a
given OS is the integrator's job: the embox tree, for example, carries
a third-party/net80211 external-project description that pulls this
repository and compiles the ports above into its build.

## Adding an OS port

Implement the surface described in `port/port.h` against your OS,
provide the presentation layer your stack expects, and register the
chip drivers.

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
