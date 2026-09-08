#!/bin/bash
# Build the embox image with the net80211 library linked in.
#
# The library is an external project for the embox build system: this
# script creates the ext_project symlink, enables the modules in the
# generated conf/mods.conf (a generated, gitignored file) and runs the
# build.  Re-run after every `make confload`.

set -e

LIB_DIR="$(cd "$(dirname "$0")/.." && pwd)"
EMBOX_SRC="${EMBOX_SRC:-$LIB_DIR/../src}"
# Cross toolchain prefix; embox build.conf pins the same compiler family.
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-elf-}"

cd "$EMBOX_SRC"

# conf/ mirrors the template; regenerate so it matches this branch
make confload-platform/rockchip/rk3568

# 1. link the library into the build tree
rm -rf ext_project
ln -sfn "$LIB_DIR" ext_project

# 2. enable the modules in the generated mods.conf
python3 - "$LIB_DIR" <<'EOF'
import sys

lib = sys.argv[1]
path = 'conf/mods.conf'
text = open(path).read()

anchor = 'include embox.compat.libc.all'
wlan_lines = (
    '\t/* net80211 wireless library (external project, cherryusb host'
    ' stack on the panel EHCI). */\n'
    '\t@Runlevel(2) include ext_project.port.cherryusb.cherryusb_host\n'
    '\t@Runlevel(3) include ext_project.port.cherryusb.net80211_port_cherryusb\n'
    '\tinclude ext_project.port.cherryusb.net80211_wlan_cmd_cherryusb\n'
    '\t@Runlevel(2) include embox.net.core\n'
    '\t@Runlevel(2) include embox.net.skbuff(amount_skb=128)\n'
    '\t@Runlevel(2) include embox.net.skbuff_data(\n'
    '\t\t\t\tamount_skb_data=128, data_size=1514,\n'
    '\t\t\t\tdata_align=1, data_padto=1, ip_align=false)\n'
    '\t@Runlevel(2) include embox.net.skbuff_extra(\n'
    '\t\t\t\tamount_skb_extra=128, extra_size=10,\n'
    '\t\t\t\textra_align=1, extra_padto=1)\n'
    '\t@Runlevel(2) include embox.net.dev\n'
    '\t@Runlevel(2) include embox.net.net_entry\n'
    '\tinclude embox.net.wifi.cfg80211\n'
    '\tinclude embox.cmd.net.iwlist\n\n'
)

if 'net80211_port_cherryusb' not in text:
    if anchor not in text:
        raise SystemExit('mods.conf: libc anchor not found')
    text = text.replace('\t' + anchor, wlan_lines + '\t' + anchor)
    open(path, 'w').write(text)
    print('mods.conf: net80211 cherryusb modules enabled')
else:
    print('mods.conf: net80211 cherryusb modules already enabled')
EOF

# 3. build
make -j"$(nproc)"
"${CROSS_COMPILE}objcopy" -O binary build/base/bin/embox embox-rk3568.bin
echo "image: $EMBOX_SRC/embox-rk3568.bin"
