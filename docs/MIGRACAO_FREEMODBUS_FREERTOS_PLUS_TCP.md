# Migração para FreeModbus TCP — STM32H743ZIT6 (FreeRTOS-Plus-TCP)

**Alvo:** projeto hand-made, sem HAL, FreeRTOS + CMSIS-OS v1, rede via
**FreeRTOS-Plus-TCP**, build por **Makefile**.

**Objetivo desta etapa:** copiar o core do FreeModbus, criar a camada de port
(TCP/FreeRTOS-Plus-TCP), registrar um mapa de teste e validar a comunicação
com `mbpoll`/cliente Modbus TCP antes de integrar o mapa real do PLC.

**Base analisada:** <https://github.com/cwalter-at/freemodbus> — master no
commit `a9ec789` ("Keep handlers reachable after callback removal"). A API
usada aqui é estável desde a 1.6.0.

**Referência validada:** este guia é a versão para FreeRTOS-Plus-TCP do
código que já roda neste repositório (PoC). Aqui o mesmo desenho foi
validado no host com `tools/modbus_test.py` contra `ports/posix/` e
compilado para o alvo com `ports/lwip/` (netconn). Use estes arquivos como
referência cruzada:

| Guia (seu projeto) | Neste repositório (validado) |
| --- | --- |
| `freemodbus_port/port.h` | `ports/lwip/port.h` (idêntico) |
| `freemodbus_port/portevent.c` | `ports/lwip/portevent.c` (idêntico) |
| `freemodbus_port/porttcp.c` | `ports/posix/porttcp.c` (sockets Linux) |
| `app_regs.c/h` | `app/app_regs.c/h` |
| task no `main.c` | `target/app/main.c` (`modbus_task`) |

---

## 0. Resumo do que será feito

1. Copiar a pasta `modbus/` do FreeModbus (licença BSD) para o projeto.
2. Criar `freemodbus_port/` com 3 arquivos: `port.h`, `portevent.c`,
   `porttcp.c`.
3. Adicionar as fontes e includes no Makefile.
4. Criar um mapa de registradores de teste (`app_regs.c/h`) com os
   callbacks do FreeModbus.
5. Criar a task Modbus no `main.c` (CMSIS-OS v1).
6. Validar com `mbpoll` (leitura, escrita e exceções).

O que **não** copiar do repositório FreeModbus:

- `demo/` → aplicações de exemplo são **GPL**;
- ports de referência (`demo/*/port/`) → **LGPL**;
- `doc/` (opcional, apenas documentação).

Só a pasta `modbus/` interessa, e ela é **BSD** (uso livre em firmware
proprietário).

> **Importante:** a pasta `modbus/` sozinha **não compila nem linka**. O core
> chama funções de port (`xMBTCPPortInit`, `xMBPortEventInit`, etc.) que não
> existem nela. É por isso que o passo 2 é obrigatório.

---

## 1. Copiar o core

No seu computador:

```bash
git clone --depth 1 https://github.com/cwalter-at/freemodbus /tmp/freemodbus

mkdir -p <seu-projeto>/third_party/freemodbus
cp -r /tmp/freemodbus/modbus <seu-projeto>/third_party/freemodbus/
```

Estrutura final no projeto:

```
<seu-projeto>/
├── third_party/
│   └── freemodbus/
│       └── modbus/          <- copiado intacto (BSD)
│           ├── mb.c
│           ├── include/*.h
│           ├── functions/*.c
│           ├── rtu/         <- pode ficar, não será compilado
│           ├── ascii/       <- pode ficar, não será compilado
│           └── tcp/mbtcp.c
├── freemodbus_port/         <- NOVO (criado manualmente, ver passo 2)
│   ├── port.h
│   ├── portevent.c
│   └── porttcp.c
├── app_regs.c / app_regs.h  <- NOVO (mapa de teste, ver passo 4)
└── main.c                   <- alterado (ver passo 5)
```

Arquivos do core que **entram no build** (modo TCP) — mesma lista usada no
PoC (`target/poc_sources.cmake`):

```text
modbus/mb.c
modbus/functions/mbfunccoils.c
modbus/functions/mbfuncdiag.c
modbus/functions/mbfuncdisc.c
modbus/functions/mbfuncfile.c
modbus/functions/mbfuncholding.c
modbus/functions/mbfuncinput.c
modbus/functions/mbfuncother.c
modbus/functions/mbutils.c
modbus/tcp/mbtcp.c
```

