# lib/ - TCP probe + cliente Modbus TCP + mapa de pontos

Bibliotecas pequenas sobre FreeRTOS-Plus-TCP, sem lwIP, sem HAL e sem
task permanente.

```
lib/
├── tcpprobe/
│   ├── tcpprobe.h / tcpprobe.c
│   ├── tcpprobe.mk
│   └── example.c              (S0, nao compilado)
└── mbtcp/
    ├── mbtcp.h / mbtcp.c      transporte Modbus TCP (todas as FCs)
    ├── mbtcp_codec.h / .c     registro <-> valor (puro C, testavel no PC)
    ├── mbtcp_map.h / .c       tabela de pontos, cache e poll
    ├── mbtcp_demo.c           exemplo UHX65A + autoteste (nao compilado)
    └── mbtcp.mk
```

## Integracao no Makefile

Depois de `C_SOURCES` e `C_INCLUDES`:

```make
include lib/tcpprobe/tcpprobe.mk
include lib/mbtcp/mbtcp.mk
```

`mbtcp.mk` adiciona `mbtcp.c`, `mbtcp_codec.c` e `mbtcp_map.c`, alem de
`-Ilib/mbtcp -Ilib/tcpprobe`. `mbtcp_demo.c` **nao** e' compilado.

## Configuracao e ciclo de vida

```c
mbtcp_cfg_t cfg;

mbtcp_cfg_default(&cfg);       /* port 502, unit 0 (UHX65A) */
cfg.ip = "192.168.10.4";       /* ajuste para a rede real        */
cfg.port = 502;
cfg.unit = 0;
cfg.timeout_ms = 500;
cfg.retries_read = 1;          /* leitura pode repetir           */
cfg.retries_write = 0;         /* escrita NAO repete por default */

if (mbtcp_init(&cfg) == MBTCP_OK) {
    mbtcp_connect();           /* opcional: a 1a transacao conecta */
}

/* ... usa a API ... */

mbtcp_disconnect();
```

`mbtcp_init()` apenas configura; nao abre socket. `mbtcp_connect()` abre
agora. Se a rede cair, a proxima transacao reconecta sozinha.

```c
int mbtcp_init(const mbtcp_cfg_t *cfg);
int mbtcp_connect(void);
void mbtcp_disconnect(void);
int mbtcp_set_unit(uint8_t unit);
const mbtcp_stats_t *mbtcp_stats(void);
```

`mbtcp_stats_t` traz contadores de TX/RX, timeouts, excecoes, erros de
frame, conexoes, retries e latencia da ultima transacao.

## API de function codes

Todas usam o `unit` configurado em `mbtcp_init()`.

| FC | Funcao |
| --- | --- |
| 01 | `mbtcp_read_coils(addr, qty, bits)` |
| 02 | `mbtcp_read_discrete_inputs(addr, qty, bits)` |
| 03 | `mbtcp_read_holding(addr, qty, regs)` |
| 04 | `mbtcp_read_input(addr, qty, regs)` |
| 05 | `mbtcp_write_coil(addr, on)` |
| 06 | `mbtcp_write_register(addr, value)` |
| 15 | `mbtcp_write_coils(addr, qty, bits)` |
| 16 | `mbtcp_write_registers(addr, qty, regs)` |
| 20 | `mbtcp_read_file_record(file, record, qty, regs)` |
| 21 | `mbtcp_write_file_record(file, record, qty, regs)` |
| 22 | `mbtcp_mask_write_register(addr, and_mask, or_mask)` |
| 23 | `mbtcp_read_write_registers(raddr, rqty, rout, waddr, wqty, win)` |
| 24 | `mbtcp_read_fifo(addr, out, cap, &count)` |
| 43/14 | `mbtcp_read_device_id(category, cb, ctx)` / `mbtcp_read_device_id_obj(category, obj, cb, ctx)` |

Para qualquer FC fora dessa lista (inclusive FCs seriais atras de um
gateway), use o PDU cru:

