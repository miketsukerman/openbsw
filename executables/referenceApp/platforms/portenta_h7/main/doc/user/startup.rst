..
   *******************************************************************************
   Copyright (c) 2026 An Dao

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

.. _portenta_h7_startup:

Startup Code
============

The startup code (``platforms/stm32/bsp/bspMcu/startup/startup_stm32h747xx_cm7.s``) provides
the vector table and the ``Reset_Handler`` for the STM32H747xx Cortex-M7 core.

Vector Table
------------

The vector table is placed at the base of flash (``0x0800_0000``) in the
``.isr_vector`` section. It contains the initial stack pointer, the Cortex-M7 exception
vectors, and the 150 STM32H747xx interrupt vectors. Every handler is a weak alias of
``Default_Handler`` (an infinite loop) and is overridden by a strong definition where
the application implements one (e.g. ``HardFault_Handler``, ``FDCAN1_IT0_IRQHandler``).

Reset Handler
-------------

``Reset_Handler`` executes the following sequence:

1. Load the initial stack pointer (``_estack``).
2. Copy the ``.data`` section from flash to SRAM.
3. Zero-fill the ``.bss`` section.
4. Enable the FPU (CP10/CP11 full access in ``CPACR``, followed by ``dsb``/``isb``).
5. Call ``SystemInit()`` (PLL and early peripheral setup, defined in ``main.cpp``).
6. Call ``__libc_init_array`` (C++ global/static constructors).
7. Call ``main()``.

The startup code does not mask or unmask interrupts; interrupt control is left to the
FreeRTOS port once the scheduler starts.

Memory Layout
-------------

The linker script places code at ``0x0804_0000``, after the 256 KB Arduino DFU
bootloader; ``Reset_Handler`` programs ``SCB->VTOR`` with the vector-table address
accordingly. The STM32H747XI has
512 KB of AXI SRAM at ``0x2400_0000`` (the last KB is reserved for the hard fault dump
32 KB, which is aliased at ``0x2001_8000``), but the linker script defines the RAM
region with ``LENGTH = 127K``: the top 1 KB (``0x2001_FC00``-``0x2002_0000``) is
reserved for the hard fault handler dump region and is left untouched so it survives
reset. Data is placed in the 127 KB RAM region and the stack grows down from
``_estack = 0x2001_FC00``.