Observações:

- `modbus/rtu/*` e `modbus/ascii/*` ficam de fora (o modo será TCP). Se o
  seu Makefile usa wildcard e compilar tudo, não há problema: esses
  arquivos são protegidos por `#if MB_RTU_ENABLED` / `#if MB_ASCII_ENABLED`
  e geram objetos vazios.
- `modbus/functions/mbfuncdiag.c` deste fork contém apenas o cabeçalho de
  licença (FC08/diagnóstico não é suportado); `mbfuncfile.c` só ganha corpo
  se habilitar *file records* (padrão: desabilitado). Compilá-los mesmo
  assim é inofensivo.
- `modbus/rtu/mbcrc.c` não é necessário no modo TCP.
- O core precisa do include `modbus/tcp` além de `modbus/include` (`mb.c`
  inclui `mbtcp.h`).

---

## 2. Camada de port (criar manualmente)

### 2.1 `freemodbus_port/port.h`

Este arquivo é incluído pelo core **antes** de `mbconfig.h` (confirmado em
`mb.c` e `mbtcp.c`), então as macros de configuração definidas aqui
prevalecem — e o core fica 100% intacto. É cópia exata do
`ports/lwip/port.h` do PoC.

```c
/*
 * FreeModbus port configuration (STM32H7, FreeRTOS + FreeRTOS-Plus-TCP).
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
 * Function codes enabled. Coils/discrete inputs stay disabled so those
 * codes answer MB_EX_ILLEGAL_FUNCTION (01), like the previous hand-written
 * core did. Flip a bit here and implement the matching callback in
 * app_regs.c to enable one.
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
 * Cortex-M: mask all interrupts (PRIMASK). Sections are tiny and never
 * nested with RTOS critical sections, so plain cpsid/cpsie is enough and
 * keeps the port RTOS agnostic. Se preferir usar o CMSIS, troque por
 * __disable_irq()/__enable_irq().
 */
#define ENTER_CRITICAL_SECTION(  )  __asm__ volatile ( "cpsid i" ::: "memory" )
#define EXIT_CRITICAL_SECTION(  )   __asm__ volatile ( "cpsie i" ::: "memory" )

#define PR_BEGIN_EXTERN_C extern "C" {
#define PR_END_EXTERN_C }
#define PR_BEGIN_IGNORE_UNUSED_PARA
#define PR_END_IGNORE_UNUSED_PARA

#define INLINE
#define assert( x )

#endif /* PORT_H */
```

Ponto crítico: com `MB_FUNC_WRITE_COIL_ENABLED = 0`, um FC05 chega como
**função ilegal (exceção 01)** — mesmo comportamento do núcleo artesanal
anterior. Se você habilitar coils e o callback não mapear coils, o FC05
passa a responder exceção **02** (endereco ilegal) em vez de 01.

> **Atenção com `eMBSetSlaveID()`:** ela só é compilada quando
> `MB_FUNC_OTHER_REP_SLAVEID_ENABLED > 0`. Com FC17 desabilitado, chamar
> `eMBSetSlaveID()` no seu `main.c` dá **erro de link**. Não chame (a
> função é opcional e só afeta o FC17).

### 2.2 `freemodbus_port/portevent.c`

Sem fila de RTOS. O I/O acontece dentro de `xMBPortEventGet()` (chamado
pelo `eMBPoll()`), o que mantém tudo em uma única task e torna o port
independente de CMSIS-OS/FreeRTOS. É cópia exata do
`ports/lwip/portevent.c` do PoC.

```c
/*
 * FreeModbus event glue (RTOS independent).
 *
 * FreeModbus calls xMBPortEventGet() from eMBPoll(). When no event is
 * pending we run the TCP pool (accept/recv) right here, so all Modbus I/O
 * happens in the single task that calls eMBPoll(). No queues and no extra
 * tasks: the same shape works on POSIX and on the FreeRTOS target.
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
```

### 2.3 `freemodbus_port/porttcp.c`

Port TCP sobre a API de sockets do **FreeRTOS-Plus-TCP**. Um cliente por
vez, framing MBAP completo (trata frames fragmentados e múltiplos frames
na mesma leitura), sem heap próprio e sem depender de HAL. O framing é
idêntico ao do PoC (`ports/posix/porttcp.c`), só trocando a API de socket.

