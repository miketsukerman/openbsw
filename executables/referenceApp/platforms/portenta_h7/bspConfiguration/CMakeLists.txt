add_library(bspConfiguration src/bsp/stdIo/stdIo.cpp
                             src/bsp/uart/UartConfig.cpp)

target_include_directories(bspConfiguration PUBLIC include)

target_link_libraries(
    bspConfiguration
    PUBLIC bspUart
    PRIVATE bspMcu)

if (PORTENTA_H7_CONSOLE STREQUAL "USB_CDC")
    target_compile_definitions(bspConfiguration PRIVATE CONSOLE_OVER_USB_CDC=1)
    target_link_libraries(bspConfiguration PRIVATE bspUsbCdc)
elseif (NOT PORTENTA_H7_CONSOLE STREQUAL "UART")
    message(FATAL_ERROR "PORTENTA_H7_CONSOLE must be USB_CDC or UART, "
                        "got: ${PORTENTA_H7_CONSOLE}")
endif ()
