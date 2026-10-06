# USB-CDC console driver built on the vendored TinyUSB stack
# (libs/3rdparty/tinyusb). The TinyUSB sources are compiled into this module
# so that tusb_config.h and the device headers come from one place.

set(TINYUSB_DIR ${CMAKE_SOURCE_DIR}/libs/3rdparty/tinyusb/src)

add_library(
    bspUsbCdc
    src/UsbCdc.cpp
    src/usb_descriptors.c
    ${TINYUSB_DIR}/tusb.c
    ${TINYUSB_DIR}/common/tusb_fifo.c
    ${TINYUSB_DIR}/device/usbd.c
    ${TINYUSB_DIR}/device/usbd_control.c
    ${TINYUSB_DIR}/class/cdc/cdc_device.c
    ${TINYUSB_DIR}/portable/synopsys/dwc2/dcd_dwc2.c)

target_include_directories(bspUsbCdc PUBLIC include ${TINYUSB_DIR})

target_compile_definitions(bspUsbCdc PUBLIC CFG_TUSB_MCU=OPT_MCU_STM32H7)
# The vendored TinyUSB STM32 port includes stm32h7xx.h directly (not via
# mcu/mcu.h), so allow the guarded CMSIS core header to be pulled in for
# exactly that translation unit.
set_source_files_properties(
    ${TINYUSB_DIR}/portable/synopsys/dwc2/dcd_dwc2.c
    PROPERTIES COMPILE_DEFINITIONS INCLUDE_CORE_CM7_IN_MCU_H)

# bspMcu must be PUBLIC: tusb_config.h consumers and the TinyUSB STM32 port
# need the device headers (stm32h7xx.h) and device-macro compile definitions.
target_link_libraries(
    bspUsbCdc
    PUBLIC bspMcu etl
    PRIVATE bspClock)
