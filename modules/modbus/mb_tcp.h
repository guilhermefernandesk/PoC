#ifndef MB_TCP_H
#define MB_TCP_H

#include "mb_types.h"

/*
 * Process one complete Modbus TCP ADU (MBAP + PDU).
 *
 * Returns:
 *   1  -> response ready in out/out_len
 *   0  -> frame intentionally ignored (wrong protocol or unit ID)
 *  <0  -> malformed frame; close the connection
 */
int mb_tcp_process(mb_map_t *map,
                   const uint8_t *adu,
                   size_t adu_len,
                   uint8_t *out,
                   size_t out_cap,
                   size_t *out_len);

#endif
