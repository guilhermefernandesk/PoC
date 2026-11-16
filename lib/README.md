# lib/ - cliente Modbus TCP + mapa de pontos

Bibliotecas pequenas sobre FreeRTOS-Plus-TCP, sem lwIP, sem HAL e sem task permanente.

```
lib/
├── tcpprobe/
│   ├── tcpprobe.h / tcpprobe.c
│   ├── tcpprobe.mk
│   └── example.c              (exemplo, não compilado)
└── mbtcp/
    ├── mbtcp.h / mbtcp.c      transporte Modbus TCP (FC03/06/16/23 + PDU cru)
    ├── mbtcp_codec.h / .c     registro <-> valor (puro C, testável no PC)
    ├── mbtcp_map.h / .c       tabela de pontos, cache e poll
    ├── mbtcp_demo.c           exemplo UHX65A (não compilado)
    └── mbtcp.mk
```

## Perfil alvo

O cliente e o mapa foram simplificados para o perfil **SEW UHX65A**:

- Unit ID `0`
- Offset `0`
- FC03: leitura de holding registers
- FC06: escrita de um holding register
- FC23: leitura e escrita na mesma transação (usada para valores de 32 bits)
- FC16: mantida apenas por compatibilidade (`mbtcp_write_multi`); o UHX65A não garante FC16
- PDU cru: escape hatch para function codes não cobertas pela API

O layout real de process data (quais palavras são controle, status, setpoint e posição/velocidade real) vem do MOVISUITE/MOVIKIT. Ajuste `address`, `type`, `scale` e `word_order` na tabela de pontos antes de qualquer escrita no equipamento real.

## Integração no Makefile

Depois de `C_SOURCES` e `C_INCLUDES`:

```make
include lib/tcpprobe/tcpprobe.mk
include lib/mbtcp/mbtcp.mk
```

`mbtcp.mk` adiciona `mbtcp.c`, `mbtcp_codec.c` e `mbtcp_map.c`, além de `-Ilib/mbtcp -Ilib/tcpprobe`. `mbtcp_demo.c` **não** é compilado.

## Uso rápido

