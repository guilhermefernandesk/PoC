/*
 * Modbus TCP PoC - FreeModbus event glue (target build, RTOS independent).
 *
 * FreeModbus calls xMBPortEventGet() from eMBPoll(). When no event is
 * pending we run the TCP pool (accept/recv) right here, so all Modbus I/O
 * happens in the single task that calls eMBPoll(). No queues and no extra
 * tasks: the same shape works on POSIX and on the FreeRTOS/LwIP target.
 */

#include "port.h"

#include "mb.h"
#include "mbport.h"

/* Implemented in porttcp.c. */
BOOL xMBPortTCPPool( void );

static eMBEventType eRcvEvent;
static BOOL xEventInQueue;

BOOL
xMBPortEventInit( void )
{
    xEventInQueue = FALSE;
    return TRUE;
}

BOOL
xMBPortEventPost( eMBEventType eEvent )
{
    xEventInQueue = TRUE;
    eRcvEvent = eEvent;
    return TRUE;
}

BOOL
xMBPortEventGet( eMBEventType * eEvent )
{
    BOOL xEventHappened;

    if( xEventInQueue )
    {
        *eEvent = eRcvEvent;
        xEventInQueue = FALSE;
        xEventHappened = TRUE;
    }
    else
    {
        ( void )xMBPortTCPPool(  );
        xEventHappened = FALSE;
    }

    return xEventHappened;
}