```c
uint8_t pdu[8] = { 0x08, /* ... */ };
uint8_t rsp[253];
uint16_t rsp_len;
int rc = mbtcp_raw_pdu(pdu, sizeof(pdu), rsp, sizeof(rsp), &rsp_len);
```

O PDU de resposta inclui o function code em `rsp[0]`. Excecao Modbus
retorna `MBTCP_EEXCEPTION`; o codigo fica em `mbtcp_last_exception()`.

### Compatibilidade com a versao anterior

Continuam existindo:

```c
int  mbtcp_open(const char *ip, uint16_t port, uint32_t timeout_ms);
void mbtcp_close(void);
int  mbtcp_read(uint8_t unit, uint8_t fc, uint16_t addr, uint16_t qty,
                uint16_t *out);
int  mbtcp_write(uint8_t unit, uint16_t addr, uint16_t value);
int  mbtcp_write_multi(uint8_t unit, uint16_t addr, uint16_t qty,
                       const uint16_t *values);
```

`mbtcp_write_multi()` usa FC16. O UHX65A **nao** garante FC16; para ele
use `mbtcp_read_write_registers()` (FC23) ou o mapa.

## Perfil SEW UHX65A

Premissas adotadas (confirmar no manual/MOVISUITE da maquina):

- unit ID **0**;
- offset de process data **0**;
- FCs **03, 06 e 23**;
- ate **120 palavras** de process data;
- FC16/FC05/FC15/FC43 nao sao assumidas.

O primeiro eixo de exemplo comeca em **1**. O mapa abaixo usa 8 PD por
eixo (modo Positioning):

| Eixo | Palavras | Conteudo de exemplo |
| --- | --- | --- |
| 1 | 1..8 | control, status, setpoint S32, actual S32, reservado |
| 2 | 9..16 | control, status, setpoint S32, actual S32, reservado |

O layout real depende dos modulos MOVIKIT e do mapeamento de process
data feito no MOVISUITE. Ajuste `addr`, `type`, `scale` e `word_order` na
tabela; o transporte nao muda.

### Escrita de 32 bits no UHX65A

Com `MBMAP_USE_FC16=0` (default), o mapa escreve valores de 2 registradores
com **FC23**: le 1 registrador e escreve 2 no mesmo endereco, em uma unica
transacao. Para escravos que suportam FC16, compile com
`-DMBMAP_USE_FC16=1`.

O control word e' tratado como um ponto `U16` inteiro. Nao use
read-modify-write de bit no control word durante operacao.

## Mapa de pontos

A tabela fica no projeto da aplicacao (nao no `lib/`). Exemplo:

```c
enum { A1_CTRL, A1_STATUS, A1_SETPOINT, A1_ACTUAL, P_COUNT };

static const mbpoint_t points[P_COUNT] = {
    /* nome          addr table       type     access   word      bit grp per scale off min max */
    { "a1.control",   1, MBT_HOLDING, MBV_U16, MBA_RW, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 65535.0f },
    { "a1.status",    2, MBT_HOLDING, MBV_U16, MBA_RO, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 0.0f },
    { "a1.setpoint",  3, MBT_HOLDING, MBV_S32, MBA_RW, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, -1000000.0f, 1000000.0f },
    { "a1.actual",    5, MBT_HOLDING, MBV_S32, MBA_RO, MBW_HI_LO, 0, 1, 20, 1.0f, 0.0f, 0.0f, 0.0f },
};

static mbgroup_t groups[] = {
    { 1, MBT_HOLDING, 1, 8, 20, 0 },   /* id, tabela, addr, qty, periodo */
};

static mbval_t cache[P_COUNT];
static mbmap_t map;

mbmap_init(&map, points, P_COUNT, groups, 1, cache);
```

Macros:

