..
   *******************************************************************************
   Copyright (c) 2026 An Dao

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0
   *******************************************************************************

.. _portenta_h7_EthernetSystem:

EthernetSystem
==============

Overview
--------

``PortentaEthernetSystem`` is the board-level lifecycle component that brings up
the on-board 10/100 Ethernet of the Portenta H7 and implements
``ethernet::IEthernetDriverSystem`` for the shared application-level
``EthernetSystem`` (lwIP) and ``SomeIpSystem``. Ethernet is only compiled in
when ``PLATFORM_SUPPORT_ETHERNET`` is ``ON`` (the default for this board).

The RJ45 connector is provided by the `Portenta Hat Carrier
<https://store.arduino.cc/collections/portenta-family/products/portenta-hat-carrier>`_;
the MAC, RMII interface and the Microchip **LAN8742AI** PHY are on the Portenta
H7 module itself.

Hardware configuration
----------------------

.. csv-table::
   :widths: 30, 70
   :width: 100%

   "MAC", "STM32H747 Ethernet MAC (Synopsys DWC EQOS), RMII mode"
   "PHY", "LAN8742AI, address 0, MDIO on PA2 / MDC on PC1"
   "RMII pins", "REF_CLK PA1, CRS_DV PA7, RXD0/1 PC4/PC5, TX_EN PG11, TXD0/1 PG13/PG12 (all AF11)"
   "PHY enable", "PJ15 high (PHY power/reset), PH1 high (shared 25 MHz oscillator, also used by the USB ULPI PHY)"
   "Link", "Auto-negotiated 10/100, full/half duplex, polled every 100 ms"
   "Interrupt", "``ETH_IRQHandler`` (RX only), NVIC priority within FreeRTOS limits"

The MAC driver itself is the platform module ``platforms/stm32/bsp/bspEthernet``
(class ``ethernet::EthDriver``).

DMA buffers and cache (important)
---------------------------------

The Cortex-M7 D-cache is enabled on this port. All Ethernet DMA descriptors and
packet buffers are therefore placed in a dedicated **non-cacheable 32 KB region
in D2 SRAM3 at 0x30040000** (linker section ``.eth_sram``), configured via an
MPU region at driver init. This avoids any per-buffer cache clean/invalidate
maintenance.

IP configuration
----------------

The board module ``ethConfiguration`` configures the lwIP network interfaces
(static IP for the PoC):

.. csv-table::
   :widths: 30, 70
   :width: 100%

   "MAC address", "10:11:22:77:77:99 (test address)"
   "eth0 (untagged)", "192.168.0.202 / 255.255.255.0"
   "eth1", "192.168.2.202 / 255.255.255.0"

SOME/IP
-------

With Ethernet enabled, the shared ``EthernetSystem`` (lwIP, runlevel 5) and
``SomeIpSystem`` demo (runlevel 6) from the reference application run
unmodified: service discovery announcements and the demo service are reachable
on the configured IP, interoperable with the POSIX reference application on the
same LAN.

Quick test: connect the Hat Carrier RJ45 to a LAN, then ``ping 192.168.0.202``
and observe SOME/IP-SD multicast traffic in Wireshark.