```c
/*
 * FreeModbus TCP port for FreeRTOS-Plus-TCP.
 *
 * One client at a time. MBAP framing (fragmented and coalesced frames),
 * MBAP length validation and the Unit ID filter are implemented here
 * because the FreeModbus TCP core uses the pseudo address 255 and accepts
 * any unit id. eMBPoll() drives accept/recv through xMBPortTCPPool(), so
 * the application is a plain "for (;;) eMBPoll();" task.
 */

#include "port.h"

#include "mb.h"
#include "mbport.h"

#include <string.h>

#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "task.h"

/* MBAP header offsets (inside aucTCPBuf). */
#define MB_TCP_TID                0
#define MB_TCP_PID                2
#define MB_TCP_LEN                4
#define MB_TCP_UID                6
#define MB_TCP_FUNC               7

#define MB_TCP_PROTOCOL_ID        0
#define MB_TCP_MAX_ADU_LENGTH     260 /* 7 byte MBAP + max 253 byte PDU */
#define MB_TCP_MAX_PDU_LENGTH     254 /* sanity bound for the MBAP field */
#define MB_TCP_DEFAULT_PORT       502

static Socket_t xListenSocket;
static Socket_t xClientSocket;

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
    if( ( xClientSocket != NULL ) &&
        ( xClientSocket != FREERTOS_INVALID_SOCKET ) )
    {
        ( void )FreeRTOS_closesocket( xClientSocket );
    }
    xClientSocket = NULL;
    prvFrameReset(  );
}

static void
prvSetSocketTimeouts( Socket_t xSocket )
{
    TickType_t xTimeout = pdMS_TO_TICKS( MB_PORT_TCP_POOL_TIMEOUT_MS );

    ( void )FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO,
                                 &xTimeout, sizeof( xTimeout ) );
    ( void )FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SNDTIMEO,
                                 &xTimeout, sizeof( xTimeout ) );
}

BOOL
xMBTCPPortInit( USHORT usTCPPort )
{
    struct freertos_sockaddr xAddress;

    if( usTCPPort == 0 )
    {
        usTCPPort = MB_TCP_DEFAULT_PORT;
    }

    if( ( xListenSocket != NULL ) &&
        ( xListenSocket != FREERTOS_INVALID_SOCKET ) )
    {
        ( void )FreeRTOS_closesocket( xListenSocket );
    }

    xListenSocket = FreeRTOS_socket( FREERTOS_AF_INET,
                                     FREERTOS_SOCK_STREAM,
                                     FREERTOS_IPPROTO_TCP );
    if( ( xListenSocket == NULL ) ||
        ( xListenSocket == FREERTOS_INVALID_SOCKET ) )
    {
        xListenSocket = NULL;
        return FALSE;
    }

    /* Timeout do accept (a task não fica bloqueada para sempre). */
    prvSetSocketTimeouts( xListenSocket );

    ( void )memset( &xAddress, 0, sizeof( xAddress ) );
    xAddress.sin_family = FREERTOS_AF_INET;
    xAddress.sin_port   = FreeRTOS_htons( usTCPPort );
    xAddress.sin_addr   = FreeRTOS_htonl( 0 );   /* qualquer interface */
    xAddress.sin_len    = ( uint8_t ) sizeof( xAddress );

    if( FreeRTOS_bind( xListenSocket, &xAddress,
                       sizeof( xAddress ) ) != 0 )
    {
        ( void )FreeRTOS_closesocket( xListenSocket );
        xListenSocket = NULL;
        return FALSE;
    }

    if( FreeRTOS_listen( xListenSocket, 1 ) != 0 )
    {
        ( void )FreeRTOS_closesocket( xListenSocket );
        xListenSocket = NULL;
        return FALSE;
    }

    xClientSocket = NULL;
    prvFrameReset(  );
    return TRUE;
}

void
vMBTCPPortClose( void )
{
    prvClientClose(  );
    if( ( xListenSocket != NULL ) &&
        ( xListenSocket != FREERTOS_INVALID_SOCKET ) )
    {
        ( void )FreeRTOS_closesocket( xListenSocket );
    }
    xListenSocket = NULL;
}

void
vMBTCPPortDisable( void )
{
    prvClientClose(  );
}

static BOOL
prvAcceptClient( void )
{
    Socket_t xNewClient;

    xNewClient = FreeRTOS_accept( xListenSocket, NULL, NULL );
    if( ( xNewClient == NULL ) ||
        ( xNewClient == FREERTOS_INVALID_SOCKET ) )
    {
        return FALSE;           /* timeout / sem conexão pendente */
    }

    /* Timeout de recepção (dados) e de envio (buffer cheio). */
    xClientSocket = xNewClient;
    prvSetSocketTimeouts( xClientSocket );

    /* One client at a time; a second client waits in the backlog. */
    prvFrameReset(  );
    return TRUE;
}

BOOL
xMBPortTCPPool( void )
{
    BaseType_t iReceived;
    USHORT     usLength;
    USHORT     usTotal;

    if( ( xListenSocket == NULL ) ||
        ( xListenSocket == FREERTOS_INVALID_SOCKET ) )
    {
        return FALSE;
    }

    if( xClientSocket == NULL )
    {
        ( void )prvAcceptClient(  );
        return TRUE;
    }

    /* Read at most what the current frame still needs: coalesced frames
     * stay in the socket buffer for the next pool call. */
    iReceived = FreeRTOS_recv( xClientSocket,
                               &aucTCPBuf[usTCPBufPos],
                               ( size_t ) usTCPFrameBytesLeft, 0 );
    if( iReceived > 0 )
    {
        usTCPBufPos = ( USHORT ) ( usTCPBufPos + ( USHORT ) iReceived );
        usTCPFrameBytesLeft = ( USHORT ) ( usTCPFrameBytesLeft - ( USHORT ) iReceived );

        if( usTCPBufPos < MB_TCP_FUNC )
        {
            usTCPFrameBytesLeft = ( USHORT ) ( MB_TCP_FUNC - usTCPBufPos );
            return TRUE;
        }

        usLength = ( USHORT ) ( ( ( USHORT ) aucTCPBuf[MB_TCP_LEN] << 8 ) | aucTCPBuf[MB_TCP_LEN + 1] );

        if( ( usLength < 2u ) || ( usLength > MB_TCP_MAX_PDU_LENGTH ) )
        {
            prvClientClose(  ); /* malformed MBAP length */
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
                /* Not for this slave: drop the frame, keep the
                 * connection. */
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
    }
    else if( iReceived < 0 )
    {
        /* Erro real: fecha. Nas versões 3/4 o fechamento pelo cliente
         * chega como -pdFREERTOS_ERRNO_ENOTCONN; EINVAL/ENOMEM também são
         * fatais. -EWOULDBLOCK (versões antigas) é apenas timeout. */
        if( iReceived != -pdFREERTOS_ERRNO_EWOULDBLOCK )
        {
            prvClientClose(  );
        }
    }
    else
    {
        /* iReceived == 0: NÃO é fechamento nas versões 3/4 — é o timeout
         * do recv (socket sem dados). Só fecha se o socket realmente
         * não estiver mais conectado (cobre versões antigas, em que o
         * fechamento podia retornar 0). */
        if( FreeRTOS_issocketconnected( xClientSocket ) == pdFALSE )
        {
            prvClientClose(  );
        }
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
    BaseType_t rc;

    if( xClientSocket == NULL )
    {
        return FALSE;
    }

    while( sent < ( size_t ) usTCPLength )
    {
        rc = FreeRTOS_send( xClientSocket, &pucMBTCPFrame[sent],
                            ( size_t ) usTCPLength - sent, 0 );
        if( rc <= 0 )
        {
            prvClientClose(  );
            return FALSE;
        }
        sent += ( size_t ) rc;
    }
    return TRUE;
}
```

