# lib/ - TCP probe + Modbus TCP client

Duas bibliotecas pequenas, sem dependencia de lwIP/HAL, sobre
FreeRTOS-Plus-TCP.

```
lib/
├── tcpprobe/
│   ├── tcpprobe.h
│   ├── tcpprobe.c
│   ├── tcpprobe.mk
│   └── example.c        (S0, nao compilado)
└── mbtcp/
    ├── mbtcp.h
    ├── mbtcp.c
    ├── mbtcp.mk
    └── mbtcp_demo.c     (S3, nao compilado)
```

## Integracao no Makefile

Depois das definicoes de `C_SOURCES` e `C_INCLUDES`:

```make
include lib/tcpprobe/tcpprobe.mk
include lib/mbtcp/mbtcp.mk
```

`tcpprobe.mk` adiciona `tcpprobe.c` e `-Ilib/tcpprobe`.
`mbtcp.mk` adiciona `mbtcp.c` e `-Ilib/mbtcp -Ilib/tcpprobe`.

## API

### tcpprobe

```c
void tcpprobe_set_logger(void (*fn)(const char *line));  /* NULL = default */
void tcpprobe_log(const char *fmt, ...);
void tcpprobe_log_hex(const char *tag, const void *buf, unsigned len);
void tcpprobe_env_dump(void);

int tcpprobe_tcp_client(const char *ip, uint16_t port,
                        const void *tx, uint16_t txlen,
                        void *rx, uint16_t rxcap,
                        uint32_t timeout_ms);
```

`tcpprobe_env_dump()` mostra heap livre/minimo, numero de tasks, link,
buffers TCP e MTU. E' o primeiro lugar para olhar quando `socket()`
falha.

### mbtcp

```c
int  mbtcp_open(const char *ip, uint16_t port, uint32_t timeout_ms);
void mbtcp_close(void);

int  mbtcp_read(uint8_t unit, uint8_t fc, uint16_t addr,
                uint16_t qty, uint16_t *out);        /* FC03/FC04 */
int  mbtcp_write(uint8_t unit, uint16_t addr, uint16_t value);  /* FC06 */
int  mbtcp_write_multi(uint8_t unit, uint16_t addr,
                       uint16_t qty, const uint16_t *values);   /* FC16 */

int  mbtcp_last_exception(void);
const char *mbtcp_strerror(int err);
```

Retornos: `MBTCP_OK` (0) ou negativo (`MBTCP_ECONN`, `MBTCP_ESEND`,
`MBTCP_ETIMEOUT`, `MBTCP_EFRAME`, `MBTCP_EEXCEPTION`, `MBTCP_EBUSY`,
`MBTCP_EINVAL`).

## Pontos de projeto

- **Um socket, um mutex.** Nao cria task permanente; usa a task que ja
  existe (ex.: a do MQTT).
- **Buffers estaticos** de 260 bytes para TX/RX. O mutex serializa, entao
  nao precisa de ~520 bytes de stack por chamador.
- **Leitura por tamanho exato**: le 6 bytes de MBAP, depois `length`
  bytes. Nunca "chuta" tamanho.
- **Ressincronizacao**: em timeout/erro de frame o socket e' fechado; a
  proxima chamada reconecta. Excecao Modbus nao fecha.
- **Coexistencia RTU**: mantenha o transporte antigo e troque apenas a
  funcao de leitura/escrita por `mbtcp_read`/`mbtcp_write`.

## Configuracoes uteis

Em `FreeRTOSIPConfig.h`:

```c
#define ipconfigUSE_TCP              1
#define ipconfigTCP_RX_BUFFER_LENGTH 2048
#define ipconfigTCP_TX_BUFFER_LENGTH 2048
```

Cada socket TCP consome ~`RX + TX` bytes do heap do FreeRTOS. Se
`FreeRTOS_socket()` retorna `FREERTOS_INVALID_SOCKET`, o heap acabou:
reduza os buffers ou aumente `configTOTAL_HEAP_SIZE`.

## Validacao (PC, sem SEW)

```sh
# terminal 1
python3 tools/sew_sim.py --port 1502 --unit 1

# terminal 2
python3 tools/mbtest.py 127.0.0.1 1502 1 3 0 4
python3 tools/mbtest.py 127.0.0.1 1502 1 6 5 1234
python3 tools/scan.py 127.0.0.1 1502 --units 1-5
```

No STM32, a escada e' S0 (TCP cru) -> S1/S2 (bytes) -> S3 (leitura) ->
S4 (escrita) -> S5 (MQTT + Modbus) -> S6 (SEW real, porta 502).
