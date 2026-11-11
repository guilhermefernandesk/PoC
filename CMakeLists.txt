cmake_minimum_required(VERSION 3.16)

project(modbus_tcp_poc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(POC_WARNINGS -Wall -Wextra -Wpedantic)

# Vendored FreeModbus stack (BSD-3-Clause, unmodified). port.h comes from
# the port directory, so the core and its port are always built together.
set(FREEMODBUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/freemodbus/modbus)

set(FREEMODBUS_CORE_SOURCES
    ${FREEMODBUS_DIR}/mb.c
    ${FREEMODBUS_DIR}/functions/mbfunccoils.c
    ${FREEMODBUS_DIR}/functions/mbfuncdiag.c
    ${FREEMODBUS_DIR}/functions/mbfuncdisc.c
    ${FREEMODBUS_DIR}/functions/mbfuncfile.c
    ${FREEMODBUS_DIR}/functions/mbfuncholding.c
    ${FREEMODBUS_DIR}/functions/mbfuncinput.c
    ${FREEMODBUS_DIR}/functions/mbfuncother.c
    ${FREEMODBUS_DIR}/functions/mbutils.c
    ${FREEMODBUS_DIR}/tcp/mbtcp.c
)

# Host build: FreeModbus core + POSIX port (sockets). The STM32H7 target
# uses ports/lwip instead (see target/poc_sources.cmake).
add_library(freemodbus_posix STATIC
    ${FREEMODBUS_CORE_SOURCES}
    ports/posix/portevent.c
    ports/posix/porttcp.c
)
target_include_directories(freemodbus_posix PUBLIC
    ${FREEMODBUS_DIR}/include
    ${FREEMODBUS_DIR}/tcp
    ${CMAKE_CURRENT_SOURCE_DIR}/ports/posix
)
# The PoC warning policy applies to PoC-owned files only; the vendored
# core is upstream code kept unmodified.
set_source_files_properties(ports/posix/portevent.c ports/posix/porttcp.c
    PROPERTIES COMPILE_OPTIONS "${POC_WARNINGS}")

add_executable(mb_server_posix
    app/app_regs.c
    tests/host/main_posix.c
)
target_include_directories(mb_server_posix PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(mb_server_posix PRIVATE freemodbus_posix)
target_compile_options(mb_server_posix PRIVATE ${POC_WARNINGS})

enable_testing()

# Register map / FreeModbus callbacks. TCP protocol behaviour is validated
# by tools/modbus_test.py against mb_server_posix (see README.md).
add_executable(test_mb_regs
    tests/test_mb_regs.c
    app/app_regs.c
)
target_include_directories(test_mb_regs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(test_mb_regs PRIVATE freemodbus_posix)
target_compile_options(test_mb_regs PRIVATE ${POC_WARNINGS})
add_test(NAME mb_regs COMMAND test_mb_regs)