### Notas de implementação (importantes)

- **Fluxo de eventos:** `eMBPoll()` chama `xMBPortEventGet()`. Sem evento,
  ele chama `xMBPortTCPPool()`, que faz `accept`/`recv` (bloqueando até
  50 ms) e posta `EV_FRAME_RECEIVED` quando o frame MBAP está completo.
  São ~3 chamadas de `eMBPoll()` por requisição (receber → validar UID →
  executar/enviar). Isso é normal.
- **O buffer é o mesmo para recepção e resposta:** `mbtcp.c` atualiza o
  campo `length` no mesmo buffer e envia a partir dele. Não chame `recv`
  entre o evento de recepção e o envio — o desenho acima garante isso
  porque o pool só roda quando não há evento pendente. O TID/PID/UID da
  resposta são ecoados do request pelo próprio core.
- **Semântica do `FreeRTOS_recv` (verificada na fonte V3.1.0 e main/V4):**
  - retorno `> 0` → bytes recebidos;
  - retorno `0` → **timeout** (nenhum dado no intervalo), **não**
    fechamento;
  - retorno `-pdFREERTOS_ERRNO_ENOTCONN` → cliente fechou a conexão (ou
    erro fatal);
  - retorno `-pdFREERTOS_ERRNO_EWOULDBLOCK` → timeout em versões antigas
    (2.x).

  Por isso o código só fecha em erro negativo e usa
  `FreeRTOS_issocketconnected()` no caso `0`.
