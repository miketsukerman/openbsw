..
   *******************************************************************************
   Copyright (c) 2026 An Dao

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

.. _bspconfig_portenta_h7:

bspConfiguration - Arduino Portenta H7
================================

Overview
--------

The ``bspConfiguration`` module contains hardware-specific configuration for the
Arduino Portenta H7 board (on the Portenta Hat Carrier). Driver logic is separated from its configuration
so that users can customise a project for different boards or pin assignments
without modifying driver source code.

The Portenta H7 platform currently exposes a minimal set of BSP peripherals:

- **bspUart** - USART1 configuration (Hat Carrier 40-pin header on PA9/PA10).
- **bspStdIo** - Standard I/O bridge that routes ``putByteToStdout`` /
  ``getByteFromStdin`` through the configured UART instance.

.. note::

   The ADC, PWM, and digital I/O configuration modules demonstrated by the
   S32K148EVB reference application are not yet ported to this platform.
   They are demonstration features independent of CAN and should be added
   following the same configuration pattern in a future change.

Hardware Summary
++++++++++++++++

.. csv-table::
   :widths: 30, 70
   :width: 100%

   "MCU", "STM32H747XI Cortex-M7 core (480 MHz, double-precision FPU)"
   "UART peripheral", "USART1 - routed to the Hat Carrier 40-pin header (pins 8/10)"
   "UART TX pin", "PA9 (AF7)"
   "UART RX pin", "PA10 (AF7)"
   "UART baud rate", "115 200 baud (BRR = 1042 at 120 MHz APB2)"
   "CAN peripheral", "FDCAN1 (configured in CanSystem, not bspConfiguration)"

.. toctree::
   :hidden:

   user/index
