/*
 * linker.ld - RK3568 FreeRTOS test system.
 *
 * Adapted from the standalone SDK tools/build/ld/aarch64_ram.ld: the
 * CONFIG_* macros are resolved to the constants in sdkconfig.h, the
 * SDK-only sections (libbsd/amp/openamp/dma-heap/letter-shell) are
 * dropped, and the CherrySH command/variable tables (FSymTab/VSymTab)
 * plus the CherryUSB host class table (usbh_class_info) are kept.
 */

#include "sdkconfig.h"

OUTPUT_ARCH(aarch64)
ENTRY(_vector_table)

MEMORY
{
    MEMORY_SPACE (rwx) : ORIGIN = 0x0a000000, LENGTH = 0x2000000
}

SECTIONS
{
    .text : {
        PROVIDE(__text_region_start = .);

        . = ALIGN(64);
        KEEP (*(.vectors))
        . = ALIGN(64);
        KEEP (*(.rtos_vectors_*))
        *(.boot)
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
        *(.plt)
        *(.gnu_warning)
        *(.gcc_execpt_table)
        *(.glue_7)
        *(.glue_7t)
    } > MEMORY_SPACE

    .end_text : {
        . = ALIGN(CONFIG_MMU_PAGE_SIZE);
        __text_region_end = .;
        __rodata_region_start = .;
    } > MEMORY_SPACE

    __text_region_size = __text_region_end - __text_region_start;

    .rodata (ALIGN(64)): {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        __rodata_end = .;
    } > MEMORY_SPACE

    .end_rodata : {
        . = ALIGN(CONFIG_MMU_PAGE_SIZE);
        __rodata_region_end = .;
        _image_ram_start = .;
    } > MEMORY_SPACE

    __rodata_region_size = __rodata_region_end - __rodata_region_start;

    .got : {
        *(.got)
    } > MEMORY_SPACE

    .note.gnu.build-id : {
        KEEP (*(.note.gnu.build-id))
    } > MEMORY_SPACE

    .preinit_array : {
        __preinit_array_start = .;
        KEEP (*(SORT(.preinit_array.*)))
        KEEP (*(.preinit_array))
        __preinit_array_end = .;
    } > MEMORY_SPACE

    .init_array : {
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        __init_array_end = .;
    } > MEMORY_SPACE

    .fini_array : {
        __fini_array_start = .;
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array))
        __fini_array_end = .;
    } > MEMORY_SPACE

    /* CherrySH: exported shell commands and environment variables. */
    .fsymtab (ALIGN(4)) : {
        __fsymtab_start = .;
        KEEP (*(FSymTab))
        __fsymtab_end = .;
    } > MEMORY_SPACE

    .vsymtab (ALIGN(4)) : {
        __vsymtab_start = .;
        KEEP (*(VSymTab))
        __vsymtab_end = .;
    } > MEMORY_SPACE

    /* CherryUSB: host class driver table consumed by usbh_find_class_driver(). */
    .cherryusb (ALIGN(4)) : {
        __usbh_class_info_start__ = .;
        KEEP(*(.usbh_class_info))
        __usbh_class_info_end__ = .;
    } > MEMORY_SPACE

    .gcc_except_table : {
        *(.gcc_except_table)
        . = ALIGN(16);
        __rom_end__ = .;
    } > MEMORY_SPACE

    .data : {
        __data_start__ = .;
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        *(.jcr)
        *(.got)
        *(.got.plt)
        . = ALIGN(CONFIG_MMU_PAGE_SIZE);
        __data_end__ = .;
    } > MEMORY_SPACE

    .sbss (NOLOAD) : {
        __sbss_start__ = .;
        *(.sbss)
        *(.sbss.*)
        *(.gnu.linkonce.sb.*)
        __sbss_end__ = .;
    } > MEMORY_SPACE

    .bss (NOLOAD) : {
        . = ALIGN(64);
        __bss_start__ = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(64);
        __bss_end__ = .;
    } > MEMORY_SPACE

    .heap (NOLOAD) : {
        . = ALIGN(64);
        _heap = .;
        HeapBase = .;
        _heap_start = .;
        . = . + CONFIG_HEAP_SIZE * 1M;
        _heap_end = .;
        HeapLimit = .;
    } > MEMORY_SPACE

    .stack (NOLOAD) : {
        __el1_stack_end = .;
        . += CONFIG_STACK_SIZE;
        __el1_stack = .;
        __el2_stack_end = .;
        . += CONFIG_STACK_SIZE;
        __el2_stack = .;
    } > MEMORY_SPACE

    .end_ram : {
        . = ALIGN(CONFIG_MMU_PAGE_SIZE);
        _image_ram_end = .;
    } > MEMORY_SPACE

    _image_ram_size = _image_ram_end - _image_ram_start;

    .end_code : {
        . = ALIGN(CONFIG_MMU_PAGE_SIZE);
        PROVIDE(_code_end = .);
    } > MEMORY_SPACE
}
