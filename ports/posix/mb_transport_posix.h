#ifndef MB_TRANSPORT_POSIX_H
#define MB_TRANSPORT_POSIX_H

#include "mb_transport.h"

/*
 * POSIX/Linux transport used only by the host test server.
 * It is intentionally outside modules/modbus so the core stays portable.
 */
const mb_transport_t *mb_transport_posix_get(void);

#endif
