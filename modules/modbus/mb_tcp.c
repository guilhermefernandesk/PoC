#include "mb_tcp.h"

#include "mb_map.h"
#include "mb_pdu.h"

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

int mb_tcp_process(mb_map_t *map,
                   const uint8_t *adu,
                   size_t adu_len,
                   uint8_t *out,
                   size_t out_cap,
                   size_t *out_len)
{
    if ((map == NULL) || (adu == NULL) || (out == NULL) || (out_len == NULL)) {
        return MB_ERR;
    }

    *out_len = 0u;

    /* MBAP (7 bytes) + at least one PDU byte. */
    if (adu_len < 8u) {
        return MB_ERR;
    }

    const uint16_t protocol_id = read_be16(&adu[2]);
    const uint16_t length = read_be16(&adu[4]);

    /* length covers Unit ID + PDU. */
    if ((length < 2u) || (length > 254u)) {
        return MB_ERR;
    }

    if (adu_len != (size_t)(6u + length)) {
        return MB_ERR;
    }

    /* Modbus TCP requires protocol identifier 0. */
    if (protocol_id != 0u) {
        return 0;
    }

    const uint8_t unit_id = adu[6];
    if (!mb_map_unit_matches(map, unit_id)) {
        return 0;
    }

    const uint8_t *pdu = &adu[7];
    const size_t pdu_len = (size_t)length - 1u;

    uint8_t pdu_response[MB_MAX_PDU];
    const size_t pdu_response_len =
        mb_pdu_handle(map, pdu, pdu_len, pdu_response, sizeof(pdu_response));

    if (pdu_response_len == 0u) {
        return 0;
    }

    if (pdu_response_len > MB_MAX_PDU) {
        return MB_ERR;
    }

    const size_t total_len = 7u + pdu_response_len;
    if (out_cap < total_len) {
        return MB_ERR;
    }

    out[0] = adu[0]; /* transaction ID high */
    out[1] = adu[1]; /* transaction ID low */
    write_be16(&out[2], 0u);
    write_be16(&out[4], (uint16_t)(1u + pdu_response_len));
    out[6] = unit_id;
    memcpy(&out[7], pdu_response, pdu_response_len);

    *out_len = total_len;
    return 1;
}