```c
#include "mbtcp.h"
#include "mbtcp_map.h"

#include "FreeRTOS.h"
#include "task.h"

enum {
    POINT_AXIS1_CONTROL = 0,
    POINT_AXIS1_STATUS,
    POINT_AXIS1_POSITION_SETPOINT,
    POINT_AXIS1_POSITION_ACTUAL,
    POINT_COUNT
};

static const mbpoint_t g_points[POINT_COUNT] = {
    [POINT_AXIS1_CONTROL] = {
        .name = "axis1.control",
        .address = 1,
        .type = MBV_U16,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1,
        .scale = 1.0f,
        .offset = 0.0f,
        .minimum = 0.0f,
        .maximum = 65535.0f,
    },
    [POINT_AXIS1_STATUS] = {
        .name = "axis1.status",
        .address = 2,
        .type = MBV_U16,
        .access = MBA_RO,
        .word_order = MBW_HI_LO,
        .group = 1,
        .scale = 1.0f,
        .offset = 0.0f,
    },
    [POINT_AXIS1_POSITION_SETPOINT] = {
        .name = "axis1.position_setpoint",
        .address = 3,
        .type = MBV_S32,
        .access = MBA_RW,
        .word_order = MBW_HI_LO,
        .group = 1,
        .scale = 1.0f,
        .offset = 0.0f,
        .minimum = -1000000.0f,
        .maximum = 1000000.0f,
    },
    [POINT_AXIS1_POSITION_ACTUAL] = {
        .name = "axis1.position_actual",
        .address = 5,
        .type = MBV_S32,
        .access = MBA_RO,
        .word_order = MBW_HI_LO,
        .group = 1,
        .scale = 1.0f,
        .offset = 0.0f,
    },
};

static mbgroup_t g_groups[] = {
    { .id = 1, .address = 1, .quantity = 8, .period_ms = 20 },
};

static mbval_t g_cache[POINT_COUNT];
static mbmap_t g_map;

void modbus_task(void *argument)
{
    mbtcp_cfg_t cfg;

    (void)argument;

    mbtcp_cfg_default(&cfg);
    cfg.ip = "192.168.10.4";   /* IP do UHX65A ou do simulador */
    cfg.port = 502;            /* 502 no UHX65A; 1502 no simulador */
    cfg.unit = 0;              /* UHX65A: 0 */
    cfg.timeout_ms = 800;
    cfg.retries_read = 1;

    if (mbtcp_init(&cfg) != MBTCP_OK) {
        return;
    }
    if (mbmap_init(&g_map, g_points, POINT_COUNT, g_groups, 1, g_cache) != MBTCP_OK) {
        return;
    }

    for (;;) {
        int rc = mbmap_tick(&g_map);
        if (rc != MBTCP_OK) {
            /* logar e manter telemetria anterior inválida/antiga conforme política */
        }

        /* escrita de posição: ver seção "Fluxo para escrever posição" */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**Importante:** `mbtcp_init`/`mbtcp_connect`/leituras/escritas podem bloquear por timeout/rede. Nunca chame essas funções do callback do MQTT; use uma task de processo e uma fila/comando para pedir a escrita.

## Fluxo para escrever posição (Modbus TCP)

Exemplo: escrever `125` no setpoint de posição do eixo 1 (`POINT_AXIS1_POSITION_SETPOINT`, endereço `3`, `S32`, `scale = 1.0`, `offset = 0.0`).

### 1. Aplicação pede a escrita

```c
int rc = mbmap_write(&g_map, POINT_AXIS1_POSITION_SETPOINT, 125.0f);
if (rc != MBTCP_OK) {
    /* MBTCP_EINVAL, MBTCP_ETIMEOUT, MBTCP_EEXCEPTION, ... */
}
```

### 2. Validações do mapa

`mbmap_write` verifica, nesta ordem:

1. mapa/ponto válido e ponto não é somente leitura (`MBA_RO`);
2. valor finito (`NaN`/infinito rejeitado);
3. valor dentro de `minimum`/`maximum`;
4. codificação do valor de engenharia para registradores (`raw = (value - offset) / scale`, com arredondamento e limite do tipo).

Para `125.0f`, `S32`, `scale = 1.0`, `offset = 0.0` e `word_order = MBW_HI_LO`, os registradores ficam:

- register `3` = `0x0000` (high word)
- register `4` = `0x007D` (low word)

### 3. Seleção da function code

- valor de 16 bits (`MBV_U16`/`MBV_S16`) -> **FC06** (`Write Single Register`)
- valor de 32 bits (`MBV_U32`/`MBV_S32`/`MBV_F32`) -> **FC23** (`Read/Write Multiple Registers`)

O mapa usa FC23 para 32 bits porque o UHX65A não garante FC16 para o process data. A transação lê 1 register e escreve 2 registers no mesmo endereço.

> **Premissa a validar no equipamento real:** a implementação assume que o UHX65A aceita FC23 com leitura e escrita no mesmo endereço. Se o equipamento rejeitar essa sobreposição, use a PDU crua/uma sequência de FC06 ou ajuste o mapeamento antes de escrever.

A sequência de controle (habilitar, selecionar modo, escrever setpoint, disparar movimento) e os bits exatos do control word vêm do MOVISUITE/MOVIKIT. Este cliente transporta os valores; ele não inventa o protocolo de movimento.

### 4. Montagem do frame Modbus TCP

Para FC23 em `address = 3`, `read_quantity = 1`, `write_quantity = 2` e dados `00 00 00 7D`:

| Campo | Valor | Bytes |
|---|---:|---|
| Transaction ID | `0x0001` (incremental) | `00 01` |
| Protocol ID | `0x0000` | `00 00` |
| Length | `0x000F` (Unit + PDU) | `00 0F` |
| Unit ID | `0x00` | `00` |
| Function | `0x17` (FC23) | `17` |
| Read address | `0x0003` | `00 03` |
| Read quantity | `0x0001` | `00 01` |
| Write address | `0x0003` | `00 03` |
| Write quantity | `0x0002` | `00 02` |
| Write byte count | `0x04` | `04` |
| Write data | `0x0000007D` | `00 00 00 7D` |

Frame completo (21 bytes):

```
00 01 00 00 00 0F 00 17 00 03 00 01 00 03 00 02 04 00 00 00 7D
```

### 5. Envio, resposta e tratamento de erro

O transporte:

1. garante socket conectado (conecta no primeiro uso ou reconecta se caiu);
2. envia o frame inteiro dentro do timeout;
3. lê o cabeçalho MBAP, valida `length` e lê a PDU de resposta;
4. valida Transaction ID, Protocol ID, Unit ID e function code;
5. se receber exceção Modbus (`function | 0x80`), retorna `MBTCP_EEXCEPTION` e guarda o código em `mbtcp_last_exception()`;
6. em timeout/erro de frame, fecha o socket para ressincronizar.

Resposta FC23 típica (lê 1 register):

```
00 01 00 00 00 05 00 17 02 00 00
```

- `17`: FC23
- `02`: 2 bytes lidos
- `00 00`: valor lido no endereço `3` (descartado pela escrita de 32 bits do mapa)

### 6. Atualização do cache

Se a transação for aceita, `mbmap_write` decodifica os registradores enviados e atualiza `g_cache[POINT_AXIS1_POSITION_SETPOINT]` (`valid`, `timestamp`, `raw_signed`, `raw_unsigned`, `value`). Leituras cíclicas posteriores via `mbmap_tick`/`mbmap_poll_group` atualizam o valor real/estado conforme o mapeamento.

### Exemplo equivalente com 16 bits (FC06)

Para um ponto `U16` no endereço `3` com valor `125`:

```
request:  00 01 00 00 00 06 00 06 00 03 00 7D
response: 00 01 00 00 00 06 00 06 00 03 00 7D
```

## API do transporte

```c
void mbtcp_cfg_default(mbtcp_cfg_t *cfg);
int  mbtcp_init(const mbtcp_cfg_t *cfg);
int  mbtcp_connect(void);
void mbtcp_disconnect(void);
int  mbtcp_set_unit(uint8_t unit);
const mbtcp_stats_t *mbtcp_stats(void);

