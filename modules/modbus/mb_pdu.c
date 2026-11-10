#include "mb_pdu.h"

#include "mb_map.h"

#include <string.h>

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)(value & 0xFFu);
}

static size_t build_exception(uint8_t *resp,
                              size_t resp_cap,
                              uint8_t function,
                              mb_exception_t exception)
{
    if ((resp == NULL) || (resp_cap < 2u)) {
        return 0u;
    }

    resp[0] = (uint8_t)(function | 0x80u);
    resp[1] = (uint8_t)exception;
    return 2u;
}

static size_t handle_read_registers(mb_map_t *map,
                                    const uint8_t *req,
                                    size_t req_len,
                                    uint8_t *resp,
                                    size_t resp_cap,
                                    bool input_registers)
{
    if (req_len != 5u) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_VALUE);
    }

    const uint16_t address = read_be16(&req[1]);
    const uint16_t quantity = read_be16(&req[3]);

    if ((quantity < 1u) || (quantity > 125u)) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_VALUE);
    }

    const uint32_t limit = input_registers ? MB_INPUT_COUNT : MB_HOLDING_COUNT;
    if (((uint32_t)address + (uint32_t)quantity) > limit) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_ADDRESS);
    }

    const size_t response_len = 2u + ((size_t)quantity * 2u);
    if (response_len > resp_cap) {
        return build_exception(resp, resp_cap, req[0], MB_EX_SERVER_FAILURE);
    }

    resp[0] = req[0];
    resp[1] = (uint8_t)(quantity * 2u);

    for (uint16_t i = 0u; i < quantity; ++i) {
        uint16_t value = 0u;
        const bool ok = input_registers
                            ? mb_map_read_input(map, (uint16_t)(address + i), &value)
                            : mb_map_read_holding(map, (uint16_t)(address + i), &value);
        if (!ok) {
            return build_exception(resp, resp_cap, req[0], MB_EX_SERVER_FAILURE);
        }
        write_be16(&resp[2u + ((size_t)i * 2u)], value);
    }

    return response_len;
}

static size_t handle_write_single_register(mb_map_t *map,
                                           const uint8_t *req,
                                           size_t req_len,
                                           uint8_t *resp,
                                           size_t resp_cap)
{
    if (req_len != 5u) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_VALUE);
    }

    const uint16_t address = read_be16(&req[1]);
    const uint16_t value = read_be16(&req[3]);

    if (resp_cap < 5u) {
        return build_exception(resp, resp_cap, req[0], MB_EX_SERVER_FAILURE);
    }

    if (!mb_map_write_holding(map, address, value)) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_ADDRESS);
    }

    memcpy(resp, req, 5u);
    return 5u;
}

static size_t handle_write_multiple_registers(mb_map_t *map,
                                              const uint8_t *req,
                                              size_t req_len,
                                              uint8_t *resp,
                                              size_t resp_cap)
{
    if (req_len < 6u) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_VALUE);
    }

    const uint16_t address = read_be16(&req[1]);
    const uint16_t quantity = read_be16(&req[3]);
    const uint8_t byte_count = req[5];

    if ((quantity < 1u) || (quantity > 123u)) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_VALUE);
    }

    if ((byte_count != (uint8_t)(quantity * 2u)) ||
        (req_len != (6u + (size_t)byte_count))) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_VALUE);
    }

    if (((uint32_t)address + (uint32_t)quantity) > MB_HOLDING_COUNT) {
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_DATA_ADDRESS);
    }

    if (resp_cap < 5u) {
        return build_exception(resp, resp_cap, req[0], MB_EX_SERVER_FAILURE);
    }

    for (uint16_t i = 0u; i < quantity; ++i) {
        const uint16_t value = read_be16(&req[6u + ((size_t)i * 2u)]);
        if (!mb_map_write_holding(map, (uint16_t)(address + i), value)) {
            return build_exception(resp, resp_cap, req[0], MB_EX_SERVER_FAILURE);
        }
    }

    resp[0] = req[0];
    write_be16(&resp[1], address);
    write_be16(&resp[3], quantity);
    return 5u;
}

size_t mb_pdu_handle(mb_map_t *map,
                     const uint8_t *req,
                     size_t req_len,
                     uint8_t *resp,
                     size_t resp_cap)
{
    if ((map == NULL) || (req == NULL) || (resp == NULL) ||
        (req_len < 1u) || (resp_cap < 2u)) {
        return 0u;
    }

    switch (req[0]) {
    case 0x03u:
        return handle_read_registers(map, req, req_len, resp, resp_cap, false);
    case 0x04u:
        return handle_read_registers(map, req, req_len, resp, resp_cap, true);
    case 0x06u:
        return handle_write_single_register(map, req, req_len, resp, resp_cap);
    case 0x10u:
        return handle_write_multiple_registers(map, req, req_len, resp, resp_cap);
    default:
        return build_exception(resp, resp_cap, req[0], MB_EX_ILLEGAL_FUNCTION);
    }
}
