# FreeRTOS kernel import

Two sources are combined in this tree:

## Kernel core (pristine subset)

- Source: <https://github.com/FreeRTOS/FreeRTOS-Kernel>, FreeRTOS Kernel
  **V11.3.1**, commit `3a22924e0a9ddbbc8b0758881c33b3422a5cc20d`.
- Imported verbatim: `croutine.c event_groups.c list.c queue.c
  stream_buffer.c tasks.c timers.c`, `include/`, `portable/MemMang/heap_4.c`,
  `LICENSE.md`.
- Everything else from the upstream repo (other ports, examples, SPD files)
  is not imported.

## aarch64 port layer (lab-verified)

- Source: the RK3568 lab FreeRTOS SDK, `third-party/freertos/portable/GCC/
  ft_platform/` (Phytium SDK derived, file headers say FreeRTOS V11.1.0),
  taken from lab commit `2d7dea7a3a4a4a639c5bdbfa1fa311244202e2da`.
- Files: `aarch64/{port.c,portASM.S,freertos_vectors.S,portmacro.h}`,
  `FreeRTOSConfig.h`, `../freertos_configs.c`.
- Directory renamed `ft_platform` -> `platform`; contents otherwise
  unmodified at import time.
- The port depends on the standalone-SDK arch layer (ftypes/finterrupt/
  fgic_v3/fgeneric_timer), which is vendored under `../../port/rk3568/`
  (see its README).
- `freertos_configs.c` carries the RK3568 tick workaround: under OP-TEE the
  EL1 virtual timer (CNTV) is the only programmable tick source and its
  interrupt line is PPI11 / INTID 27 (not the architectural PPI14/30).
- Known risk: the port layer was written against V11.1/V11.2; building it
  against the V11.3.1 kernel is expected to surface small drift, to be
  fixed in place (documented below as they land).

| Fix | Reason |
| --- | --- |
| `freertos_configs.c`: rename `finterrupt` to `intr_instance` | the tree does not carry the SDK object of that name (`common/finterrupt.c` is excluded); keep the local controller instance distinct from the file it used to mirror |
