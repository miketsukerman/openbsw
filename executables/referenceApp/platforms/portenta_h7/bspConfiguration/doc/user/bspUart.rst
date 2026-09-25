..
   *******************************************************************************
   Copyright (c) 2026 An Dao

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

.. _bspConfig_Uart_PortentaH7:

bspUart Configuration
=====================

Overview
--------

The UART configuration defines the hardware parameters for the on-board
40-pin HAT header of the Portenta Hat Carrier (pin 8 TX / pin 10 RX,
USART1 on PA9/PA10). A single UART instance
(``TERMINAL``) is configured for debug logging and interactive console use.

``UartConfig.h`` declares the ``Uart::Id`` enumeration of available UART
instances, and ``UartConfig.cpp`` maps each ``Uart::Id`` to its hardware
parameters and provides the singleton accessor
``bsp::Uart::getInstance(Uart::Id)``.

Configuration
-------------

.. csv-table::
   :header: "Parameter", "Value"
   :widths: 30, 70
   :width: 100%

   "Peripheral", "USART1 (APB2)"
   "TX pin", "PA9, alternate function AF7 (HAT header pin 8)"
   "RX pin", "PA10, alternate function AF7 (HAT header pin 10)"
   "Baud rate", "115 200 baud"
   "BRR", "120 000 000 / 115 200 = 1041.67 -> 1042"

The resulting actual baud rate is 120 MHz / 1042 = 115 163 baud (0.032 % error),
well within the UART tolerance.

.. note::

   The UART driver uses polling mode (no DMA, no interrupts). For high-
   throughput or latency-sensitive applications, consider extending the
   ``bspUart`` BSP module with an interrupt-driven or DMA-backed backend.
