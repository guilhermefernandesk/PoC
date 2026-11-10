#ifndef MB_TYPES_H
#define MB_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef MB_HOLDING_COUNT
#define MB_HOLDING_COUNT 100u
#endif

#ifndef MB_INPUT_COUNT
#define MB_INPUT_COUNT 100u
#endif

#define MB_MAX_PDU 253u
#define MB_MAX_ADU 260u
#define MB_MBAP_SIZE 7u
#define MB_TCP_PORT_DEFAULT 502u
#define MB_UNIT_ID_DEFAULT 1u

typedef enum {
    MB_OK = 0,
    MB_ERR = -1,
    MB_TIMEOUT = -2,
    MB_CLOSED = -3
} mb_status_t;

typedef enum {
    MB_EX_NONE = 0x00,
    MB_EX_ILLEGAL_FUNCTION = 0x01,
    MB_EX_ILLEGAL_DATA_ADDRESS = 0x02,
    MB_EX_ILLEGAL_DATA_VALUE = 0x03,
    MB_EX_SERVER_FAILURE = 0x04
} mb_exception_t;

typedef struct {
    uint16_t holding[MB_HOLDING_COUNT];
    uint16_t input[MB_INPUT_COUNT];
    uint8_t unit_id;
    bool accept_any_unit;
} mb_map_t;

#endif
