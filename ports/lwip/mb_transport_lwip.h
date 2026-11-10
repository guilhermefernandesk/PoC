#ifndef MB_TRANSPORT_LWIP_H
#define MB_TRANSPORT_LWIP_H

#include "mb_transport.h"

/*
 * LwIP netconn transport for the STM32 target.
 *
 * Required LwIP options in lwipopts.h:
 *   LWIP_TCP=1
 *   LWIP_SO_RCVTIMEO=1   (lets accept/recv time out instead of blocking forever)
 *
 * This file is not part of the host build. Keep the Modbus core free of LwIP
 * headers; only this adapter includes lwip/api.h.
 */
const mb_transport_t *mb_transport_lwip_get(void);

#endif
