#ifndef APP_REGS_H
#define APP_REGS_H

#include "mb_types.h"

/*
 * Demo register map. Replace this file with the real PLC mapping later.
 *
 * Protocol addresses are zero-based:
 *   PLC 40001 <-> holding[0]
 *   PLC 30001 <-> input[0]
 */
void app_regs_init(void);
mb_map_t *app_regs_map(void);

#endif
