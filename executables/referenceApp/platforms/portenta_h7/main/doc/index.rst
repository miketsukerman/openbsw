..
   *******************************************************************************
   Copyright (c) 2026 An Dao

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

.. _portenta_h7_main:

main - Arduino Portenta H7
====================

Overview
--------

The ``main`` module contains the platform-specific entry point, startup code, linker
script, and lifecycle systems for the Arduino Portenta H7 board:

.. csv-table::
   :widths: 30, 70
   :width: 100%

   "MCU", "STM32H747XI Cortex-M7 core - double-precision FPU"
   "Core clock", "480 MHz (HSE 25 MHz with PLL1, VOS0 boost enabled)"
   "Flash", "2 MB (application at 0x08040000, after the 256 KB Arduino bootloader)"
   "SRAM", "512 KB AXI SRAM (D1 domain) at 0x24000000"
   "CAN", "FDCAN1 - PH14 (RX) / PH13 (TX), AF9, 500 kbit/s, Hat Carrier transceiver"
   "Debug UART", "USART1 - PA9 TX (AF7), 115200 baud, Hat Carrier header pins 8/10"
   "User LED", "Green RGB LED on PK6 (active-low)"
   "Debug interface", "Hat Carrier JTAG/SWD header; DFU bootloader as fallback"

See :ref:`bspconfig_portenta_h7` for the UART and standard I/O configuration, and the
user documentation for the sub-modules:

.. toctree::
   user/index
