..
   *******************************************************************************
   Copyright (c) 2026 An Dao

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

.. _portenta_h7_UsbSystem:

UsbSystem
=========

Overview
--------

``UsbSystem`` is the lifecycle component that services the USB-CDC console on
the Portenta H7's USB-C connector. It is only compiled and registered when the
board is configured with ``PORTENTA_H7_CONSOLE=USB_CDC`` (the default).

The actual USB device driver lives in the platform module
``platforms/stm32/bsp/bspUsbCdc`` (class ``bsp::UsbCdc``), which wraps a
vendored subset of `TinyUSB <https://github.com/hathach/tinyusb>`_
(``libs/3rdparty/tinyusb``). The hardware configuration is the one used by the
Arduino Portenta H7: **USB1 OTG_HS with its internal full-speed PHY** on
PB14 (D-) / PB15 (D+), alternate function 12, clocked from **HSI48 with CRS**
(clock recovery trimmed against USB SOF packets).

Lifecycle
---------

- ``init()``: nothing to do, transition completes immediately.
- ``run()`` (lifecycle level 1, before the console-facing systems):

  - calls ``bsp::UsbCdc::init()``, which enables the USB 48 MHz kernel clock
    (HSI48 + CRS), the USB 3.3 V level detector, the USB1 OTG_HS peripheral
    clock and the PB14/PB15 pins, and initializes the TinyUSB device stack.
    Every step is fail-safe: if USB bring-up fails, the driver stays inactive
    and the rest of the system boots normally (console output is dropped).
  - configures the ``OTG_HS`` interrupt (NVIC priority 6, compatible with
    ``configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY``),
  - schedules a 1 ms periodic runnable on the ``TASK_BSP`` context that calls
    the TinyUSB device task (``bsp::UsbCdc::poll()``). The interrupt handler
    only queues events; all USB protocol processing happens in this task-level
    poll.

- ``shutdown()``: cancels the poll timer and disables the ``OTG_HS`` interrupt.

Interrupt wiring
----------------

``main/src/os/isr/isr_usb.cpp`` provides the strong ``OTG_HS_IRQHandler``
definition (overriding the weak default in the startup code) and forwards it
to ``bsp::UsbCdc::interruptHandler()``. As with the CAN ISRs, the ``main``
library is linked with ``--whole-archive`` so the strong definition is not
discarded by the linker.

Host side
---------

The board enumerates as a CDC-ACM device (``/dev/ttyACM0`` on Linux, ``COMx``
on Windows). The descriptors currently use the pid.codes test VID/PID
``1209:0001`` - suitable for lab use only; set a real VID/PID in
``platforms/stm32/bsp/bspUsbCdc/src/usb_descriptors.c`` before distributing
binaries. Output produced before the host attaches (early boot) is dropped by
design; the first-instruction boot banner remains visible on USART1.
