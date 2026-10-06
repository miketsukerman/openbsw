/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// Implemented per target in clockConfig_*.cpp. Called from startup code
// before main(); must not use heap, RTOS primitives, or BSW services.
void configurePll(void);

// H7 only: enable the 48 MHz USB kernel clock (HSI48 trimmed by the CRS
// from USB SOF) and select it in RCC->D2CCIP2R. Fail-safe: returns false
// on timeout without affecting the core clock setup.
// Returns true (non-zero) on success.
int configureUsbClock(void);

#ifdef __cplusplus
}
#endif