- **Campo do endereço (compatibilidade de versões):** o código usa
  `xAddress.sin_addr`, que existe na V2.x e na V3/V4 com
  `ipconfigIPv4_BACKWARD_COMPATIBLE == 1` (padrão). Se o seu stack for V4
  com `ipconfigIPv4_BACKWARD_COMPATIBLE == 0`, troque por
  `xAddress.sin_address.ulIP_IPv4 = FreeRTOS_htonl( 0 );`.
- **`FreeRTOS_socket()`/`FreeRTOS_accept()` em falha:** versões diferentes
  retornam `NULL` ou `FREERTOS_INVALID_SOCKET` (`(Socket_t)~0U`); o código
  trata os dois casos.
- **`FreeRTOS_setsockopt` tem 5 argumentos** (`socket, level, option,
  value, size`) — não confundir com o BSD, que não tem `level`.
- **Um cliente por vez:** o `accept` só é chamado quando não há cliente
  conectado; conexões pendentes ficam no backlog (1) e são aceitas quando
  o cliente atual fecha. É o mesmo comportamento do PoC.
- **Filtro de Unit ID** está no port (o core TCP usa endereço pseudo 255 e
  aceitaria qualquer UID).
- **FreeRTOS-Plus-TCP exige alocação dinâmica:**
  `configSUPPORT_DYNAMIC_ALLOCATION` deve ser `1` (a API de sockets usa
  `pvPortMalloc`), e o `FreeRTOS_IPInit()`/interface deve estar de pé antes
  da task Modbus rodar (o `eMBTCPInit` cria o socket dentro da task, com o
  scheduler já rodando).
- Se a sua versão do FreeRTOS-Plus-TCP retornar outro código de erro no
  lugar de `pdFREERTOS_ERRNO_EWOULDBLOCK`, confira
  `FreeRTOS_errno_TCP.h`.

---

## 3. Makefile

Se o seu Makefile for no estilo CubeMX (`C_SOURCES`/`C_INCLUDES`),
acrescente:

```make
FREEMODBUS_DIR := third_party/freemodbus/modbus

C_SOURCES += \
$(FREEMODBUS_DIR)/mb.c \
$(FREEMODBUS_DIR)/functions/mbfunccoils.c \
$(FREEMODBUS_DIR)/functions/mbfuncdiag.c \
$(FREEMODBUS_DIR)/functions/mbfuncdisc.c \
$(FREEMODBUS_DIR)/functions/mbfuncfile.c \
$(FREEMODBUS_DIR)/functions/mbfuncholding.c \
$(FREEMODBUS_DIR)/functions/mbfuncinput.c \
$(FREEMODBUS_DIR)/functions/mbfuncother.c \
$(FREEMODBUS_DIR)/functions/mbutils.c \
$(FREEMODBUS_DIR)/tcp/mbtcp.c \
freemodbus_port/portevent.c \
freemodbus_port/porttcp.c \
app_regs.c

C_INCLUDES += \
-I$(FREEMODBUS_DIR)/include \
-I$(FREEMODBUS_DIR)/tcp \
-Ifreemodbus_port
```

Se o Makefile descobre fontes por wildcard, basta garantir que
`third_party/freemodbus/modbus` e `freemodbus_port` estejam nas listas de
diretórios.

Pontos de atenção:

- O core faz `#include "port.h"` / `#include "mb.h"` / `#include "mbtcp.h"`
  — `-I$(FREEMODBUS_DIR)/include`, `-I$(FREEMODBUS_DIR)/tcp` e
  `-Ifreemodbus_port` são obrigatórios.
- Se o seu projeto já tem um `port.h`, garanta que `-Ifreemodbus_port`
  venha **antes** desse diretório (o nome do arquivo é fixo no core).
