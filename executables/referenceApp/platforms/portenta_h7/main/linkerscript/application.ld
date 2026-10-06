/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/* Linker script for the Arduino Portenta H7 (STM32H747XI, Cortex-M7 core)
 *
 * Flash bank 1: 1 MB at 0x08000000, of which the first 256 KB
 * (0x08000000-0x0803FFFF) are occupied by the Arduino bootloader. The
 * application is linked at 0x08040000 so it can be flashed both via the
 * Arduino DFU bootloader (dfu-util) and via SWD without erasing the
 * bootloader. Flash bank 2 (1 MB at 0x08100000) extends contiguously.
 *
 * RAM: 512 KB AXI SRAM (D1 domain) at 0x24000000. The startup code and the
 * FreeRTOS SysTick port run entirely from this region. D2 SRAM3 (32 KB at
 * 0x30040000) holds the Ethernet DMA descriptors and frame buffers; it is
 * configured non-cacheable via the MPU by the Ethernet driver.
 */

ENTRY(Reset_Handler)

_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08040000, LENGTH = 1792K
    /* Top 1 KB (0x2407FC00-0x24080000) is reserved for the hard fault
     * handler dump region and survives reset (never initialized here). */
    RAM   (rwx) : ORIGIN = 0x24000000, LENGTH = 511K
    /* D2 SRAM3: Ethernet DMA descriptors + buffers (non-cacheable region) */
    SRAM3 (rw)  : ORIGIN = 0x30040000, LENGTH = 32K
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        __end__ = .;
        _end = .;
        PROVIDE(end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > RAM

    /* Ethernet DMA descriptors and frame buffers. NOLOAD: the driver fully
     * initializes this memory before starting the DMA. */
    .eth_sram (NOLOAD) :
    {
        . = ALIGN(32);
        *(.eth_sram)
        *(.eth_sram*)
    } > SRAM3

    _estack = ORIGIN(RAM) + LENGTH(RAM);

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
