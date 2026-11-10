#include "mb_map.h"
#include "mb_tcp.h"

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

static size_t make_adu(uint8_t *adu,
                       uint16_t transaction_id,
                       uint16_t protocol_id,
                       uint8_t unit_id,
                       const uint8_t *pdu,
                       size_t pdu_len)
{
    const uint16_t length = (uint16_t)(pdu_len + 1u);

    adu[0] = (uint8_t)(transaction_id >> 8);
    adu[1] = (uint8_t)(transaction_id & 0xFFu);
    adu[2] = (uint8_t)(protocol_id >> 8);
    adu[3] = (uint8_t)(protocol_id & 0xFFu);
    adu[4] = (uint8_t)(length >> 8);
    adu[5] = (uint8_t)(length & 0xFFu);
    adu[6] = unit_id;
    memcpy(&adu[7], pdu, pdu_len);

    return 7u + pdu_len;
}

static int test_fc03_response(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);
    map.holding[0] = 0x1111u;
    map.holding[1] = 0x2222u;

    const uint8_t pdu[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x02u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x1234u, 0u, 1u, pdu, sizeof(pdu));

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result == 1);
    CHECK(response_len == 13u);
    CHECK((response[0] == 0x12u) && (response[1] == 0x34u));
    CHECK((response[2] == 0x00u) && (response[3] == 0x00u));
    CHECK((response[4] == 0x00u) && (response[5] == 0x07u));
    CHECK(response[6] == 0x01u);
    CHECK(response[7] == 0x03u);
    CHECK(response[8] == 0x04u);
    CHECK(memcmp(&response[9], "\x11\x11\x22\x22", 4u) == 0);

    return 0;
}

static int test_fc06_response(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t pdu[] = {0x06u, 0x00u, 0x07u, 0xABu, 0xCDu};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x0002u, 0u, 1u, pdu, sizeof(pdu));

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result == 1);
    CHECK(response_len == 12u);
    CHECK(response[5] == 0x06u);
    CHECK(memcmp(&response[7], pdu, sizeof(pdu)) == 0);
    CHECK(map.holding[7] == 0xABCDu);

    return 0;
}

static int test_fc16_response(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t pdu[] = {0x10u, 0x00u, 0x01u, 0x00u, 0x02u, 0x04u,
                           0x00u, 0x0Au, 0x00u, 0x14u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x0003u, 0u, 1u, pdu, sizeof(pdu));

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result == 1);
    CHECK(response_len == 12u);
    CHECK(response[5] == 0x06u);
    CHECK((response[7] == 0x10u) && (response[8] == 0x00u));
    CHECK((response[9] == 0x01u) && (response[10] == 0x00u));
    CHECK(response[11] == 0x02u);
    CHECK(map.holding[1] == 10u);
    CHECK(map.holding[2] == 20u);

    return 0;
}

static int test_exception_response(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t pdu[] = {0x03u, 0x03u, 0xE8u, 0x00u, 0x01u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x0004u, 0u, 1u, pdu, sizeof(pdu));

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result == 1);
    CHECK(response_len == 9u);
    CHECK(response[5] == 0x03u);
    CHECK((response[7] == 0x83u) && (response[8] == 0x02u));

    return 0;
}

static int test_wrong_protocol_is_ignored(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t pdu[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x01u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x0005u, 1u, 1u, pdu, sizeof(pdu));

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 123u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result == 0);
    CHECK(response_len == 0u);

    return 0;
}

static int test_wrong_unit_is_ignored(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t pdu[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x01u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x0006u, 0u, 2u, pdu, sizeof(pdu));

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result == 0);
    CHECK(response_len == 0u);

    return 0;
}

static int test_accept_any_unit(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, true);

    const uint8_t pdu[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x01u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x0007u, 0u, 0xFFu, pdu, sizeof(pdu));

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result == 1);
    CHECK(response[6] == 0xFFu);

    return 0;
}

static int test_truncated_adu(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t adu[7] = {0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x06u, 0x01u};
    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, sizeof(adu), response,
                                      sizeof(response), &response_len);

    CHECK(result < 0);
    CHECK(response_len == 0u);

    return 0;
}

static int test_length_mismatch(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t pdu[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x01u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x0008u, 0u, 1u, pdu, sizeof(pdu));
    adu[5] = 0x7Fu; /* length says more bytes than the ADU contains */

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result < 0);
    CHECK(response_len == 0u);

    return 0;
}

static int test_length_too_small(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t pdu[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x01u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x0009u, 0u, 1u, pdu, sizeof(pdu));
    adu[4] = 0u;
    adu[5] = 1u;

    uint8_t response[MB_MAX_ADU];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result < 0);

    return 0;
}

static int test_response_buffer_too_small(void)
{
    mb_map_t map;
    mb_map_init(&map, 1u, false);

    const uint8_t pdu[] = {0x03u, 0x00u, 0x00u, 0x00u, 0x01u};
    uint8_t adu[MB_MAX_ADU];
    const size_t adu_len = make_adu(adu, 0x000Au, 0u, 1u, pdu, sizeof(pdu));

    uint8_t response[8];
    size_t response_len = 0u;
    const int result = mb_tcp_process(&map, adu, adu_len, response,
                                      sizeof(response), &response_len);

    CHECK(result < 0);

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
        {"fc03_response", test_fc03_response},
        {"fc06_response", test_fc06_response},
        {"fc16_response", test_fc16_response},
        {"exception_response", test_exception_response},
        {"wrong_protocol_is_ignored", test_wrong_protocol_is_ignored},
        {"wrong_unit_is_ignored", test_wrong_unit_is_ignored},
        {"accept_any_unit", test_accept_any_unit},
        {"truncated_adu", test_truncated_adu},
        {"length_mismatch", test_length_mismatch},
        {"length_too_small", test_length_too_small},
        {"response_buffer_too_small", test_response_buffer_too_small},
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