- O core é C89/C90 e compila sem problemas em `-std=gnu11` (padrão da
  maioria dos Makefiles STM32).
- Os headers do FreeRTOS-Plus-TCP (`FreeRTOS_Sockets.h` etc.) já devem
  estar no `C_INCLUDES` do projeto.

---

## 4. Mapa de registradores de teste

O FreeModbus **não armazena registradores**: ele chama callbacks da
aplicação. Crie `app_regs.h` e `app_regs.c`.

> **Convenção de endereços (validada no PoC):** o PDU Modbus é base 0 no
> fio, mas o FreeModbus entrega ao callback endereços **base 1** — ele
> soma 1 antes de chamar (`usRegAddress++` em `mbfuncholding.c` /
> `mbfuncinput.c`). PLC `40001` (PDU 0) chega no callback como
> `usAddress == 1`. O código abaixo converte de volta para índice base 0
> dos arrays; sem essa conversão, leituras "andam" um registrador.

### `app_regs.h`

```c
#ifndef APP_REGS_H
#define APP_REGS_H

/*
 * Demo register map. Replace this file with the real PLC mapping later.
 *
 * Register arrays are zero-based (holding[0] is PLC 40001). The FreeModbus
 * callbacks translate the 1-based protocol addresses they receive
 * (PDU address N -> callback N + 1), so callers see plain 0-based PDU
 * addresses.
 */

#define APP_REGS_HOLDING_COUNT 100u
#define APP_REGS_INPUT_COUNT   100u

void app_regs_init( void );

/* Opcional: prova de vida (chamar 1x por segundo de outra task). */
void app_regs_heartbeat( void );

#endif
```

### `app_regs.c`

```c
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

void
app_regs_heartbeat( void )
{
    usInputRegs[0]++;    /* muda a cada segundo: prova de vida */
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
 * Coils/discrete inputs: not mapped. The function codes are also disabled
 * in port.h, so these are only reached if you enable them there; they
 * answer "no such register" (exception 02).
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
```

Convenções:

- No fio, PDU é base 0: PLC `40001` → PDU 0 → callback `usAddress = 1`
  (o código converte de volta).
- Valores de 16 bits são serializados **big-endian** (igual à PoC).
- Quantidades fora dos limites (0 ou > 125 para leitura, > 123 para
  FC16) são rejeitadas pelo **core** com exceção 03 antes de chegar ao
  callback.
- Se outras tasks escreverem nos arrays, proteja o acesso (mutex
  CMSIS-OS v1 ou seção crítica na aplicação).

---

## 5. `main.c` (CMSIS-OS v1)

A task do Modbus deve ser criada depois do kernel/`FreeRTOS_IPInit` (o
`eMBTCPInit` cria o socket dentro da task, com o scheduler já rodando).

```c
#include "cmsis_os.h"
#include "mb.h"
#include "app_regs.h"
#include "port.h"

static void modbus_task( void const *argument )
{
    ( void )argument;

    /* FreeModbus lifecycle: init (binds the port) -> enable -> poll.
     * All TCP I/O happens inside eMBPoll() (freemodbus_port/porttcp.c). */
    while( eMBTCPInit( MB_PORT_TCP_LISTEN_PORT ) != MB_ENOERR )
    {
        osDelay( 1000 );
    }

    while( eMBEnable( ) != MB_ENOERR )
    {
        osDelay( 1000 );
    }

    for( ;; )
    {
        if( eMBPoll( ) != MB_ENOERR )
        {
            /* Should not happen; restart the stack if it does. */
            ( void )eMBDisable( );
            osDelay( 1000 );
            ( void )eMBEnable( );
        }
    }
}

static void app_task( void const *argument )
{
    ( void )argument;
    for( ;; )
    {
        app_regs_heartbeat( );
        osDelay( 1000 );
    }
}

osThreadDef( modbus, modbus_task, osPriorityNormal, 0, 4096 );
osThreadDef( app,    app_task,    osPriorityLow,    0, 512 );

int main( void )
{
    /* ... seu init: clock, GPIO, FreeRTOS_IPInit/netif ... */

    app_regs_init( );

    osKernelInitialize( );                 /* se ainda não inicializou */
    osThreadCreate( osThread( modbus ), NULL );
    osThreadCreate( osThread( app ),    NULL );
    osKernelStart( );                      /* se ainda não iniciou */

    for( ;; ) { }
}
```

