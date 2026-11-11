/*
 * Modbus TCP PoC - host server (POSIX sockets, no HAL).
 *
 * Usage: mb_server_posix [port]   (default 5020, 502 needs root)
 *
 * With FreeModbus the server is just the stack lifecycle:
 * eMBTCPInit -> eMBEnable -> eMBPoll loop. All TCP I/O happens inside
 * eMBPoll() (see ports/posix/), one client at a time.
 */

#include "app_regs.h"
#include "mb.h"
#include "port.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int
main( int argc, char **argv )
{
    uint16_t port = 5020u;
    long     parsed;

    if( argc > 2 )
    {
        fprintf( stderr, "usage: %s [port]\n", argv[0] );
        return 2;
    }
    if( argc == 2 )
    {
        parsed = strtol( argv[1], NULL, 10 );
        if( ( parsed < 1 ) || ( parsed > 65535 ) )
        {
            fprintf( stderr, "invalid port: %s\n", argv[1] );
            return 2;
        }
        port = ( uint16_t ) parsed;
    }

    ( void )signal( SIGPIPE, SIG_IGN );
    ( void )setvbuf( stdout, NULL, _IONBF, 0 );

    app_regs_init(  );

    if( eMBTCPInit( ( USHORT ) port ) != MB_ENOERR )
    {
        fprintf( stderr, "eMBTCPInit failed on port %u\n", ( unsigned ) port );
        return 1;
    }
    if( eMBEnable(  ) != MB_ENOERR )
    {
        fprintf( stderr, "eMBEnable failed\n" );
        return 1;
    }

    printf( "Modbus TCP server (FreeModbus) listening on 0.0.0.0:%u (unit %u)\n",
            ( unsigned ) port, ( unsigned ) MB_PORT_TCP_UNIT_ID );

    for( ;; )
    {
        if( eMBPoll(  ) != MB_ENOERR )
        {
            fprintf( stderr, "eMBPoll failed\n" );
            ( void )eMBDisable(  );
            return 1;
        }
    }
}
