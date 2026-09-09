# SDIO bus backend (reserved)

This directory is reserved for an SDIO bus backend: enumerating the
SDIO bus, claiming matched wlan devices and driving their transfers
behind the port interface (`port/port.h`, with SDIO-specific types in
a `port_sd.h` next to the backend, mirroring
`port/bus/usb/port_usb.h`). Nothing is implemented yet; the USB
backend under `port/bus/usb/cherryusb/` is the reference for the role
a bus backend plays.
