# Portable Modbus PoC source list for an existing STM32Cube/CMake project.
#
# Usage after the firmware target exists:
#
#   include(${CMAKE_CURRENT_LIST_DIR}/target/poc_sources.cmake)
#   target_sources(your_firmware_target PRIVATE ${POC_SOURCES})
#   target_include_directories(your_firmware_target PRIVATE ${POC_INCLUDE_DIRS})
#
# The generated project keeps its startup file, linker script, CMSIS device
# files, FreeRTOS kernel and LwIP sources. Remove CubeMX HAL-generated
# peripheral initialization from the build/startup path and use the files
# listed below instead. See README.md for the full integration sequence.

set(POC_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(POC_MODBUS_SOURCES
    "${POC_ROOT}/modules/modbus/mb_map.c"
    "${POC_ROOT}/modules/modbus/mb_pdu.c"
    "${POC_ROOT}/modules/modbus/mb_tcp.c"
    "${POC_ROOT}/modules/modbus/mb_server.c"
    "${POC_ROOT}/ports/lwip/mb_transport_lwip.c"
)

set(POC_TARGET_BOARD_SOURCES
    "${POC_ROOT}/target/board/clock.c"
    "${POC_ROOT}/target/board/delay.c"
    "${POC_ROOT}/target/board/gpio.c"
    "${POC_ROOT}/target/board/mpu.c"
)

set(POC_TARGET_ETH_SOURCES
    "${POC_ROOT}/target/eth/mdio.c"
    "${POC_ROOT}/target/eth/lan8742.c"
    "${POC_ROOT}/target/eth/eth.c"
    "${POC_ROOT}/target/net/ethernetif.c"
)

set(POC_TARGET_APP_SOURCES
    "${POC_ROOT}/app/app_regs.c"
    "${POC_ROOT}/target/app/main.c"
    "${POC_ROOT}/target/app/freertos_hooks.c"
)

set(POC_SOURCES
    ${POC_MODBUS_SOURCES}
    ${POC_TARGET_BOARD_SOURCES}
    ${POC_TARGET_ETH_SOURCES}
    ${POC_TARGET_APP_SOURCES}
)

set(POC_INCLUDE_DIRS
    "${POC_ROOT}/modules/modbus"
    "${POC_ROOT}/ports/lwip"
    "${POC_ROOT}/app"
    "${POC_ROOT}/target"
    "${POC_ROOT}/target/net"
)

# The existing project should define STM32H743xx (or select the device in
# CMSIS), and provide FreeRTOS/LwIP include directories.
