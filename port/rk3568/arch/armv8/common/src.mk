

ARCH_CSRCS += common/fcpp_setup.c common/fpsci.c common/fpmu_perf.c common/fboot_core.c common/freboot.c

ifeq ($(CONFIG_SOC_NAME),"rk3568")
# rk3568 (GIC-600, OP-TEE 引导): 使用 rk 专用 GIC/ITS 实现, 排除共享版。
else
endif

ifeq ($(CONFIG_ENABLE_GIC_ITS),y)
ifeq ($(CONFIG_SOC_NAME),"rk3568")
else
endif
endif

ifeq ($(CONFIG_SOC_NAME),"pd2008")
ARCH_CSRCS += common/fl3cache.c
endif

ifeq ($(CONFIG_SOC_NAME),"pd1904")
ARCH_CSRCS += common/fl3cache.c
endif

ifeq ($(CONFIG_ENABLE_GDB_STUB),y)
ARCH_CSRCS += common/gdb/fgdb_packet.c \
			  common/gdb/fgdb_thread.c \
			  common/gdb/fgdb_uart.c \
			  common/gdb/fgdb_main.c \
			  common/gdb/fgdb.c
endif
