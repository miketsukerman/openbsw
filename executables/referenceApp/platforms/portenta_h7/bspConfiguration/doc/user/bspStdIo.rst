..
   *******************************************************************************
   Copyright (c) 2026 An Dao

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

.. _bspConfig_StdIo_PortentaH7:

bspStdIo
========

Overview
--------

The standard I/O bridge implements the ``putByteToStdout`` and
``getByteFromStdin`` C-linkage functions required by the OpenBSW logging
framework. The backing device is selected at build time with the
``PORTENTA_H7_CONSOLE`` CMake option:

- ``USB_CDC`` (default): bytes are routed through ``bsp::UsbCdc``, the
  TinyUSB-based CDC-ACM (virtual COM port) driver on the USB-C connector.
  ``putByteToStdout`` is non-blocking - bytes are dropped when no host
  terminal is connected or when the TX FIFO is full, so logging can never
  stall the system. ``getByteFromStdin`` returns ``-1`` when no byte is
  available.
- ``UART``: character I/O is routed through the UART ``TERMINAL`` instance
  configured in :ref:`bspConfig_Uart_PortentaH7`.
  ``putByteToStdout(uint8_t byte)`` transmits one byte over USART1 TX (PA9),
  blocking by polling until the UART TX register is free.
  ``getByteFromStdin()`` attempts to read one byte from USART1 RX (PA10) and
  is non-blocking: if ``Uart::read()`` reports that no byte was received, it
  returns ``-1`` (POSIX ``getchar()`` convention); otherwise it returns the
  byte value (0-255). The UART instance is resolved once into a ``static``
  local reference, avoiding a global constructor ordering dependency.
