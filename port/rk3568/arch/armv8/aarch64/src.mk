
TOOL_PATH = $(subst ",,$(CONFIG_TOOLCHAIN_NAME))

ARCH_CSRCS := fcache.c \
				fexception.c \
				fgeneric_timer.c \
				fmmu.c \
				fmmu_display.c \
				$(TOOL_PATH)/fgcc_debug.c \
				fsmcc.c \
				funwind.c \
				fend_print.c

ARCH_ASRCS += $(TOOL_PATH)/fboot.S \
				$(TOOL_PATH)/fcrt0.S \
				$(TOOL_PATH)/fsmccc_call.S \
				$(TOOL_PATH)/ftlb.S \
				$(TOOL_PATH)/fvectors.S \
				$(TOOL_PATH)/ftrace_uart.S

ifeq ($(CONFIG_ENABLE_GDB_STUB),y)
ARCH_CSRCS += gdb/fgdb_arch.c
ARCH_ASRCS += gdb/fgdb_exception.S
endif