int mbtcp_read_holding(uint16_t address, uint16_t quantity, uint16_t *registers);
int mbtcp_write_register(uint16_t address, uint16_t value); /* FC06 */
int mbtcp_read_write_registers(uint16_t read_address, uint16_t read_quantity,
                               uint16_t *read_registers,
                               uint16_t write_address, uint16_t write_quantity,
                               const uint16_t *write_registers); /* FC23 */
int mbtcp_raw_pdu(const uint8_t *request_pdu, uint16_t request_pdu_length,
                  uint8_t *response_pdu, uint16_t response_capacity,
                  uint16_t *response_pdu_length);
```

Compatibilidade legada preservada:

- `mbtcp_open`, `mbtcp_close`
- `mbtcp_read(unit, function_code, address, quantity, registers)` (somente FC03)
- `mbtcp_write(unit, address, value)` (FC06)
- `mbtcp_write_multi(unit, address, quantity, registers)` (FC16, legado)

### Configuração

| Campo | Default | Observação |
|---|---:|---|
| `ip` | — | obrigatório |
| `port` | 502 | 502 UHX65A, 1502 simulador |
| `unit` | 0 | UHX65A usa 0 |
| `timeout_ms` | 1000 | limitado a 60000 ms |
| `retries_read` | 0 | leituras podem repetir em timeout/conexão/envio; nunca em exceção/frame inválido |
| `nodelay` | 1 | TCP_NODELAY |
| `rx_buf` / `tx_buf` | 0 | 0 = default do stack |

`mbtcp_init()` copia a string de IP, então `cfg.ip` pode apontar para buffer temporário. Escritas não são repetidas automaticamente.

### Erros e diagnóstico

```c
int rc = mbmap_write(&g_map, POINT_AXIS1_POSITION_SETPOINT, 125.0f);
if (rc == MBTCP_EEXCEPTION) {
    int ex = mbtcp_last_exception();
    /* ex = 1 illegal function, 2 illegal address, 3 illegal value, ... */
}
```

`mbtcp_strerror(rc)` e `mbtcp_exception_str(code)` retornam textos curtos para log.

`mbtcp_stats()` expõe contadores (`tx_count`, `rx_count`, `timeouts`, `exceptions`, `frame_errors`, `connect_errors`, `connects`, `retries`, `latency_ms`, `last_error`, `connected`).

## API do mapa

- `MBV_U16`, `MBV_S16`, `MBV_U32`, `MBV_S32`, `MBV_F32`
- `MBA_RO`, `MBA_RW`, `MBA_WO`
- `MBW_HI_LO`, `MBW_LO_HI`
- `MBMAP_MAX_REGISTERS = 125`

Cada ponto possui `address`, `type`, `access`, `word_order`, `group`, `scale`, `offset`, `minimum` e `maximum`. Pontos graváveis devem ter `minimum < maximum`; o mapa rejeita valores fora da faixa e valores não finitos.

`mbgroup_t` define o bloco de leitura cíclica (`address`, `quantity`, `period_ms`). Um grupo lê no máximo 125 registers. `mbmap_tick` retorna o primeiro erro de poll, mas continua processando os demais grupos vencidos.

Valores em cache (`mbval_t`): `valid`, `type`, `timestamp`, `raw_signed`, `raw_unsigned`, `value`. Uma leitura ou poll que falha marca o cache correspondente como inválido (`valid = 0`); valores decodificados não finitos (`NaN`/infinito) também ficam inválidos.

## Bring-up seguro (UHX65A)

1. **Simulador**: valide com `tools/sew_sim.py --profile uhx65a --unit 0 --port 1502` e `bash tools/run_validation.sh`.
2. **Leitura no equipamento real**: configure `cfg.ip/port/unit` e o mapeamento real; execute somente `mbmap_tick`/leituras e confirme status/valores antes de escrever.
3. **Primeira escrita**: com drive em estado seguro, escreva o setpoint/control word correto para o layout MOVISUITE, monitore exceções, status e atual.
4. **Integração**: comande escritas por fila/comando a partir da task de processo; não faça I/O Modbus no callback MQTT.

## Validação no PC

Pré-requisito: Python 3, `gcc` e `make`.

```bash
bash tools/run_validation.sh
```

O script executa:

- `tools/sew_sim.py --profile uhx65a --unit 0`
- `tools/host/test_mbtcp` (transporte + mapa, FC03/06/23, rejeição de FC16)
- `tools/test_codec.c`
- `tools/mbtest.py` (oráculo Python independente para FC03/FC06/FC23 e rejeição de FC16)
- `tools/host/test_faults_asan` (MBAP inválido, transação errada, exceção, timeout, retry, resposta parcial e reconexão, com ASan/UBSan)
- `tools/host/test_mbtcp_asan` (transporte/mapa contra o simulador, com ASan/UBSan)

Compilação manual dos testes:

```bash
make -C tools/host
ASAN_OPTIONS=detect_leaks=0 ./tools/host/test_faults
ASAN_OPTIONS=detect_leaks=0 ./tools/host/test_faults_asan
gcc -std=c99 -Wall -Wextra -Werror -Ilib/mbtcp \
    tools/test_codec.c lib/mbtcp/mbtcp_codec.c -lm -o /tmp/test_codec
```

## Memória

`mbtcp.c` usa buffers estáticos: `s_tx_frame[260]`, `s_rx_frame[260]` e estado de sessão (~600 bytes). `mbtcp_map.c` usa `s_registers[125]` (250 bytes) para leitura de bloco. O transporte é protegido por mutex interno; o mapa assume uso por uma task (proteja com mutex da aplicação se múltiplas tasks chamarem o mapa).

## Observações

- `MBTCP_TRACE` é `0` por padrão; defina `1` para logar TX/RX em hexadecimal via `tcpprobe`.
- Leituras usam o campo `length` do MBAP para determinar o tamanho exato da resposta.
- `mbtcp_raw_pdu` permite usar function codes específicas do equipamento sem aumentar a API pública.
- Valide sempre no simulador e em modo somente leitura antes da primeira escrita no UHX65A real.
