..
   *******************************************************************************
   Copyright (c) 2026 An Dao

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

.. _stm32_overview:

STM32 Boards
============

Overview
--------

- The STM32 platform brings Eclipse OpenBSW to STMicroelectronics Nucleo development
  boards - low-cost, widely available hardware with an on-board ST-LINK
  debugger/programmer, so no external debug probe is needed - and to the
  Arduino Portenta H7 board mounted on the Portenta Hat Carrier.
- Eclipse OpenBSW provides a reference application for these boards which can be built
  out-of-the-box and flashed onto the board, providing an immediate starting point for
  further development and learning.

Supported boards
----------------

.. csv-table:: STM32 boards
   :header: "Board", "MCU", "Core", "Flash / RAM", "CAN peripheral", "Form factor"
   :widths: 18, 14, 16, 14, 14, 12

   "NUCLEO-G474RE", "STM32G474RE", "Cortex-M4F, 170 MHz", "512 KB / 128 KB", "FDCAN", "Nucleo-64"
   "NUCLEO-F413ZH", "STM32F413ZH", "Cortex-M4F, 100 MHz", "1.5 MB / 320 KB", "bxCAN", "Nucleo-144"
   "Arduino Portenta H7", "STM32H747XI", "Cortex-M7F, 480 MHz (CM4 unused)", "2 MB / 512 KB AXI", "FDCAN", "Portenta (on Hat Carrier)"

Scope
-----

The reference application on the STM32 boards provides:

- lifecycle management and the serial console over the ST-LINK virtual COM port
  (Nucleo boards) or the 40-pin HAT header UART (Portenta H7 on the Hat Carrier),
- CAN communication (FDCAN on the STM32G474RE and STM32H747XI, bxCAN on the
  STM32F413ZH),
- DoCAN transport and UDS diagnostics (see :ref:`feature_diagnostics`),
- a choice of RTOS: FreeRTOS (default) or ThreadX, selected via the
  ``BUILD_TARGET_RTOS`` CMake option (the Portenta H7 currently supports
  FreeRTOS only; ThreadX support is a planned follow-up).

The platform options enabled per board are defined in
``executables/referenceApp/platforms/<board_name>/Options.cmake``.

Reference Links
---------------

1. **NUCLEO-G474RE**:
    - `NUCLEO-G474RE Product Page <https://www.st.com/en/evaluation-tools/nucleo-g474re.html>`_
    - `STM32G474RE MCU Page <https://www.st.com/en/microcontrollers-microprocessors/stm32g474re.html>`_

2. **NUCLEO-F413ZH**:
    - `NUCLEO-F413ZH Product Page <https://www.st.com/en/evaluation-tools/nucleo-f413zh.html>`_
    - `STM32F413ZH MCU Page <https://www.st.com/en/microcontrollers-microprocessors/stm32f413zh.html>`_

3. **Arduino Portenta H7**:
    - `Portenta H7 Product Page <https://www.arduino.cc/pro/hardware-product-portenta-h7/>`_
    - `Portenta Hat Carrier Product Page <https://store.arduino.cc/collections/portenta-family/products/portenta-hat-carrier>`_
    - `STM32H747XI MCU Page <https://www.st.com/en/microcontrollers-microprocessors/stm32h747xi.html>`_

4. **Software and Tools**:
    - `STM32CubeProgrammer <https://www.st.com/en/development-tools/stm32cubeprog.html>`_
    - `OpenOCD <https://openocd.org/>`_
    - `dfu-util <https://dfu-util.sourceforge.net/>`_ (Portenta H7 bootloader flashing)

Availability
------------

All boards are active products, available from the usual electronics distributors.

Arduino Portenta H7 hardware setup
----------------------------------

The Portenta H7 port targets the **Cortex-M7 core only** (480 MHz, VOS0 boost);
the Cortex-M4 core is left parked. The board is used mounted on the
**Portenta Hat Carrier**:

- **Console UART**: USART1 (PA9 TX / PA10 RX, 115 200 baud) is routed to the
  carrier's 40-pin HAT header, pin 8 (TX) and pin 10 (RX). Connect a
  3.3 V USB-UART adapter to these pins and a GND pin.
- **CAN**: FDCAN1 (PH13 TX / PH14 RX) is wired to the carrier's onboard
  high-speed CAN FD transceiver; connect the bus to the CANH/CANL screw
  terminals. Bit rate is 500 kbit/s; add 120 Ohm termination if the carrier is
  at the end of the bus.
- **Power**: power the carrier (or the Portenta via USB-C) as described in the
  carrier documentation.

Flashing and debugging
~~~~~~~~~~~~~~~~~~~~~~

The application is linked at ``0x08040000``, after the 256 KB Arduino
bootloader, so the factory bootloader is preserved and both flashing flows work:

- **SWD/JTAG probe (primary)**: connect a probe (J-Link, ST-LINK, ...) to the
  carrier's JTAG/SWD debug header and flash ``app.referenceApp.elf`` with
  OpenOCD or STM32CubeProgrammer. Restrict erases to sectors from
  ``0x08040000`` upward so the bootloader is not erased.
- **Arduino DFU bootloader (no probe needed)**: double-tap the reset button to
  enter the bootloader, then flash the binary with ``dfu-util``::

      arm-none-eabi-objcopy -O binary app.referenceApp.elf app.referenceApp.bin
      dfu-util -a 0 -d 2341:035b -s 0x08040000:leave -D app.referenceApp.bin

Current limitations (first iteration): the Cortex-M4 core, Ethernet, USB,
microSD and the watchdog/safety features are not used yet.

Build environment
-----------------

For instructions on building for this platform see :ref:`learning_setup`
