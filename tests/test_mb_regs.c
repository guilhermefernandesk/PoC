/*
 * Modbus TCP PoC - unit tests for the application register map wired to
 * the FreeModbus callbacks (app/app_regs.c).
 *
 * Protocol behaviour over TCP (framing, exceptions, unit filter) is
 * covered by tools/modbus_test.py against mb_server_posix.
 */

#include "app_regs.h"
#include "mb.h"
#include "port.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CHECK( cond ) do { \
        if( !( cond ) ) { \
            printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond ); \
            return false; \
        } \
    } while( 0 )

static bool test_holding_initial_values( void )
{
    UCHAR buf[8];

    app_regs_init(  );

    /* Callbacks are 1-based: PDU address 0 arrives as 1. */
    CHECK( eMBRegHoldingCB( buf, 1, 4, MB_REG_READ ) == MB_ENOERR );
    CHECK( buf[0] == 0x00 && buf[1] == 0x00 );   /* holding[0] = 0 */
    CHECK( buf[2] == 0x00 && buf[3] == 0x01 );   /* holding[1] = 1 */
    CHECK( buf[4] == 0x00 && buf[5] == 0x02 );   /* holding[2] = 2 */
    CHECK( buf[6] == 0x00 && buf[7] == 0x03 );   /* holding[3] = 3 */
    return true;
}

static bool test_input_initial_values( void )
{
    UCHAR buf[8];

    app_regs_init(  );

    CHECK( eMBRegInputCB( buf, 1, 4 ) == MB_ENOERR );
    CHECK( buf[0] == 0x03 && buf[1] == 0xE8 );   /* input[0] = 1000 */
    CHECK( buf[2] == 0x03 && buf[3] == 0xE9 );   /* input[1] = 1001 */
    CHECK( buf[4] == 0x03 && buf[5] == 0xEA );   /* input[2] = 1002 */
    CHECK( buf[6] == 0x03 && buf[7] == 0xEB );   /* input[3] = 1003 */
    return true;
}

static bool test_holding_write_and_readback( void )
{
    UCHAR in[2] = { 0xAB, 0xCD };
    UCHAR out[2];

    CHECK( eMBRegHoldingCB( in, 6, 1, MB_REG_WRITE ) == MB_ENOERR );
    CHECK( eMBRegHoldingCB( out, 6, 1, MB_REG_READ ) == MB_ENOERR );
    CHECK( out[0] == 0xAB && out[1] == 0xCD );
    return true;
}

static bool test_holding_write_multiple( void )
{
    UCHAR in[6] = { 0x00, 0x0A, 0x00, 0x14, 0x00, 0x1E };
    UCHAR out[6];

    CHECK( eMBRegHoldingCB( in, 11, 3, MB_REG_WRITE ) == MB_ENOERR );
    CHECK( eMBRegHoldingCB( out, 11, 3, MB_REG_READ ) == MB_ENOERR );
    CHECK( memcmp( in, out, sizeof( in ) ) == 0 );

    /* Neighbouring registers are untouched. */
    CHECK( eMBRegHoldingCB( out, 10, 1, MB_REG_READ ) == MB_ENOERR );
    CHECK( out[0] == 0x00 && out[1] == 0x09 );
    return true;
}

static bool test_address_bounds( void )
{
    UCHAR buf[2] = { 0 };

    app_regs_init(  );

    /* Last register (PDU address 99) is readable/writable. */
    CHECK( eMBRegHoldingCB( buf, 100, 1, MB_REG_READ ) == MB_ENOERR );
    /* ...but not two registers from there. */
    CHECK( eMBRegHoldingCB( buf, 100, 2, MB_REG_READ ) == MB_ENOREG );
    CHECK( eMBRegHoldingCB( buf, 101, 1, MB_REG_READ ) == MB_ENOREG );
    CHECK( eMBRegHoldingCB( buf, 101, 1, MB_REG_WRITE ) == MB_ENOREG );
    CHECK( eMBRegHoldingCB( buf, 0, 1, MB_REG_READ ) == MB_ENOREG );
    CHECK( eMBRegHoldingCB( buf, 0xFFFF, 1, MB_REG_READ ) == MB_ENOREG );

    CHECK( eMBRegInputCB( buf, 100, 1 ) == MB_ENOERR );
    CHECK( eMBRegInputCB( buf, 100, 2 ) == MB_ENOREG );
    CHECK( eMBRegInputCB( buf, 101, 1 ) == MB_ENOREG );
    CHECK( eMBRegInputCB( buf, 0, 1 ) == MB_ENOREG );
    CHECK( eMBRegInputCB( buf, 0xFFFF, 1 ) == MB_ENOREG );
    return true;
}

static bool test_unmapped_types( void )
{
    UCHAR buf[2] = { 0 };

    /* Coils/discrete inputs are not mapped in this PoC. */
    CHECK( eMBRegCoilsCB( buf, 0, 1, MB_REG_READ ) == MB_ENOREG );
    CHECK( eMBRegCoilsCB( buf, 0, 1, MB_REG_WRITE ) == MB_ENOREG );
    CHECK( eMBRegDiscreteCB( buf, 0, 1 ) == MB_ENOREG );
    return true;
}

int
main( void )
{
    typedef bool ( *test_fn_t ) ( void );
    static const test_fn_t tests[] = {
        test_holding_initial_values,
        test_input_initial_values,
        test_holding_write_and_readback,
        test_holding_write_multiple,
        test_address_bounds,
        test_unmapped_types,
    };
    size_t i;
    int    failed = 0;

    for( i = 0; i < sizeof( tests ) / sizeof( tests[0] ); i++ )
    {
        if( !tests[i](  ) )
        {
            failed++;
        }
    }

    printf( "%zu tests, %d failed\n", sizeof( tests ) / sizeof( tests[0] ), failed );
    return ( failed == 0 ) ? 0 : 1;
}
