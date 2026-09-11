# PCIe bus backend

Backends for the PCIe bus role: enumerating the PCIe hierarchy,
claiming matched wlan devices and driving their transfers behind the
port interface (`port/port.h`, with PCIe-specific types in
`port_pcie.h`, mirroring `port/bus/usb/port_usb.h`). The USB backend
under `port/bus/usb/cherryusb/` is the reference for the role a bus
backend plays.

## Hardware core

`../../rk3568/drivers/pcie/pcie_dw.c` is the controller core, derived
from the lab's NetBSD `rkdwpcie` driver (`sys/arch/arm/rockchip/
rk_pcie.c`): it keeps the firmware-trained link untouched, programs the
outbound MEM window and the per-bus config viewport, guards the
single-device link against phantom slot responses, and owns the client
APB INTx plumbing.

## Backends

- `embox/pcie_embox.c` - placeholder for the embox presentation lane
  (embox drives iwm through its own driver framework).
- `freertos/pcie_freertos.c` - the backend of the FreeRTOS test
  system on this board: claims the 7260 behind the pcie3x2 root
  port over the pcie_dw core, drives the endpoint through legacy
  INTx (client APB 0xfe280000 unmask -> GIC INTID 194) and exposes
  bus_dma/bus_space over the flat device map.
