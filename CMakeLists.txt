cmake_minimum_required(VERSION 3.16)

project(modbus_tcp_poc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(POC_WARNINGS -Wall -Wextra -Wpedantic)

# Portable Modbus core: no HAL, LwIP, FreeRTOS or POSIX headers here.
add_library(modbus STATIC
    modules/modbus/mb_map.c
    modules/modbus/mb_pdu.c
    modules/modbus/mb_tcp.c
    modules/modbus/mb_server.c
)
target_include_directories(modbus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/modbus
)
target_compile_options(modbus PRIVATE ${POC_WARNINGS})

# Host-only transport. The target uses ports/lwip instead.
add_library(mb_port_posix STATIC
    ports/posix/mb_transport_posix.c
)
target_include_directories(mb_port_posix PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ports/posix
)
target_link_libraries(mb_port_posix PUBLIC modbus)
target_compile_options(mb_port_posix PRIVATE ${POC_WARNINGS})

add_executable(mb_server_posix
    app/app_regs.c
    tests/host/main_posix.c
)
target_include_directories(mb_server_posix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
)
target_link_libraries(mb_server_posix PRIVATE modbus mb_port_posix)
target_compile_options(mb_server_posix PRIVATE ${POC_WARNINGS})

enable_testing()

add_executable(test_mb_pdu tests/test_mb_pdu.c)
target_link_libraries(test_mb_pdu PRIVATE modbus)
target_compile_options(test_mb_pdu PRIVATE ${POC_WARNINGS})
add_test(NAME mb_pdu COMMAND test_mb_pdu)

add_executable(test_mb_tcp tests/test_mb_tcp.c)
target_link_libraries(test_mb_tcp PRIVATE modbus)
target_compile_options(test_mb_tcp PRIVATE ${POC_WARNINGS})
add_test(NAME mb_tcp COMMAND test_mb_tcp)
