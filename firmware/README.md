# Firmware blobs

Firmware files are organized per driver, one subdirectory each, and the
license that allows redistribution ships next to every blob. Blobs are
not edited. Ports embed or load them unchanged; use
`scripts/gen_firmware_array.py` to turn a blob into a C array for
environments without a file system.

- `urtwn/rtl8188eufw.bin`: Realtek RTL8188EU firmware (13904 bytes),
  Redistributable under the enclosed Realtek license.
