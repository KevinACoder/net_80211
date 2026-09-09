# lwIP presentation (reserved)

This directory is reserved for a `net/` presentation layer that maps
the port hooks in `port/port.h` (eapol/data rx, xmit, hwaddr, events)
onto an lwIP netif. It is bus-agnostic: it pairs with whichever
`bus/` backend brings the device up (e.g. `bus/usb/cherryusb/`).
Nothing is implemented yet; the embox presentation under
`port/net/embox/` with the embox OSAL under `port/osal/embox/` is the
reference for what a port must provide.
