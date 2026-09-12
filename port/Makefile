# net_80211 FreeRTOS test system (RK3568 ITX)
#
# Builds the freertos + lwip + net_80211 + cherrysh combination into a
# U-Boot-loadable image (tftp 0xa000000 + go 0xa000000; the oslab
# boot_os "freertos" profile expects freertos.bin).
#
#   make            build port/build/net80211.elf / .bin
#   make deploy     copy the binary to $(TFTPDIR)/freertos.bin
#   make clean
#
# Toolchain: aarch64-none-elf- (xpack gcc 13.2.1, AARCH64_CROSS_PATH in
# ~/.bashrc); override with CROSS=aarch64-linux-gnu- if needed.
#
# The imported NetBSD-world translation units (net80211 core, urtwn,
# the BSD shim layer) are force-included port/port_config*.h exactly
# like the embox extbld build; the compat include path is kept away
# from lwIP/CherryUSB/board objects so the NetBSD <sys/*.h> shadows
# never cover the newlib ones.

TOPDIR  := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
REPODIR := $(TOPDIR)/..
FTDIR   := $(REPODIR)/third-party/freertos
LWIPDIR := $(REPODIR)/third-party/lwip
CSHDIR  := $(TOPDIR)/utility/cherrysh
CSHCORE := $(CSHDIR)/cherrysh
BOARD   := $(TOPDIR)/rk3568
BUSUSB  := $(TOPDIR)/bus/usb/cherryusb
CSBUS   := $(BUSUSB)/cherryusb
NETR    := $(REPODIR)
COMPAT  := $(NETR)/compat/netbsd
WPAR    := $(abspath $(TOPDIR)/../../wpa_supplicant-rtos)

BUILD   := $(TOPDIR)/build
PROJECT := net80211
TFTPDIR ?= /mnt/d/tftpboot
IMAGE_BIN := $(TFTPDIR)/freertos.bin

CROSS ?= aarch64-none-elf-
CC      := $(CROSS)gcc
CPP     := $(CROSS)cpp
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

CFLAGS := -O2 -g -march=armv8-a -fno-builtin -fno-common -MMD -MP
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DGUEST -DWLAN_PORT_FREERTOS
CFLAGS += -Wall
# the imported SDK/vendor subtrees are not warning-clean; relax them
CFLAGS += -Wno-unused-function -Wno-unused-variable -Wno-unused-value
CFLAGS += -Wno-unused-but-set-variable -Wno-misleading-indentation
CFLAGS += -Wno-array-parameter -Wno-pointer-sign -Wno-int-conversion
CFLAGS += -Wno-address-of-packed-member -Wno-unused-parameter
CFLAGS += -Wno-sign-compare -Wno-old-style-definition
CFLAGS += -Wno-missing-field-initializers -Wno-format

INCLUDES := -I$(FTDIR)/include
INCLUDES += -I$(FTDIR)/portable/GCC/platform
INCLUDES += -I$(FTDIR)/portable/GCC/platform/aarch64
INCLUDES += -I$(BOARD)
INCLUDES += -I$(BOARD)/arch
INCLUDES += -I$(BOARD)/common
INCLUDES += -I$(TOPDIR)/net/lwip
INCLUDES += -I$(BOARD)/arch/armv8/aarch64
INCLUDES += -I$(BOARD)/arch/armv8/aarch64/gcc
INCLUDES += -I$(BOARD)/arch/armv8/gcc
INCLUDES += -I$(BOARD)/arch/armv8/common
INCLUDES += -I$(BOARD)/soc/rk3568
INCLUDES += -I$(BOARD)/soc/common
INCLUDES += -I$(BOARD)/drivers/com
INCLUDES += -I$(BOARD)/drivers/gicv3
INCLUDES += -I$(BOARD)/drivers/pcie
INCLUDES += -I$(CSHDIR)
INCLUDES += -I$(CSHCORE)
INCLUDES += -I$(BUSUSB)
INCLUDES += -I$(CSBUS)/core
INCLUDES += -I$(CSBUS)/common
INCLUDES += -I$(CSBUS)/class/hub
INCLUDES += -I$(CSBUS)/port/ehci
INCLUDES += -I$(LWIPDIR)/src/include
INCLUDES += -I$(LWIPDIR)/contrib/ports/freertos/include

