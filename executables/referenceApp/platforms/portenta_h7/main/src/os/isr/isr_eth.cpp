/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

extern "C"
{
extern void ethIsr();

// Strong definition overriding the weak default handler in the startup code.
// STM32H747 Ethernet MAC global interrupt -> EthDriver RX processing.
void ETH_IRQHandler(void) { ethIsr(); }

} /* extern "C" */
