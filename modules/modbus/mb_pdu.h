#ifndef MB_PDU_H
#define MB_PDU_H

#include "mb_types.h"

/*
 * Handle one Modbus PDU (function code + data).
 *
 * Returns the response PDU length, or 0 when no response should be sent.
 * The response buffer must be at least 2 bytes.
 */
size_t mb_pdu_handle(mb_map_t *map,
                     const uint8_t *req,
                     size_t req_len,
                     uint8_t *resp,
                     size_t resp_cap);

#endif
