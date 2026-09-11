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
CFG_BSD  := -D_KERNEL -D_COMPAT_SYS_SYSCTL_H_ -include $(STDARG_INC) -include $(NETR)/port/port_config_bsd.h
CFG_HOST := -D_KERNEL -include $(STDARG_INC) -include $(NETR)/port/port_config.h

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

# CherryUSB EHCI/USBH host stack

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
SRCS_HOST += usbdi_compat.c usbh_urtwn_class.c
SRCS_HOST += bsd_mbuf.c bsd_ifnet.c lwip_netif.c

OBJS      := $(addprefix $(BUILD)/,$(addsuffix .o,$(basename $(notdir $(SRCS_C) $(SRCS_S)))))
OBJS_BSD  := $(addprefix $(BUILD)/bsd/,$(addsuffix .o,$(basename $(notdir $(SRCS_BSD)))))
OBJS_HOST := $(addprefix $(BUILD)/host/,$(addsuffix .o,$(basename $(notdir $(SRCS_HOST)))))

vpath %.c $(FTDIR) $(FTDIR)/portable/MemMang $(FTDIR)/portable
vpath %.c $(FTDIR)/portable/GCC/platform/aarch64
vpath %.c $(BOARD) $(BOARD)/common $(BOARD)/arch/armv8/aarch64
vpath %.c $(BOARD)/arch/armv8/aarch64/gcc $(BOARD)/arch/armv8/gcc
vpath %.c $(BOARD)/arch/armv8/common $(BOARD)/soc/rk3568 $(BOARD)/soc/common
vpath %.c $(BOARD)/drivers/com $(BOARD)/drivers/gicv3 $(TOPDIR)
vpath %.c $(NETR)/driver/iwm $(NETR)/port/bus/pcie/freertos
vpath %.c $(CSHDIR) $(CSHCORE) $(CSHCORE)/cherryrl
vpath %.c $(LWIPDIRS)
vpath %.c $(BUSUSB) $(CSBUS)/core $(CSBUS)/common $(CSBUS)/class/hub $(CSBUS)/port/ehci $(CSBUS)/osal
vpath %.c $(TOPDIR)/net/lwip

vpath %.c $(NETR)/net80211 $(NETR)/crypto/aes $(NETR)/driver/urtwn
vpath %.c $(NETR)/port/osal/freertos $(NETR)/port/osal/embox
vpath %.c $(NETR)/port/net/embox $(NETR)/port/net/lwip

vpath %.S $(FTDIR)/portable/GCC/platform/aarch64
vpath %.S $(BOARD)/arch/armv8/aarch64/gcc

# --- rules -----------------------------------------------------------

.PHONY: all clean deploy
all: $(BUILD)/$(PROJECT).bin

$(BUILD) $(BUILD)/bsd $(BUILD)/host:
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/bsd/%.o: %.c | $(BUILD)/bsd
	$(CC) $(CFLAGS) $(INCLUDES) $(INC_NET) $(CFG_BSD) -c $< -o $@

$(BUILD)/host/%.o: %.c | $(BUILD)/host
	$(CC) $(CFLAGS) $(INCLUDES) $(INC_NET) $(CFG_HOST) -c $< -o $@

$(BUILD)/$(PROJECT).elf: $(OBJS) $(OBJS_BSD) $(OBJS_HOST) $(BUILD)/linker.pp.ld
	$(CC) $(CFLAGS) -nostartfiles -Wl,--gc-sections \
	    -Wl,-Map,$(BUILD)/$(PROJECT).map \
	    -T$(BUILD)/linker.pp.ld $(OBJS) $(OBJS_BSD) $(OBJS_HOST) -o $@
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
