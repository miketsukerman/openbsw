/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * TinyUSB configuration: device-only, single CDC-ACM interface, full speed.
 *
 * The STM32H747's USB1 OTG_HS controller is used with its internal
 * full-speed PHY (the configuration the Arduino Portenta H7 bootloader
 * uses), so the maximum speed is forced to full speed - this makes the
 * DWC2 driver select the internal FS transceiver (GUSBCFG.PHYSEL) instead
 * of the ULPI interface.
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C"
{
#endif

// CFG_TUSB_MCU is provided as a compile definition by the build system
// (OPT_MCU_STM32H7).
#ifndef CFG_TUSB_MCU
#error "CFG_TUSB_MCU must be defined by the build system"
#endif

#define CFG_TUSB_OS OPT_OS_NONE

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

// Device stack only; polled via tud_task() from a low-priority task.
#define CFG_TUD_ENABLED 1
#define CFG_TUH_ENABLED 0

// Internal FS PHY on the OTG_HS controller -> full speed only.
#define CFG_TUD_MAX_SPEED OPT_MODE_FULL_SPEED

// No cache-sensitive DMA: the DWC2 driver runs in slave (FIFO) mode.
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

#define CFG_TUD_ENDPOINT0_SIZE 64

//------------- Class -------------//
#define CFG_TUD_CDC 1
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

// CDC FIFO sizes (must be multiples of the 64-byte FS endpoint size).
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256
#define CFG_TUD_CDC_EP_BUFSIZE 64

#ifdef __cplusplus
}
#endif

#endif // TUSB_CONFIG_H_
