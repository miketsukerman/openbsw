# STM32H7 Ethernet MAC (Synopsys DWC EQOS) driver in RMII mode, providing the
# lwIP netif driver contract used by lwipSocket / the reference application.

add_library(bspEthernet src/ethernet/EthDriver.cpp)

target_include_directories(bspEthernet PUBLIC include)

target_link_libraries(
    bspEthernet
    PUBLIC etl lwipcore lwipSocket cpp2ethernet
    PRIVATE bspInterrupts bspMcu)