Observações:

- **Não chame `eMBSetSlaveID()`** com a configuração recomendada (FC17
  desabilitado): a função não é compilada e o link falha. Ela só é
  necessária para o FC17 (report slave ID).
- `osThreadDef(..., stacksz)` no CMSIS-OS v1 é em **bytes** (4096 é
  folgado; 2048 costuma bastar).
- Só **uma** task pode chamar `eMBPoll()`. O stack não é thread-safe.
- Se o seu main já inicializa o kernel em outro lugar, basta criar
  `osThread(osThread(modbus), NULL)` lá.
- Se preferir não usar a API CMSIS para a task, o port não depende dela:
  um `xTaskCreate` nativo também funciona (é o que o PoC faz em
  `target/app/main.c`).

---

## 6. Validação

### 6.1 Checklist antes de testar

1. A placa responde `ping`.
2. `configSUPPORT_DYNAMIC_ALLOCATION` = 1.
3. `FreeRTOS_IPInit` executado e interface com IP configurado.
4. Porta 502 livre (nenhum outro listener).
5. Firewall do PC não bloqueia a porta 502.

### 6.2 Testes com `mbpoll`

```bash
# porta TCP aberta?
nc -vz 192.168.1.10 502

# Ler 10 holding registers (40001..40010) uma vez
mbpoll -m tcp -a 1 -t 4 -r 1 -c 10 -1 192.168.1.10 -p 502

# Escrever 1234 no 40001
mbpoll -m tcp -a 1 -t 4 -r 1 192.168.1.10 -p 502 1234

# Ler de novo (deve mostrar 1234 no primeiro)
mbpoll -m tcp -a 1 -t 4 -r 1 -c 10 -1 192.168.1.10 -p 502

# Ler input registers (30001..30010) — o primeiro deve estar incrementando
mbpoll -m tcp -a 1 -t 3 -r 1 -c 10 -1 192.168.1.10 -p 502
```

`-t 4` = holding (FC03), `-t 3` = input (FC04).
Sem `-a` o mbpoll usa o Unit ID padrão (1).

### 6.3 Testes de exceção

```bash
# Endereço fora do mapa (101 > 100 registradores) -> exceção 02 (illegal data address)
mbpoll -m tcp -a 1 -t 4 -r 101 -1 192.168.1.10 -p 502

# Quantidade inválida (0) -> exceção 03 (illegal data value)
# (mbpoll não envia quantidade 0; use um script/ferramenta se precisar)

# Unit ID diferente (com filtro ativo) -> sem resposta (timeout)
mbpoll -m tcp -a 9 -t 4 -r 1 -1 192.168.1.10 -p 502

# FC05 (coil) com coils desabilitados -> exceção 01 (illegal function)
mbpoll -m tcp -a 1 -t 0 -r 1 -1 192.168.1.10 -p 502
```

### 6.4 Resultado esperado

- leitura retorna os valores iniciais (`0..9` holding; `1000..1009`
  input);
- escrita altera o valor e a leitura seguinte confirma;
- input[0] muda a cada segundo (heartbeat);
- endereço inválido gera resposta de exceção, não timeout;
- UID diferente do configurado é ignorado;
- FC01/02/05/15 respondem exceção 01 (função ilegal).

> **Bateria completa:** o PoC valida framing fragmentado/agrupado,
> exceções e filtro de Unit ID com `tools/modbus_test.py` contra o servidor
> POSIX (`ports/posix`). O script é black-box (só sockets) e funciona
> contra a placa também: `python3 tools/modbus_test.py --host
> 192.168.1.10 --port 502`.

---

## 7. Troubleshooting

