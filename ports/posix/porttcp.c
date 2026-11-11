/*
 * Modbus TCP PoC - FreeModbus TCP port for host builds (Linux sockets).
 *
 * One client at a time. MBAP framing (fragmented and coalesced frames),
 * MBAP length validation and the Unit ID filter are implemented here
 * because the FreeModbus TCP core uses the pseudo address 255 and accepts
 * any unit id. eMBPoll() drives accept/recv through xMBPortTCPPool(), so
 * the server is a plain "while (eMBPoll())" loop.
 */

#define _POSIX_C_SOURCE 200809L

#include "port.h"

#include "mb.h"
#include "mbport.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* MBAP header offsets (inside aucTCPBuf). */
#define MB_TCP_TID                0
#define MB_TCP_PID                2
#define MB_TCP_LEN                4
#define MB_TCP_UID                6
#define MB_TCP_FUNC               7

#define MB_TCP_PROTOCOL_ID        0
#define MB_TCP_MAX_ADU_LENGTH     260 /* 7 byte MBAP + max 253 byte PDU */
#define MB_TCP_MAX_PDU_LENGTH     254 /* sanity bound for the MBAP field */

static int      listen_fd = -1;
static int      client_fd = -1;

static UCHAR    aucTCPBuf[MB_TCP_MAX_ADU_LENGTH];
static USHORT   usTCPBufPos;
static USHORT   usTCPFrameBytesLeft;

static void
prvFrameReset( void )
{
    usTCPBufPos = 0;
    usTCPFrameBytesLeft = MB_TCP_FUNC;
}

static void
prvClientClose( void )
{
    if( client_fd >= 0 )
    {
        ( void )close( client_fd );
        client_fd = -1;
    }
    prvFrameReset(  );
}

/* Returns 1 readable/hangup, 0 timeout, -1 error. */
static int
prvWaitReadable( int fd, uint32_t timeout_ms )
{
    struct pollfd pfd;
    int           rc;
    int           timeout;

    if( timeout_ms > ( uint32_t ) INT_MAX )
    {
        timeout = INT_MAX;
    }
    else
    {
        timeout = ( int )timeout_ms;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    for( ;; )
    {
        rc = poll( &pfd, 1, timeout );
        if( rc < 0 )
        {
            if( errno == EINTR )
            {
                continue;
            }
            return -1;
        }
        if( rc == 0 )
        {
            return 0;
        }
        if( pfd.revents & ( POLLERR | POLLNVAL ) )
        {
            return -1;
        }
        return 1;               /* POLLIN and/or POLLHUP */
    }
}

BOOL
xMBTCPPortInit( USHORT usTCPPort )
{
    struct sockaddr_in address;
    int                fd;
    int                one = 1;

    if( usTCPPort == 0 )
    {
        usTCPPort = 502;
    }

    fd = socket( AF_INET, SOCK_STREAM, 0 );
    if( fd < 0 )
    {
        return FALSE;
    }

    ( void )setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof( one ) );

    memset( &address, 0, sizeof( address ) );
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl( INADDR_ANY );
    address.sin_port = htons( usTCPPort );

    if( ( bind( fd, ( const struct sockaddr * )&address, sizeof( address ) ) < 0 ) ||
        ( listen( fd, 4 ) < 0 ) )
    {
        ( void )close( fd );
        return FALSE;
    }

    if( listen_fd >= 0 )
    {
        ( void )close( listen_fd );
    }
    listen_fd = fd;

    client_fd = -1;
    prvFrameReset(  );
    return TRUE;
}

void
vMBTCPPortClose( void )
{
    prvClientClose(  );
    if( listen_fd >= 0 )
    {
        ( void )close( listen_fd );
        listen_fd = -1;
    }
}

void
vMBTCPPortDisable( void )
{
    prvClientClose(  );
}

static BOOL
prvAcceptClient( void )
{
    int fd;

    if( prvWaitReadable( listen_fd, MB_PORT_TCP_POOL_TIMEOUT_MS ) != 1 )
    {
        return FALSE;
    }

    fd = accept( listen_fd, NULL, NULL );
    if( fd < 0 )
    {
        return FALSE;
    }

    /* One client at a time; a second client waits in the backlog. */
    client_fd = fd;
    prvFrameReset(  );
    return TRUE;
}

