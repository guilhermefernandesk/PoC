# FreeModbus (vendored)

Modbus slave stack vendored into this PoC, unmodified.

- **Upstream:** <https://github.com/cwalter-at/freemodbus>
- **Commit:** `a9ec789` ("Keep handlers reachable after callback removal")
- **License:** BSD-3-Clause (see the file headers)

Only the `modbus/` tree is used: the core (`mb.c`), the function handlers
(`functions/`) and the TCP backend (`tcp/mbtcp.c`). The reference ports and
demos from upstream are **not** vendored (different licenses / HAL based);
this repository provides its own ports in `ports/posix/` and `ports/lwip/`.

Do not edit files in this directory. Configure the stack from the port
`port.h` (include order guarantees our macros win over the `#ifndef`
defaults in `modbus/include/mbconfig.h`).

## Files compiled in this PoC

| File | Content |
| --- | --- |
| `modbus/mb.c` | Stack state machine: `eMBInit/eMBTCPInit`, `eMBEnable/eMBDisable`, `eMBPoll` |
| `modbus/functions/mbfuncholding.c` | FC03 (read), FC06, FC16 (write), FC23 |
| `modbus/functions/mbfuncinput.c` | FC04 (read input registers) |
| `modbus/functions/mbfunccoils.c` | FC01/05/15 (disabled in `port.h`) |
| `modbus/functions/mbfuncdisc.c` | FC02 (disabled in `port.h`) |
| `modbus/functions/mbfuncother.c` | FC17 (disabled in `port.h`) |
| `modbus/functions/mbutils.c` | `xMBUtilSetBits`, error-to-exception mapping |
| `modbus/functions/mbfuncdiag.c` | empty upstream stub |
| `modbus/functions/mbfuncfile.c` | FC14/15 file records (disabled upstream default) |
| `modbus/tcp/mbtcp.c` | TCP frame layer: `eMBTCPInit/eMBPoll` glue |

`modbus/rtu/` and `modbus/ascii/` are not compiled (RTU/ASCII disabled in
`port.h`); they stay vendored so the tree matches upstream.
