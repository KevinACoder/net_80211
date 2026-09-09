# Chip drivers

One directory per chipset. A driver consists of the imported OS-neutral
sources plus a small registration translation unit that exposes a
`wlan_chip_driver` (see `port/port.h`):

- `name`: short chip name used for the interface rename and logs
- `bus` + match table: which devices the driver claims (USB: vid/pid)
- `firmware`: name of the blob under `firmware/<driver>/`
- `attach`/`detach`/`stop`: lifecycle hooks; the driver attaches to the
  net80211 stack (`ieee80211_ifattach`) from within `attach`
- `ioctl`-free scan/connect paths run through net80211

The driver talks to the bus only through the port interface
(`port/port.h`, with the USB types from `port/bus/usb/port_usb.h`),
never directly to an OS USB API.