- tabela: `MBT_HOLDING`, `MBT_INPUT`, `MBT_COIL`, `MBT_DISCRETE`;
- tipo: `MBV_U16`, `MBV_S16`, `MBV_U32`, `MBV_S32`, `MBV_F32`, `MBV_BIT`;
- acesso: `MBA_RO`, `MBA_RW`, `MBA_WO`;
- ordem de 32 bits: `MBW_HI_LO` (AB CD) ou `MBW_LO_HI` (CD AB).

Uso:

```c
int id = mbmap_id(&map, "a1.actual");
mbval_t v;

mbmap_read(&map, id, &v);              /* le agora e atualiza o cache */
mbmap_read_cached(&map, id, &v);       /* so copia o cache           */

mbmap_write(&map, mbmap_id(&map, "a1.setpoint"), 1234.0f);

mbmap_poll_group(&map, 1);             /* le o bloco 1..8 em uma FC3 */
mbmap_dump(&map);                      /* loga os pontos validos     */

mbmap_wait(&map, id, 0x0001u, 0x0001u, 2000);
```

`min < max` valida a escrita em valor de engenharia. `scale`/`offset`
convertem `eng = raw * scale + offset`. Bit dentro de registrador
(`MBV_BIT` em `MBT_HOLDING`) usa read-modify-write com FC03+FC06; nao e'
atomico.

### Task de processo

O mapa nao cria task. Copie esta ideia para uma task que ja existe:

```c
for (;;) {
    int rc = mbmap_tick(&map);         /* poll dos grupos vencidos */

    if (rc != MBTCP_OK) {
        /* nao pare o loop: a proxima volta reconecta */
        tcpprobe_log("modbus: %s", mbtcp_strerror(rc));
    }

    {
        mbval_t v;
        if (mbmap_read_cached(&map, A1_ACTUAL, &v) == MBTCP_OK && v.valid) {
            /* publica no MQTT / atualiza estado */
        }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}
```

O periodo de cada grupo (`period_ms`) e' o minimo; a cadencia real
depende de com que frequencia a task chama `mbmap_tick()`. Chamar a cada
10..20 ms e' suficiente. Nao ha task, fila nem comando interno escondido.

### Concorrencia e bloqueio

- As funcoes de `mbtcp.c` sao thread-safe (mutex interno).
- O mapa usa buffers de scratch estaticos e **nao** e' reentrante. Use
  `mbmap_tick()`, `mbmap_read()` e `mbmap_write()` em **uma unica task**
  (a task de processo). Outras tasks devem ler o cache com
  `mbmap_read_cached()` ou enviar comandos por uma fila para a task de
  processo.
- `mbmap_tick()`, `mbmap_read()` e `mbmap_write()` bloqueiam durante a
  transacao. Com a rede caida, o pior caso e' aproximadamente
  `timeout_ms` por grupo. Nunca chame do callback do MQTT, de timer ou
  de callback de interrupcao.
- `mbtcp_connect()` e `mbtcp_disconnect()` tambem bloqueiam; chame da
  task de processo.

## Memoria

- transporte: `s_tx[260] + s_rx[260] + s_rsp[253]` = **773 bytes** estaticos;
- mapa: `s_regs[125]` (250 B) + `s_bits[250]` (250 B) = **500 bytes**;
- cache: `sizeof(mbval_t)` por ponto (tipicamente 20..24 B);
- mutex: alocado no heap do FreeRTOS;
- socket: consome `RX + TX` do heap do FreeRTOS (tipicamente 2 KB + 2 KB).

Se `FreeRTOS_socket()` falhar, veja `tcpprobe_env_dump()` e
`mbtcp_stats()`. `cfg.rx_buf`/`cfg.tx_buf` permitem reduzir os buffers
do socket, mas teste a maior PDU usada (FC3 de 125 registradores = 259
bytes de frame).

## Validacao no PC (sem SEW)

Um comando faz tudo:

```sh
tools/run_validation.sh
```

Ele sobe dois simuladores, compila o teste host do transporte/mapa,
testa o codec e roda checagens Python. O teste host usa `tools/host/`
(mocks de FreeRTOS e sockets POSIX) e exercita o **mbtcp.c real**, nao
so o Python.