| Sintoma | Causa provável | Ação |
|---|---|---|
| Erro de link: `xMBTCPPortInit`, `xMBPortEventInit`… | port não compilado/incluído | Confira as fontes em `C_SOURCES` e os includes de `freemodbus_port` |
| Erro de link: `eMBSetSlaveID` | FC17 desabilitado no `port.h` | Remova a chamada (ou habilite `MB_FUNC_OTHER_REP_SLAVEID_ENABLED`) |
| Erro de link: `mbtcp.h` não encontrado | falta `-I$(FREEMODBUS_DIR)/tcp` | Acrescente o include |
| `eMBTCPInit` retorna `MB_EPORTERR` | `FreeRTOS_socket`/`bind`/`listen` falhou | Verifique heap dinâmico, IP configurado, porta em uso |
| Aceita conexão mas não responde | Unit ID diferente | Ajuste `MB_PORT_TCP_UNIT_ID` ou `MB_PORT_TCP_ACCEPT_ANY_UNIT=1` |
| Leitura "anda" um registrador (40001 mostra o valor do 40002) | callback sem a conversão base 1 → base 0 | Converta com `usIndex = usAddress - 1` (ver passo 4) |
| Resposta com valores trocados (ex.: 0x0102 virou 0x0201) | endianness no callback | Leia/escreva como nos exemplos (big-endian) |
| Cliente conecta e cai a cada ~50 ms | `recv` retornando `0` tratado como fechamento | Nas versões V3/V4, `0` é timeout. Feche apenas em erro negativo (ou se `FreeRTOS_issocketconnected()==pdFALSE`) |
| `mbpoll` dá timeout no 40001 | mapa/limite dos callbacks | Confira `usAddress - 1 + usNRegs` e o retorno `MB_ENOREG` |
| Task trava (watchdog) | `eMBPoll` chamado de mais de uma task, ou pool sem timeout | Use uma única task; mantenha o timeout do socket |
| `FreeRTOS_socket` retorna erro | `configSUPPORT_DYNAMIC_ALLOCATION = 0` | Habilite alocação dinâmica |

---

## 8. Limitações e diferenças em relação à PoC TCP anterior

| Item | Comportamento |
|---|---|
| Clientes simultâneos | 1 por vez (backlog 1); novo cliente só após o atual fechar |
| Instância do stack | Singleton: uma instância por firmware; TCP **ou** RTU, não ambos |
| Unit ID | O core ignora; o filtro é feito no port (`MB_PORT_TCP_UNIT_ID`) |
| FCs | 03/04/06/16 habilitados; demais respondem **exceção 01** (ligue em `port.h` + callback se precisar). FC08 (diagnóstico) não existe neste fork |
| Endereços | PDU base 0 no fio; callbacks recebem **base 1** (converter no callback) |
| Persistência | Nenhuma; registradores voltam ao inicial no reboot |
| Validação MBAP | `protocol id` é validado pelo core; o `length` e o framing são feitos pelo port |
| Thread safety | Só a task do `eMBPoll` acessa o stack; o mapa da aplicação precisa de proteção própria |
| Licença | Core BSD; os 3 arquivos de port são novos (licença do seu projeto) |

---

## 9. Checklist final de integração

- [ ] `third_party/freemodbus/modbus/` copiado (core BSD, intacto).
- [ ] `freemodbus_port/port.h`, `portevent.c`, `porttcp.c` criados.
- [ ] Fontes e includes adicionados ao Makefile.
- [ ] `app_regs.c/h` criados e callbacks implementados (atenção à
      conversão base 1 → base 0).
- [ ] Task `modbus` criada no `main.c` (CMSIS-OS v1) com
      `eMBTCPInit`/`eMBEnable`/`eMBPoll`.
- [ ] `ping` OK e porta 502 aberta.
- [ ] `mbpoll` lê/escreve holding e lê input.
- [ ] Exceções respondem corretamente.
- [ ] Mapa real do PLC implementado em `app_regs.c` (depois da validação).

---

## Apêndice A — Licenças

- `modbus/` (core): BSD 3-cláusulas — pode ser usado em firmware
  proprietário mantendo o aviso de copyright.
- `demo/` e ports de referência do repositório: GPL/LGPL — **não
  copiados**.
- `freemodbus_port/` e `app_regs.*`: código novo, seguem a licença do seu
  projeto.

## Apêndice B — Variante LwIP

Este repositório já contém o port LwIP pronto e compilado para o alvo em
`ports/lwip/`:

- `port.h` e `portevent.c` são **idênticos** aos deste guia;
- `porttcp.c` usa `netconn_new/bind/listen/accept/recv_tcp_pbuf/write`
  no lugar das `FreeRTOS_*` (backlog do `netconn_listen`, timeouts via
  `netconn_set_recvtimeout`);
- requer `LWIP_NETCONN=1` e `LWIP_SO_RCVTIMEO=1` no `lwipopts.h` (ver
  `target/net/lwipopts.h`).

Todo o restante (callbacks, task, validação) permanece igual.
