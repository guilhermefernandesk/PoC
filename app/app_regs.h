#ifndef APP_REGS_H
#define APP_REGS_H

/*
 * Demo register map. Replace this file with the real PLC mapping later.
 *
 * Register arrays are zero-based (holding[0] is PLC 40001). The FreeModbus
 * callbacks translate the 1-based protocol addresses they receive
 * (PDU address N -> callback N + 1), so callers see plain 0-based PDU
 * addresses, as tools/modbus_test.py expects.
 */

#define APP_REGS_HOLDING_COUNT 100u
#define APP_REGS_INPUT_COUNT   100u

void app_regs_init( void );

#endif
