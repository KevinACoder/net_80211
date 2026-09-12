# rtw88 import (RTL8821CU) — what is imported and what is not

## Imported chip logic: `dist/`

- Source: <https://github.com/lwfinger/rtw88.git>, commit
  `a56bcd26e770257612a0803249cbd4095fc6feca` (2026-05-21), the out-of-tree
  tree the Linux lane verified on this board.
- Every file carries `SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause`
  and the Realtek copyright notice; they are used here under the
  BSD-3-Clause option, as FreeBSD does for the same code.
- The files are unmodified.  The closure a USB 8821C needs is the 39
  files in this directory; nothing else was taken.

Not imported, and why:

| file | replaced by |
| --- | --- |
| `mac80211.c`, `usb.c`, `pci.c`, `sdio.c` | the port glue in `driver/rtw88/` (net80211 integration, usbdi transport) |
| `wow.c`, `led.c`, `debugfs` paths | features not built (`CONFIG_RTW88_DEBUGFS`/`_LEDS` off) |
| `compiler.h`, `bitfield.h` | the compatibility layer in `compat/` |

## Glue written for this port

| file | role |
| --- | --- |
| `compat/` | the Linux kernel API shapes the imported code compiles against, on top of `compat/netbsd/` and the port's osal/bus layers |
| `if_rtw88.c` | net80211 driver: state machine hook, transmit encapsulation, receive delivery |
| `rtw88_chip.c` | chip bridge: owns `struct rtw_dev`, drives `rtw_core_*`, demultiplexes RX |
| `rtw88_usb.c` | usbdi transport implementing `struct rtw_hci_ops` |
| `rtw88u_reg.c` | `wlan_chip_driver` registration for `0bda:c820` |

## Firmware

`firmware/rtw88/rtw8821c_fw.bin`, 139472 bytes, sha256
`2ef409bc418549fcf294061dd0cae1fc22fd9da79b60524950b25de18732f3f0`,
version 24.11.0 (H2C v12).  Embedded by the ports as a C array, so the
driver finds it through the same `firmware(9)` shim as the other chips.