Manual:

```sh
# terminal 1 - perfil UHX65A
python3 tools/sew_sim.py --port 1503 --profile uhx65a --unit 0

# terminal 2
python3 tools/mbtest.py 127.0.0.1 1503 0 3 1 8
python3 tools/mbtest.py 127.0.0.1 1503 0 6 1 1
python3 tools/mbtest.py 127.0.0.1 1503 0 23 1 8 1 1 0 0 0 0 0 0 0

# perfil generico (todas as FCs)
python3 tools/sew_sim.py --port 1504 --profile generic --unit 1
python3 tools/mbtest.py 127.0.0.1 1504 1 1 0 16
python3 tools/mbtest.py 127.0.0.1 1504 1 16 20 1 2 3
python3 tools/mbtest.py 127.0.0.1 1504 1 22 20 0xFF00 0x0001
python3 tools/mbtest.py 127.0.0.1 1504 1 24 0x3000
python3 tools/mbtest.py 127.0.0.1 1504 1 43 2
```

Para descobrir unit/endereco no bring-up:

```sh
python3 tools/scan.py 127.0.0.1 1503 --units 0 --addr 1 --range --end 16
```

O simulador UHX65A:

- aceita somente FC03, FC06 e FC23;
- responde excecao 01 para qualquer FC fora de FC03/FC06/FC23
  (FC16, FC05, FC15, FC43, ...);
- valida enderecos dentro de `--pd-start .. --pd-start+--pd-count-1`;
- simula dois eixos: control bit 0 liga o eixo e faz `actual = setpoint`;
- liga o bit 15 do status apos `--fieldbus-timeout` ms sem escrita;
- permite injetar latencia (`--latency-ms`), perda (`--drop-rate`) e
  excecao (`--fault-fc`).

## Bring-up no UHX65A real

Ordem recomendada, sem escrita ate' o mapeamento estar confirmado:

1. **V0** - ping/ARP no controlador, confirme IP e mascara.
2. **V1** - `mbtcp_connect()` e `tcpprobe_env_dump()`.
3. **V2** - `mbtcp_read_holding(1, 16, regs)` somente leitura.
4. **V3** - `mbmap_poll_group()` somente leitura por varios minutos;
   acompanhe `mbtcp_stats()` (timeouts, frame errors, latencia).
5. **V4** - primeira escrita: control word inteiro, com valor seguro;
   so depois setpoint/parametros.

Se V2 der excecao 02, o offset/area de PD esta diferente do suposto. Se
der excecao 01, a FC nao existe nesse firmware/mapeamento.

## Migracao do codigo atual

- Remova `demo_init()` do callback do MQTT. Ele nao cria task, mas faz
  I/O de rede e pode bloquear o callback.
- Prefira `mbtcp_init()` + `mbtcp_connect()` + `mbmap_tick()` em uma task
  de processo.
- O `mbtcp` antigo continua compilando; `mbtcp_open()`/`mbtcp_read()`/
  `mbtcp_write()`/`mbtcp_write_multi()` estao preservados.
- Se o app tinha um loop de leitura com FC03, ele pode ser substituido
  por um grupo do mapa, reduzindo o numero de transacoes.

## Macros uteis

| Macro | Efeito |
| --- | --- |
| `-DMBTCP_TRACE=0` | silencia hex de TX/RX |
| `-DMBMAP_USE_FC16=1` | mapa escreve 32 bits com FC16 em vez de FC23 |
| `-DMBTCP_MUTEX_TIMEOUT_MS=...` | tempo maximo esperando o socket |
| `-DTCPPROBE_HAS_ISNETWORKUP=0` | se a versao do stack nao tiver a funcao |
| `-DTCPPROBE_HAS_MIN_HEAP=0` | se nao usar heap_4/heap_5 |

Exemplo: `C_DEFS += -DMBTCP_TRACE=0 -DMBMAP_USE_FC16=0`.
