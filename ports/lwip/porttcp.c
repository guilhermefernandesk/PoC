/*
 * Modbus TCP PoC - FreeModbus TCP port for the STM32H7 target (LwIP
 * netconn API, no HAL).
 *
 * One client at a time. MBAP framing (fragmented and coalesced frames),
 * MBAP length validation and the Unit ID filter are implemented here
 * because the FreeModbus TCP core uses the pseudo address 255 and accepts
 * any unit id. eMBPoll() drives accept/recv through xMBPortTCPPool(), so
 * the application is a plain "for (;;) eMBPoll();" task.
 *
 * Requires lwipopts.h: LWIP_NETCONN = 1 (and LWIP_SO_RCVTIMEO = 1 for the
 * pool timeout; without it the pool blocks until data arrives).
 */

#include "port.h"

#include "mb.h"
#include "mbport.h"

#include "lwip/api.h"
#include "lwip/opt.h"
#include "lwip/pbuf.h"

/* MBAP header offsets (inside aucTCPBuf). */
#define MB_TCP_TID                0
#define MB_TCP_PID                2
#define MB_TCP_LEN                4
#define MB_TCP_UID                6
#define MB_TCP_FUNC               7

#define MB_TCP_PROTOCOL_ID        0
#define MB_TCP_MAX_ADU_LENGTH     260 /* 7 byte MBAP + max 253 byte PDU */
#define MB_TCP_MAX_PDU_LENGTH     254 /* sanity bound for the MBAP field */

static struct netconn *listener_conn;
static struct netconn *client_conn;

/* Partially consumed pbuf chain from the last netconn_recv_tcp_pbuf(). */
static struct pbuf *rx_pbuf;
static u16_t        rx_offset;

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
    if( rx_pbuf != NULL )
    {
        ( void )pbuf_free( rx_pbuf );
        rx_pbuf = NULL;
    }
    rx_offset = 0;

    if( client_conn != NULL )
    {
        ( void )netconn_close( client_conn );
        ( void )netconn_delete( client_conn );
        client_conn = NULL;
    }
    prvFrameReset(  );
}

static void
prvSetRecvTimeout( struct netconn * conn, uint32_t timeout_ms )
{
#if LWIP_SO_RCVTIMEO
    netconn_set_recvtimeout( conn, ( s32_t ) timeout_ms );
#else
    ( void )conn;
    ( void )timeout_ms;
#endif
}

BOOL
xMBTCPPortInit( USHORT usTCPPort )
{
    struct netconn *listener;
    err_t           err;

    if( usTCPPort == 0 )
    {
        usTCPPort = 502;
    }

    listener = netconn_new( NETCONN_TCP );
    if( listener == NULL )
    {
        return FALSE;
    }

    err = netconn_bind( listener, IP_ADDR_ANY, ( u16_t ) usTCPPort );
    if( err == ERR_OK )
    {
        err = netconn_listen( listener );
    }
    if( err != ERR_OK )
    {
        ( void )netconn_delete( listener );
        return FALSE;
    }

    if( listener_conn != NULL )
    {
        ( void )netconn_delete( listener_conn );
    }
    listener_conn = listener;

    client_conn = NULL;
    rx_pbuf = NULL;
    rx_offset = 0;
    prvFrameReset(  );
    return TRUE;
}

void
vMBTCPPortClose( void )
{
    prvClientClose(  );
    if( listener_conn != NULL )
    {
        ( void )netconn_delete( listener_conn );
        listener_conn = NULL;
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
    struct netconn *accepted = NULL;
    err_t           err;

    prvSetRecvTimeout( listener_conn, MB_PORT_TCP_POOL_TIMEOUT_MS );

    err = netconn_accept( listener_conn, &accepted );
    if( err != ERR_OK )
    {
        return FALSE;           /* timeout or error */
    }

    /* One client at a time; a second client waits in the backlog. */
    client_conn = accepted;
    rx_pbuf = NULL;
    rx_offset = 0;
    prvFrameReset(  );
    return TRUE;
}

/*
 * Receive up to length bytes into buffer.
 * Returns bytes read (>0), 0 on timeout, -1 on close/error.
 */
static int
prvRecvClient( void *buffer, size_t length )
{
    struct pbuf *received = NULL;
    err_t         err;
    u16_t         available;
    u16_t         copied;

    if( rx_pbuf == NULL )
    {
        prvSetRecvTimeout( client_conn, MB_PORT_TCP_POOL_TIMEOUT_MS );

        err = netconn_recv_tcp_pbuf( client_conn, &received );
        if( err == ERR_TIMEOUT )
        {
            return 0;
        }
        if( err != ERR_OK )
        {
            return -1;
        }
        if( ( received == NULL ) || ( received->tot_len == 0 ) )
        {
            if( received != NULL )
            {
                ( void )pbuf_free( received );
            }
            return -1;          /* orderly shutdown */
        }
        rx_pbuf = received;
        rx_offset = 0;
    }

    available = ( u16_t ) ( rx_pbuf->tot_len - rx_offset );
    if( ( size_t ) available > length )
    {
        available = ( u16_t ) length;
    }
    copied = pbuf_copy_partial( rx_pbuf, buffer, available, rx_offset );
    rx_offset = ( u16_t ) ( rx_offset + copied );

    if( ( copied == 0 ) || ( rx_offset >= rx_pbuf->tot_len ) )
    {
        ( void )pbuf_free( rx_pbuf );
        rx_pbuf = NULL;
        rx_offset = 0;
    }
    return ( int )copied;
}

BOOL
xMBPortTCPPool( void )
{
    int    received;
    USHORT usLength;
    USHORT usTotal;

    if( listener_conn == NULL )
    {
        return FALSE;
    }

    if( client_conn == NULL )
    {
        ( void )prvAcceptClient(  );
        return TRUE;
    }

    /* Read at most what the current frame still needs: coalesced frames
     * stay in the pbuf chain for the next pool call. */
    received = prvRecvClient( &aucTCPBuf[usTCPBufPos], ( size_t ) usTCPFrameBytesLeft );
    if( received < 0 )
    {
        prvClientClose(  );
        return TRUE;
    }
    if( received == 0 )
    {
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
    err_t err;

    if( client_conn == NULL )
    {
        return FALSE;
    }

    err = netconn_write( client_conn, pucMBTCPFrame, ( size_t ) usTCPLength, NETCONN_COPY );
    if( err != ERR_OK )
    {
        prvClientClose(  );
        return FALSE;
    }
    return TRUE;
}
