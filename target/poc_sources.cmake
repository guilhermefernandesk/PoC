# Modbus TCP PoC source list for an existing STM32Cube/CMake project.
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

set(POC_FREEMODBUS_DIR "${POC_ROOT}/third_party/freemodbus/modbus")

# Vendored FreeModbus stack (BSD-3-Clause, unmodified) + LwIP port.
set(POC_MODBUS_SOURCES
    "${POC_FREEMODBUS_DIR}/mb.c"
    "${POC_FREEMODBUS_DIR}/functions/mbfunccoils.c"
    "${POC_FREEMODBUS_DIR}/functions/mbfuncdiag.c"
    "${POC_FREEMODBUS_DIR}/functions/mbfuncdisc.c"
    "${POC_FREEMODBUS_DIR}/functions/mbfuncfile.c"
    "${POC_FREEMODBUS_DIR}/functions/mbfuncholding.c"
    "${POC_FREEMODBUS_DIR}/functions/mbfuncinput.c"
    "${POC_FREEMODBUS_DIR}/functions/mbfuncother.c"
    "${POC_FREEMODBUS_DIR}/functions/mbutils.c"
    "${POC_FREEMODBUS_DIR}/tcp/mbtcp.c"
    "${POC_ROOT}/ports/lwip/portevent.c"
    "${POC_ROOT}/ports/lwip/porttcp.c"
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
    "${POC_FREEMODBUS_DIR}/include"
    "${POC_FREEMODBUS_DIR}/tcp"
    "${POC_ROOT}/ports/lwip"
    "${POC_ROOT}/app"
    "${POC_ROOT}/target"
    "${POC_ROOT}/target/net"
)

# The existing project should define STM32H743xx (or select the device in
# CMSIS), and provide FreeRTOS/LwIP include directories.
