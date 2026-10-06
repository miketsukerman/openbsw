/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "bsp/usb/UsbCdc.h"

// Strong definition overriding the weak default handler in the startup code.
// USB1 OTG_HS (the Portenta H7's USB-C connector) -> TinyUSB device driver.
extern "C" void OTG_HS_IRQHandler(void) { ::bsp::UsbCdc::interruptHandler(); }