INC_NET  := -I$(NETR) -I$(TOPDIR)/osal/freertos/compat -idirafter $(COMPAT)
NEWLIB_INC := $(shell $(CC) -print-sysroot)/include
STDARG_INC := $(shell $(CC) -print-file-name=stdarg.h)
CFG_BSD  := -D_KERNEL -DIWM_DEBUG -D_COMPAT_SYS_SYSCTL_H_ -include $(STDARG_INC) -include $(NETR)/port/port_config_bsd.h
CFG_HOST := -D_KERNEL -include $(STDARG_INC) -include $(NETR)/port/port_config.h
# rtw88: the Linux-API compat layer comes first (it provides linux/*.h,
# asm/*.h and net/mac80211.h), then dist/ for the chip's bare-name
# headers. The imported code casts away qualifiers, does void-pointer
# arithmetic and trips the uninitialised-use analysis in a few spots;
# CONFIG_RTW88_DEBUG compiles rtw_dbg() in. The compat shadow is pulled
# in ahead of the NetBSD one, so the rtw88 group does not use INC_NET.
INC_RTW  := -I$(NETR) -I$(TOPDIR)/osal/freertos/compat
INC_RTW  += -I$(NETR)/driver/rtw88 -I$(NETR)/driver/rtw88/compat
INC_RTW  += -I$(NETR)/driver/rtw88/dist -idirafter $(COMPAT)
CFG_RTW  := -D_KERNEL -DCONFIG_RTW88_DEBUG -include $(STDARG_INC)
CFG_RTW  += -include $(NETR)/port/port_config_bsd.h
CFG_RTW  += -Wno-cast-qual -Wno-discarded-qualifiers -Wno-pointer-arith
CFG_RTW  += -Wno-maybe-uninitialized
# wpa_supplicant port: the vanilla tree (path-referenced) plus the
# FreeRTOS glue under port/wpa; CONFIG_OS_EMBOX in the config header
# steers the vanilla includes.h to the plain-libc branch. newlib has
# no net/* headers and the compat shadow drags the BSD malloc macros
# with it, so the core group gets minimal shims instead (shim/ first
# in the search order of the wpa rule only).
INC_WPA  := -I$(WPAR) -I$(WPAR)/src -I$(WPAR)/src/utils -I$(WPAR)/src/drivers
INC_WPA  += -I$(WPAR)/src/l2_packet -I$(WPAR)/wpa_supplicant
INC_WPA  += -I$(TOPDIR)/wpa
CFG_WPA  := -w -include $(TOPDIR)/wpa/wpa_port_config.h

# --- sources ---------------------------------------------------------

# FreeRTOS kernel + port layer
SRCS_C  := tasks.c list.c queue.c timers.c event_groups.c stream_buffer.c
SRCS_C  += croutine.c heap_4.c freertos_configs.c port.c

# board startup asm + arch C
SRCS_S  := portASM.S freertos_vectors.S
SRCS_S  += fboot.S fcrt0.S fvectors.S ftlb.S ftrace_uart.S fsmccc_call.S
SRCS_C  += fcache.c fexception.c fgeneric_timer.c fmmu.c funwind.c
SRCS_C  += fsmcc.c fgcc_debug.c
SRCS_C  += fboot_core.c fpsci.c
SRCS_C  += gicv3.c gicv3_its.c intr.c

# board soc / common / driver
SRCS_C  += fmmu_table.c fcpu_affinity_mask.c fmmu_code_table.c
SRCS_C  += fcpu_info.c fspin.c
SRCS_C  += fprintk.c fprintf.c fassert.c fdebug.c fsleep.c
SRCS_C  += fbitmap.c ftrace_printk.c
SRCS_C  += com.c com_console.c
SRCS_C  += pcie_dw.c

# application + console
SRCS_C  += main.c syscalls.c
SRCS_C  += csh_console.c chry_shell.c chry_readline.c
SRCS_C  += net_cmds.c

# lwIP 2.2.1 (upstream; NO_SYS=0 with the contrib FreeRTOS sys_arch)
LWIPDIRS := $(LWIPDIR)/src/core $(LWIPDIR)/src/core/ipv4 \
	$(LWIPDIR)/src/api $(LWIPDIR)/src/netif \
	$(LWIPDIR)/contrib/ports/freertos
