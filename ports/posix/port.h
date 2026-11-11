/*
 * Modbus TCP PoC - FreeModbus port configuration for host builds (POSIX).
 *
 * FreeModbus includes port.h before mbconfig.h, so every macro defined here
 * overrides the #ifndef defaults in third_party/freemodbus/modbus/include.
 */

#ifndef PORT_H
#define PORT_H

/* ----------------------- Modbus stack configuration ---------------------- */

/* Modbus mode: TCP only. */
#define MB_ASCII_ENABLED                    ( 0 )
#define MB_RTU_ENABLED                      ( 0 )
#define MB_TCP_ENABLED                      ( 1 )

/*
 * Function codes enabled in this PoC. Coils/discrete inputs stay disabled
 * so those codes answer MB_EX_ILLEGAL_FUNCTION (01), like the previous
 * hand-written core did. Flip a bit here and implement the matching
 * callback in app/app_regs.c to enable one.
 */
#define MB_FUNC_READ_HOLDING_ENABLED        ( 1 ) /* FC03 */
#define MB_FUNC_READ_INPUT_ENABLED          ( 1 ) /* FC04 */
#define MB_FUNC_WRITE_HOLDING_ENABLED       ( 1 ) /* FC06 */
#define MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED ( 1 ) /* FC16 */
#define MB_FUNC_READ_COILS_ENABLED          ( 0 ) /* FC01 */
#define MB_FUNC_READ_DISCRETE_INPUTS_ENABLED ( 0 ) /* FC02 */
#define MB_FUNC_WRITE_COIL_ENABLED          ( 0 ) /* FC05 */
#define MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED ( 0 ) /* FC15 */
#define MB_FUNC_READWRITE_HOLDING_ENABLED   ( 0 ) /* FC23 */
#define MB_FUNC_OTHER_REP_SLAVEID_ENABLED    ( 0 ) /* FC17 */

/*
 * Unit ID filter. The FreeModbus TCP core uses the pseudo address 255 and
 * answers any unit id, so the filter lives in the port (porttcp.c).
 */
#define MB_PORT_TCP_UNIT_ID                 ( 1 )
#define MB_PORT_TCP_ACCEPT_ANY_UNIT         ( 0 )

/* How long xMBPortTCPPool() may block on accept/recv before returning. */
#define MB_PORT_TCP_POOL_TIMEOUT_MS         ( 50 )

/* TCP port the slave listens on (0 = Modbus default 502). */
#define MB_PORT_TCP_LISTEN_PORT             ( 502 )

/* The port implements vMBTCPPortClose(); let eMBClose() call it. */
#define MB_PORT_HAS_CLOSE                   ( 1 )

/* ----------------------- Platform types ---------------------------------- */

typedef char    BOOL;
typedef char    CHAR;
typedef unsigned char UCHAR;
typedef short   SHORT;
typedef unsigned short USHORT;
typedef long    LONG;
typedef unsigned long ULONG;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ----------------------- Critical sections ------------------------------- */

/*
 * The host server is single threaded: only the main() loop touches the
 * stack, so critical sections are empty.
 */
#define ENTER_CRITICAL_SECTION()
#define EXIT_CRITICAL_SECTION()
#define PR_BEGIN_EXTERN_C extern "C" {
#define PR_END_EXTERN_C }
#define PR_BEGIN_IGNORE_UNUSED_PARA
#define PR_END_IGNORE_UNUSED_PARA

#define INLINE
#define assert( x )

#endif /* PORT_H */
