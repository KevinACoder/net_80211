# lwIP + CherryUSB port (reserved)

This directory is reserved for a port implementing `port/port.h` over
the CherryUSB host stack and presenting the interface to lwIP as a
netif. Nothing is implemented yet; the embox port under `port/embox/`
plus `port/cherryusb/` is the reference for what a port must provide.
