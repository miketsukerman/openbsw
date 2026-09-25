# *******************************************************************************
# Copyright (c) 2026 An Dao
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

# STM32H747XI, Cortex-M7 core only (CM4 stays parked).
set(STM32_FAMILY "H7")
set(STM32_DEVICE_UPPER "STM32H747xx")
set(STM32_CORE "CM7")
set(CAN_TYPE "FDCAN")
set(STM32_STARTUP_ASM
    "${CMAKE_CURRENT_LIST_DIR}/../bsp/bspMcu/startup/startup_stm32h747xx_cm7.s")