BOOL
xMBPortTCPPool( void )
{
    ssize_t received;
    USHORT  usLength;
    USHORT  usTotal;

    if( listen_fd < 0 )
    {
        return FALSE;
    }

    if( client_fd < 0 )
    {
        ( void )prvAcceptClient(  );
        return TRUE;
    }

    if( prvWaitReadable( client_fd, MB_PORT_TCP_POOL_TIMEOUT_MS ) != 1 )
    {
        return TRUE;
    }

    /* Read at most what the current frame still needs: coalesced frames
     * stay in the socket buffer for the next pool call. */
    received = recv( client_fd, &aucTCPBuf[usTCPBufPos], ( size_t ) usTCPFrameBytesLeft, 0 );
    if( received == 0 )
    {
        prvClientClose(  );     /* orderly shutdown */
        return TRUE;
    }
    if( received < 0 )
    {
        if( ( errno == EINTR ) || ( errno == EAGAIN ) || ( errno == EWOULDBLOCK ) )
        {
            return TRUE;
        }
        prvClientClose(  );
        return TRUE;
    }

    usTCPBufPos = ( USHORT ) ( usTCPBufPos + ( USHORT ) received );
    usTCPFrameBytesLeft = ( USHORT ) ( usTCPFrameBytesLeft - ( USHORT ) received );

    if( usTCPBufPos < MB_TCP_FUNC )
    {
        usTCPFrameBytesLeft = ( USHORT ) ( MB_TCP_FUNC - usTCPBufPos );
        return TRUE;
    }

    usLength = ( USHORT ) ( ( ( USHORT ) aucTCPBuf[MB_TCP_LEN] << 8 ) | aucTCPBuf[MB_TCP_LEN + 1] );

    if( ( usLength < 2u ) || ( usLength > MB_TCP_MAX_PDU_LENGTH ) )
    {
        prvClientClose(  );     /* malformed MBAP length */
        return TRUE;
    }

    usTotal = ( USHORT ) ( MB_TCP_UID + usLength );

    if( usTCPBufPos < usTotal )
    {
        usTCPFrameBytesLeft = ( USHORT ) ( usTotal - usTCPBufPos );
    }
    else if( usTCPBufPos == usTotal )
    {
#if MB_PORT_TCP_ACCEPT_ANY_UNIT == 0
        if( aucTCPBuf[MB_TCP_UID] != MB_PORT_TCP_UNIT_ID )
        {
            /* Not for this slave: drop the frame, keep the connection. */
            prvFrameReset(  );
            return TRUE;
        }
#endif
        ( void )xMBPortEventPost( EV_FRAME_RECEIVED );
    }
    else
    {
        prvClientClose(  );
    }

    return TRUE;
}

BOOL
xMBTCPPortGetRequest( UCHAR ** ppucMBTCPFrame, USHORT * pusTCPLength )
{
    *ppucMBTCPFrame = &aucTCPBuf[0];
    *pusTCPLength = usTCPBufPos;

    /* FreeModbus builds the response in this same buffer (TID/PID/UID are
     * kept from the request, so they are echoed back). */
    prvFrameReset(  );
    return TRUE;
}

BOOL
xMBTCPPortSendResponse( const UCHAR * pucMBTCPFrame, USHORT usTCPLength )
{
    size_t sent = 0;

    if( client_fd < 0 )
    {
        return FALSE;
    }

    while( sent < ( size_t ) usTCPLength )
    {
#ifdef MSG_NOSIGNAL
        ssize_t rc = send( client_fd, &pucMBTCPFrame[sent],
                           ( size_t ) usTCPLength - sent, ( int )MSG_NOSIGNAL );
#else
        ssize_t rc = send( client_fd, &pucMBTCPFrame[sent],
                           ( size_t ) usTCPLength - sent, 0 );
#endif
        if( rc < 0 )
        {
            if( errno == EINTR )
            {
                continue;
            }
            prvClientClose(  );
            return FALSE;
        }
        if( rc == 0 )
        {
            prvClientClose(  );
            return FALSE;
        }
        sent += ( size_t ) rc;
    }
    return TRUE;
}
