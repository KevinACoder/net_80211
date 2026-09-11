# RK3568 board layer

Self-contained board support for the RK3568 E4AP5G1-ITX FreeRTOS test
system, assembled from the lab's verified FreeRTOS SDK so that the build
here does not depend on the SDK build system.

## Provenance

Everything under `arch/ common/ soc/ drivers/` is copied from the lab
FreeRTOS SDK (commit `2d7dea7a3a4a4a639c5bdbfa1fa311244202e2da`; the
nested standalone SDK tree it references is at
`3d77c214f5d743c5d3fa954028b59d8552c124f1`), path mapping:

| here | from the lab FreeRTOS SDK |
| --- | --- |
| `arch/armv8/aarch64/` | `standalone/arch/armv8/aarch64/` (minus `gdb/`) |
| `arch/armv8/common/` | `standalone/arch/armv8/common/` (minus the GIC driver, see below) |
| `common/` | `standalone/common/` (minus `finterrupt.c`, see below) |
| `soc/rk3568/` | `standalone/soc/rk3568/` |
| `soc/common/` | `standalone/soc/common/` (minus `fearly_uart.c`) |

Drivers re-derived from NetBSD (upstream BSD notices kept in the files):

| here | derived from |
| --- | --- |
| `drivers/com/` | NetBSD `sys/dev/ic/com.c` (rev 1.388), `comreg.h`, `ns16550reg.h` - the polled console subset: divisor math, line/FIFO init for DesignWare APB UARTs, LSR-polled getc/putc; the register index map follows the upstream `com_std_map` layer |
| `drivers/gicv3/` | NetBSD `sys/arch/arm/cortex/gicv3.c` (rev 1.54.12.1), `gicv3_its.c` (rev 1.41) and `gic_reg.h` - distributor/redistributor/CPU-interface init, the LPI prop/pend table programming with its shareability readback, the ITS command queue and direct device table, plus a bare-metal `intr` front end exposing the flat Interrupt* API |

The port layer consuming these is vendored at
`../../third-party/freertos/portable/GCC/platform/` (see its IMPORT-INFO).

New files written for this port:

| file | role |
| --- | --- |
| `sdkconfig.h` | effective standalone-SDK configuration, values taken from the SDK's verified `rk3568_aarch64_el1_itx_shell` / `_wlan` configs |
| `linker.ld` | adapted from the SDK `tools/build/ld/aarch64_ram.ld`; CONFIG_* macros resolved to constants, SDK-only sections dropped, CherrySH `FSymTab`/`VSymTab` and CherryUSB `usbh_class_info` sections added |
| `main.c` | application entry: banner, uptime task, console bring-up |
| `drivers/com/com_console.c` | console attachment over polled UART2 (0xFE660000, 115200, 24 MHz ref clock assumed on from firmware) and the `printf_call` byte hook |
| `common/console.h` | console front end shared by the printk backends and the application |

## Boot chain (as inherited from the SDK, unchanged)

U-Boot `go 0xa000000` lands EL2; `fboot.S` drops to EL1
(`El1Entry`), `fcrt0.S` (`_startup`) clears bss, enables the MMU flat map
(`MmuInit` + `soc/rk3568/fmmu_table.c`), probes UART2 for printf
(`com_console_early_init`), brings the GIC-600 up (`InterruptEarlyInit` ->
`drivers/gicv3/`, GICD 0xFD400000 / GICR 0xFD460000, ITS 0xFD440000,
firmware security state preserved) and calls `main()`.
`vTaskStartScheduler()` -> `xPortStartScheduler()` installs the FreeRTOS
vector table and programs the tick: EL1 virtual timer (CNTV), INTID 27
under OP-TEE (`freertos_configs.c`). Console is polled - no UART IRQ is
used.

## Compile-time selection

- The aarch64 port is compiled with `-DGUEST` (EL1 tasks), matching the
  SDK's EL1 configs (`CONFIG_USE_EL2` unset).
- `sdkconfig.h` mirrors the SDK Kconfig values consumed by the copied
  sources; do not trim entries without grepping the consumers.
- `CONFIG_ENABLE_GIC_ITS` is off for the USB-only build; the PCIe/iwm
  backend (port/bus/pcie) enables it together with `fgic_its_rk.c`.
