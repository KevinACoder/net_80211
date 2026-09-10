# CherrySH import

- Source: <https://github.com/cherry-embedded/CherrySH>, master, commit
  `8efe539c6e55b71d2f2cd1116cd00c4d64222a67` (Apache-2.0).
- Imported verbatim under `cherrysh/`: `chry_shell.c chry_shell.h csh.h
  csh_config_template.h LICENSE README.md` and the `cherryrl/` readline
  sub-library.
- Dropped: `cherryrl/example.c`, `cherryrl/Makefile` (sample build files),
  upstream `builtin/` and `samples/` (console commands are provided by the
  port under this directory instead; `builtin/lsusb` may be revisited later
  against the vendored CherryUSB).
- No code modifications.
- Host integration: `csh_console.c` (I/O hooks + FreeRTOS REPL task),
  `csh_config.h` (configuration, adapted from the upstream stm32 FreeRTOS
  sample), commands in `wlan_cmds.c` exported via `CSH_SCMD_EXPORT*`
  (linker section `FSymTab`; the board linker script must
  `KEEP(*(FSymTab))` between `__fsymtab_start/__fsymtab_end`, likewise
  `VSymTab`).
