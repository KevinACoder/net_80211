# lwIP import

- Source: <https://github.com/lwip-tcpip/lwip>, release **2.2.1**, commit
  `77dcd25a72509eb83f72b033d219b1d40cd8eb95`.
- Imported verbatim: `src/` (whole tree, including `Filelists.mk`),
  `COPYING`, `FILES`.
- Plus the upstream FreeRTOS port, verbatim: `contrib/ports/freertos/sys_arch.c`
  and `contrib/ports/freertos/include/arch/sys_arch.h` (kept under
  `contrib/ports/freertos/` as upstream).
- No modifications. Host integration (lwipopts.h, cc.h hooks, netif glue)
  lives outside this tree: application config under `../../port/rk3568/`,
  the net80211 presentation under `../../port/net/lwip/`.

The upstream `sys_arch.c` compile-time requires (via `#error`):
`configSUPPORT_DYNAMIC_ALLOCATION`, `INCLUDE_vTaskDelay`,
`INCLUDE_vTaskSuspend`, `configUSE_MUTEXES` - all enabled by the vendored
`../freertos/portable/GCC/platform/FreeRTOSConfig.h`.
