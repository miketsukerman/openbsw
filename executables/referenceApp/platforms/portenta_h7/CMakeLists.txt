# Arduino Portenta H7 (Cortex-M7 core) platform for referenceApp

add_subdirectory(bspConfiguration)
add_subdirectory(safety)
add_subdirectory(main)

if (BUILD_TARGET_RTOS STREQUAL "FREERTOS")
    add_subdirectory(freeRtosCoreConfiguration)
    add_library(freeRtosPort ALIAS freeRtosCm7SysTickPort)
    add_library(freeRtosPortImpl ALIAS freeRtosCm7SysTick)
elseif (BUILD_TARGET_RTOS STREQUAL "THREADX")
    message(FATAL_ERROR "ThreadX is not yet supported on the Portenta H7 "
                        "(planned follow-up); use BUILD_TARGET_RTOS=FREERTOS")
endif ()
