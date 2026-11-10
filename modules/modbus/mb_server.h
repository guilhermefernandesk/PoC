#ifndef MB_SERVER_H
#define MB_SERVER_H

#include "mb_transport.h"
#include "mb_types.h"

typedef struct {
    const mb_transport_t *transport;
    mb_map_t *map;
    uint16_t port;
    uint32_t accept_timeout_ms;
    uint32_t recv_timeout_ms;
} mb_server_cfg_t;

/*
 * Run the Modbus TCP server forever.
 *
 * The current implementation handles one client connection at a time. When the
 * client disconnects it goes back to accept(). This is enough for a PLC that
 * keeps a single connection open.
 */
int mb_server_run(const mb_server_cfg_t *cfg);

#endif