LWIP_C   := $(notdir $(foreach d,$(LWIPDIRS),$(wildcard $(d)/*.c)))
SRCS_C   += $(LWIP_C)

# CherryUSB host (vendored 1.6.1, port subset) + RK3568 EHCI glue
SRCS_C   += usbh_core.c usbh_hub.c usb_hc_ehci.c usb_osal_freertos.c
#SRCS_C   += usbdi_compat.c usbh_urtwn_class.c
#SRCS_C   += net_bridge_freertos.c

# CherryUSB EHCI/USBH host stack; the PCIe DWC core lives in the
# board driver tree (pcie_dw.c)

# net80211 BSD-world: stack, AES-CCMP, urtwn (forced port_config_bsd.h)
SRCS_BSD := ieee80211.c ieee80211_proto.c ieee80211_node.c ieee80211_output.c
SRCS_BSD += ieee80211_input.c ieee80211_amrr.c ieee80211_netbsd.c
SRCS_BSD += ieee80211_crypto.c ieee80211_crypto_none.c ieee80211_crypto_ccmp.c
SRCS_BSD += aes_bear.c aes_ct.c aes_ct_enc.c aes_ct_dec.c aes_ccm.c
SRCS_BSD += aes_ccm_mbuf.c aes_impl_compat.c
SRCS_BSD += urtwn_reg.c iwm_reg.c wlan_cmds.c

# host-world port code (forced port_config.h)
SRCS_HOST := osal_freertos.c port_core.c firmware_embed.c
SRCS_HOST += usb_glue_rk3568_freertos.c net_bridge_freertos.c
SRCS_HOST += usbdi_compat.c usbh_urtwn_class.c usbh_modeswitch.c
SRCS_HOST += pcie_freertos.c
SRCS_HOST += bsd_mbuf.c bsd_ifnet.c lwip_netif.c

# wpa_supplicant: PSK-only core file set from the vanilla tree (same
# list as the embox extbld Mybuild) + the FreeRTOS port glue; the
# driver wrapper and l2_packet live in both worlds and take CFG_BSD
SRCS_WPA := wpa_common.c ieee802_11_common.c hw_features_common.c
SRCS_WPA += driver_common.c drivers.c
SRCS_WPA += wpa.c wpa_ie.c pmksa_cache.c preauth.c
SRCS_WPA += common.c wpabuf.c base64.c bitfield.c wpa_debug.c
SRCS_WPA += crypto_internal.c aes-internal.c aes-internal-dec.c
SRCS_WPA += aes-internal-enc.c aes-wrap.c aes-unwrap.c aes-omac1.c
SRCS_WPA += sha1.c sha1-internal.c sha1-prf.c sha1-pbkdf2.c
SRCS_WPA += md5.c md5-internal.c rc4.c
SRCS_WPA += sha256.c sha256-internal.c sha256-prf.c tls_none.c
SRCS_WPA += wpa_supplicant.c events.c scan.c bss.c config.c config_none.c
SRCS_WPA += notify.c wpas_glue.c bssid_ignore.c eap_register.c
SRCS_WPA += op_classes.c rrm.c robust_av.c
SRCS_WPA += eloop_port.c os_port.c supp_main_freertos.c wpa_cmd.c
SRCS_WPABSD := driver_net80211.c l2_packet_freertos.c

# rtw88 (RTL8821CU): the imported Linux chip logic, compiled against the
# driver's own Linux-API compat layer under driver/rtw88/compat, plus the
# usbdi transport, the chip/net80211 bridge and the registration unit.
# These are BSD-world translation units like the other imported drivers,
# but they get their own object directory: dist/ carries generic header
# names (debug.h, main.h, mac.h) that must not leak into the other TUs.
SRCS_RTW := rtw88_compat.c
SRCS_RTW += main.c util.c tx.c rx.c mac.c phy.c coex.c efuse.c fw.c ps.c sec.c
SRCS_RTW += bf.c regd.c sar.c rtw8821c.c rtw8821c_table.c debug.c
SRCS_RTW += rtw88_usb.c rtw88_chip.c rtw88u_reg.c

OBJS      := $(addprefix $(BUILD)/,$(addsuffix .o,$(basename $(notdir $(SRCS_C) $(SRCS_S)))))
OBJS_BSD  := $(addprefix $(BUILD)/bsd/,$(addsuffix .o,$(basename $(notdir $(SRCS_BSD)))))
OBJS_HOST := $(addprefix $(BUILD)/host/,$(addsuffix .o,$(basename $(notdir $(SRCS_HOST)))))
OBJS_WPA  := $(addprefix $(BUILD)/wpa/,$(addsuffix .o,$(basename $(notdir $(SRCS_WPA)))))
OBJS_WPABSD := $(addprefix $(BUILD)/wpabsd/,$(addsuffix .o,$(basename $(notdir $(SRCS_WPABSD)))))
OBJS_RTW  := $(addprefix $(BUILD)/rtw/,$(addsuffix .o,$(basename $(notdir $(SRCS_RTW)))))

vpath %.c $(FTDIR) $(FTDIR)/portable/MemMang $(FTDIR)/portable
vpath %.c $(FTDIR)/portable/GCC/platform/aarch64
vpath %.c $(BOARD) $(BOARD)/common $(BOARD)/arch/armv8/aarch64
vpath %.c $(BOARD)/arch/armv8/aarch64/gcc $(BOARD)/arch/armv8/gcc
vpath %.c $(BOARD)/arch/armv8/common $(BOARD)/soc/rk3568 $(BOARD)/soc/common
vpath %.c $(BOARD)/drivers/com $(BOARD)/drivers/gicv3 $(BOARD)/drivers/pcie $(TOPDIR)
vpath %.c $(NETR)/driver/iwm $(NETR)/port/bus/pcie/freertos
vpath %.c $(CSHDIR) $(CSHCORE) $(CSHCORE)/cherryrl
vpath %.c $(LWIPDIRS)
vpath %.c $(BUSUSB) $(CSBUS)/core $(CSBUS)/common $(CSBUS)/class/hub $(CSBUS)/port/ehci $(CSBUS)/osal
vpath %.c $(TOPDIR)/net/lwip

vpath %.c $(NETR)/net80211 $(NETR)/crypto/aes $(NETR)/driver/urtwn
vpath %.c $(NETR)/port/osal/freertos $(NETR)/port/osal/embox
vpath %.c $(NETR)/port/net/embox $(NETR)/port/net/lwip

# wpa_supplicant/ first: src/utils also has a config.c and the
# wpa_supplicant one is the file this build needs
vpath %.c $(WPAR)/wpa_supplicant
vpath %.c $(WPAR)/src/common $(WPAR)/src/drivers $(WPAR)/src/rsn_supp
vpath %.c $(WPAR)/src/utils $(WPAR)/src/crypto
vpath %.c $(TOPDIR)/wpa

vpath %.S $(FTDIR)/portable/GCC/platform/aarch64
vpath %.S $(BOARD)/arch/armv8/aarch64/gcc

# --- rules -----------------------------------------------------------

.PHONY: all clean deploy
all: $(BUILD)/$(PROJECT).bin

$(BUILD) $(BUILD)/bsd $(BUILD)/host $(BUILD)/wpa $(BUILD)/wpabsd $(BUILD)/rtw:
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/bsd/%.o: %.c | $(BUILD)/bsd
	$(CC) $(CFLAGS) $(INCLUDES) $(INC_NET) $(CFG_BSD) -c $< -o $@

$(BUILD)/host/%.o: %.c | $(BUILD)/host
	$(CC) $(CFLAGS) $(INCLUDES) $(INC_NET) $(CFG_HOST) -c $< -o $@

$(BUILD)/wpa/%.o: %.c | $(BUILD)/wpa
	$(CC) $(CFLAGS) $(INCLUDES) -I$(TOPDIR)/wpa/shim $(INC_WPA) $(CFG_WPA) -c $< -o $@

$(BUILD)/wpabsd/%.o: %.c | $(BUILD)/wpabsd
	$(CC) $(CFLAGS) $(INCLUDES) $(INC_NET) $(CFG_BSD) $(INC_WPA) $(CFG_WPA) -c $< -o $@

# the imported rtw88 code includes its chip headers by bare name and the
# Linux API through the compat/ tree; CONFIG_RTW88_DEBUGFS/LEDS stay off.
#
# The rules are spelled out per source instead of going through vpath:
# the same file name spaces hold the board's main.c, so a pattern rule
# would pull in the wrong file for dist/main.c.
RTW_SRCDIRS := $(NETR)/driver/rtw88 $(NETR)/driver/rtw88/compat
RTW_SRCDIRS += $(NETR)/driver/rtw88/dist

define RTW_OBJECT_RULE
$(BUILD)/rtw/$(basename $(1)).o: $(firstword $(foreach d,$(RTW_SRCDIRS),$(wildcard $(d)/$(1)))) | $(BUILD)/rtw
	$$(CC) $$(CFLAGS) $$(INCLUDES) $$(INC_RTW) $$(CFG_RTW) -c $$< -o $$@
endef

$(foreach s,$(SRCS_RTW),$(eval $(call RTW_OBJECT_RULE,$(s))))

$(BUILD)/$(PROJECT).elf: $(OBJS) $(OBJS_BSD) $(OBJS_HOST) $(OBJS_WPA) $(OBJS_WPABSD) $(OBJS_RTW) $(BUILD)/linker.pp.ld
	$(CC) $(CFLAGS) -nostartfiles -Wl,--gc-sections \
	    -Wl,-Map,$(BUILD)/$(PROJECT).map \
	    -T$(BUILD)/linker.pp.ld $(OBJS) $(OBJS_BSD) $(OBJS_HOST) $(OBJS_WPA) $(OBJS_WPABSD) $(OBJS_RTW) -o $@
	$(SIZE) $@

# ld does not preprocess scripts itself; resolve the sdkconfig includes
$(BUILD)/linker.pp.ld: $(BOARD)/linker.ld | $(BUILD)
	$(CPP) -P -x c -I$(BOARD) $< -o $@

$(BUILD)/$(PROJECT).bin: $(BUILD)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

deploy: $(BUILD)/$(PROJECT).bin
	cp $(BUILD)/$(PROJECT).bin $(IMAGE_BIN)
	ls -l $(IMAGE_BIN)

clean:
	rm -rf $(BUILD)

-include $(shell find $(BUILD) -name "*.d" 2>/dev/null)
