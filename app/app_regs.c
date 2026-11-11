/*
 * Modbus TCP PoC - application register map wired to the FreeModbus
 * callbacks.
 *
 * holding[i] starts as i, input[i] as 1000 + i (values used by
 * tools/modbus_test.py). Registers are 16 bit, big-endian on the wire,
 * zero-based addressing. The Modbus unit id and the unit filter live in
 * ports/<port>/port.h (MB_PORT_TCP_UNIT_ID).
 */

#include "app_regs.h"

#include "mb.h"

static USHORT usHoldingRegs[APP_REGS_HOLDING_COUNT];
static USHORT usInputRegs[APP_REGS_INPUT_COUNT];

void
app_regs_init( void )
{
    USHORT i;

    for( i = 0; i < ( USHORT ) APP_REGS_HOLDING_COUNT; i++ )
    {
        usHoldingRegs[i] = i;
    }

    for( i = 0; i < ( USHORT ) APP_REGS_INPUT_COUNT; i++ )
    {
        usInputRegs[i] = ( USHORT ) ( 1000u + i );
    }
}

/*
 * FC03 / FC06 / FC16: holding registers (read/write).
 *
 * pucRegBuffer holds usNRegs registers, big-endian, one per two bytes.
 *
 * FreeModbus passes 1-based addresses (PDU address N -> callback N + 1),
 * so PDU address 0 (PLC 40001) arrives here as usAddress == 1.
 */
eMBErrorCode
eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress,
                 USHORT usNRegs, eMBRegisterMode eMode )
{
    USHORT i;
    USHORT usIndex;

    if( ( usAddress == 0u ) ||
        ( usAddress > ( USHORT ) APP_REGS_HOLDING_COUNT ) ||
        ( usNRegs > ( USHORT ) ( APP_REGS_HOLDING_COUNT - usAddress + 1u ) ) )
    {
        return MB_ENOREG;
    }

    usIndex = ( USHORT ) ( usAddress - 1u );

    if( eMode == MB_REG_READ )
    {
        for( i = 0; i < usNRegs; i++ )
        {
            *pucRegBuffer++ = ( UCHAR ) ( usHoldingRegs[usIndex + i] >> 8 );
            *pucRegBuffer++ = ( UCHAR ) ( usHoldingRegs[usIndex + i] & 0xFF );
        }
    }
    else
    {
        for( i = 0; i < usNRegs; i++ )
        {
            USHORT usValue = ( USHORT ) ( ( ( USHORT ) pucRegBuffer[0] << 8 ) | pucRegBuffer[1] );

            usHoldingRegs[usIndex + i] = usValue;
            pucRegBuffer += 2;
        }
    }
    return MB_ENOERR;
}

/* FC04: input registers (read-only). Same 1-based addressing. */
eMBErrorCode
eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
    USHORT i;
    USHORT usIndex;

    if( ( usAddress == 0u ) ||
        ( usAddress > ( USHORT ) APP_REGS_INPUT_COUNT ) ||
        ( usNRegs > ( USHORT ) ( APP_REGS_INPUT_COUNT - usAddress + 1u ) ) )
    {
        return MB_ENOREG;
    }

    usIndex = ( USHORT ) ( usAddress - 1u );

    for( i = 0; i < usNRegs; i++ )
    {
        *pucRegBuffer++ = ( UCHAR ) ( usInputRegs[usIndex + i] >> 8 );
        *pucRegBuffer++ = ( UCHAR ) ( usInputRegs[usIndex + i] & 0xFF );
    }
    return MB_ENOERR;
}

/*
 * Coils/discrete inputs: not mapped in this PoC. The function codes are
 * also disabled in port.h, so these are only reached if you enable them
 * there; they answer "no such register" (exception 02).
 */
eMBErrorCode
eMBRegCoilsCB( UCHAR * pucRegBuffer, USHORT usAddress,
               USHORT usNCoils, eMBRegisterMode eMode )
{
    ( void )pucRegBuffer;
    ( void )usAddress;
    ( void )usNCoils;
    ( void )eMode;
    return MB_ENOREG;
}

eMBErrorCode
eMBRegDiscreteCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete )
{
    ( void )pucRegBuffer;
    ( void )usAddress;
    ( void )usNDiscrete;
    return MB_ENOREG;
}
