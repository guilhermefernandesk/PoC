#include "mb_map.h"
#include "mb_pdu.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,           \
                    #condition);                                               \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static int test_fc03_read_holding(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);
    for (uint16_t i = 0u; i < 4u; ++i) {
        map.holding[i] = (uint16_t)(0x1000u + i);
    }

    const uint8_t request[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x04u};
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 10u);
    CHECK(response[0] == 0x03u);
    CHECK(response[1] == 0x08u);
    CHECK(memcmp(&response[2], "\x10\x00\x10\x01\x10\x02\x10\x03", 8u) == 0);

    return 0;
}

static int test_fc04_read_input(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);
    for (uint16_t i = 0u; i < 3u; ++i) {
        map.input[i] = (uint16_t)(2000u + i);
    }

    const uint8_t request[] = {0x04u, 0x00u, 0x00u, 0x00u, 0x03u};
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 8u);
    CHECK(response[0] == 0x04u);
    CHECK(response[1] == 0x06u);
    CHECK(memcmp(&response[2], "\x07\xD0\x07\xD1\x07\xD2", 6u) == 0);

    return 0;
}

static int test_fc06_write_single(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t request[] = {0x06u, 0x00u, 0x05u, 0x12u, 0x34u};
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 5u);
    CHECK(memcmp(response, request, 5u) == 0);
    CHECK(map.holding[5] == 0x1234u);

    return 0;
}

static int test_fc16_write_multiple(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t request[] = {
        0x10u, 0x00u, 0x01u, 0x00u, 0x03u, 0x06u,
        0x00u, 0x0Au, 0x00u, 0x14u, 0x00u, 0x1Eu,
    };
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 5u);
    CHECK(response[0] == 0x10u);
    CHECK((response[1] == 0x00u) && (response[2] == 0x01u));
    CHECK((response[3] == 0x00u) && (response[4] == 0x03u));
    CHECK(map.holding[1] == 10u);
    CHECK(map.holding[2] == 20u);
    CHECK(map.holding[3] == 30u);

    return 0;
}

static int test_exception_illegal_address(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t request[] = {0x03u, 0x03u, 0xE8u, 0x00u, 0x01u};
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 2u);
    CHECK(response[0] == 0x83u);
    CHECK(response[1] == 0x02u);

    return 0;
}

static int test_exception_illegal_value(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t zero_quantity[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x00u};
    const uint8_t too_many[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x7Eu};
    uint8_t response[MB_MAX_PDU];

    size_t length = mb_pdu_handle(&map, zero_quantity, sizeof(zero_quantity),
                                  response, sizeof(response));
    CHECK((length == 2u) && (response[0] == 0x83u) && (response[1] == 0x03u));

    length = mb_pdu_handle(&map, too_many, sizeof(too_many), response,
                           sizeof(response));
    CHECK((length == 2u) && (response[0] == 0x83u) && (response[1] == 0x03u));

    return 0;
}

static int test_exception_illegal_function(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t request[] = {0x05u, 0x00u, 0x00u, 0x00u, 0x01u};
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 2u);
    CHECK(response[0] == 0x85u);
    CHECK(response[1] == 0x01u);

    return 0;
}

static int test_fc16_bad_byte_count(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t request[] = {0x10u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u,
                               0x00u, 0x01u};
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 2u);
    CHECK(response[0] == 0x90u);
    CHECK(response[1] == 0x03u);
    CHECK(map.holding[0] == 0u);

    return 0;
}

static int test_fc16_bad_length(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t request[] = {0x10u, 0x00u, 0x00u, 0x00u, 0x02u, 0x04u,
                               0x00u, 0x01u};
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 2u);
    CHECK(response[0] == 0x90u);
    CHECK(response[1] == 0x03u);

    return 0;
}

static int test_fc06_illegal_address(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t request[] = {0x06u, 0x03u, 0xE8u, 0x00u, 0x01u};
    uint8_t response[MB_MAX_PDU];
    const size_t length =
        mb_pdu_handle(&map, request, sizeof(request), response, sizeof(response));

    CHECK(length == 2u);
    CHECK(response[0] == 0x86u);
    CHECK(response[1] == 0x02u);

    return 0;
}

typedef int (*test_function_t)(void);

typedef struct {
    const char *name;
    test_function_t function;
} test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"fc03_read_holding", test_fc03_read_holding},
        {"fc04_read_input", test_fc04_read_input},
        {"fc06_write_single", test_fc06_write_single},
        {"fc16_write_multiple", test_fc16_write_multiple},
        {"exception_illegal_address", test_exception_illegal_address},
        {"exception_illegal_value", test_exception_illegal_value},
        {"exception_illegal_function", test_exception_illegal_function},
        {"fc16_bad_byte_count", test_fc16_bad_byte_count},
        {"fc16_bad_length", test_fc16_bad_length},
        {"fc06_illegal_address", test_fc06_illegal_address},
    };

    for (size_t i = 0u; i < (sizeof(tests) / sizeof(tests[0])); ++i) {
        if (tests[i].function() != 0) {
            fprintf(stderr, "test failed: %s\n", tests[i].name);
            return 1;
        }
        printf("PASS %s\n", tests[i].name);
    }

    return 0;
}
