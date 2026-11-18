# lib/ - transporte Modbus TCP + driver UHX15A

Bibliotecas pequenas sobre FreeRTOS-Plus-TCP, sem lwIP, sem HAL e sem task
permanente.

```
lib/
├── tcpprobe/
│   ├── tcpprobe.h / tcpprobe.c   log de TX/RX
│   └── tcpprobe.mk
├── mbtcp/
│   ├── mbtcp.h / mbtcp.c         transporte Modbus TCP (FC03/06/23 + PDU cru)
│   └── mbtcp.mk
└── uhx15a/
    ├── uhx15a.h                  API da aplicacao (graus, status, enable)
    ├── uhx15a_config.h           TODA a configuracao do UHX15A
    ├── uhx15a.c                  driver
    └── uhx15a.mk
```

O mapa de pontos (`mbtcp_map`), o codec (`mbtcp_codec`), o demo e os
wrappers legados foram removidos. A aplicacao nao monta registradores nem
toca em enderecos: usa apenas `uhx15a.h`.

## Perfil alvo

O driver foi feito para o **SEW UHX15A**:

- Unit ID `0`
- Offset `0`
- FC03 para leitura de holding registers
- FC06 para escrita pontual de um register
- FC23 para ler e escrever o setpoint de 32 bits na mesma transacao
- FC16 nao faz parte da API publica; o perfil `uhx15a` do simulador a rejeita
- `mbtcp_raw_pdu` continua disponivel para bancada/function codes futuras

O layout real de process data vem do MOVISUITE/MOVIKIT. O mapa abaixo e'
**provisorio**, usado para validar o transporte e o fluxo do driver no
simulador.

| Eixo | Control | Status | Setpoint (S32) | Actual (S32) |
|---|---:|---:|---:|---:|
| Azimute | 1 | 2 | 3..4 | 5..6 |
| Elevacao | 9 | 10 | 11..12 | 13..14 |

- Tipo provisorio: `S32`, high word primeiro (`HI_LO`).
- Escala provisoria: `1.0`; offset: `0.0`.
- O driver le o bloco `1..16` em uma unica FC03.
- O simulador inicia os dois eixos com `control = 1` para o primeiro teste.
- `uhx15a_set_target()` escreve apenas o setpoint; nao habilita nem dispara
  movimento.
- No simulador, FC06 so aceita as control words (`1`, `9`) e FC23 so aceita
  os setpoints (`3..4`, `11..12`). Status, actual e reservados sao somente
  leitura e respondem excecao `02`.

## Configuracao

Tudo que e' configuracao do equipamento fica em `lib/uhx15a/uhx15a_config.h`:

```c
#ifndef UHX15A_IP
#define UHX15A_IP "192.168.10.4"
#endif

#ifndef UHX15A_PORT
#define UHX15A_PORT 502u
#endif

#ifndef UHX15A_UNIT_ID
#define UHX15A_UNIT_ID 0u
#endif

#define UHX15A_POLL_START  1u
#define UHX15A_POLL_COUNT  16u

#define UHX15A_AZ_CONTROL  1u
#define UHX15A_AZ_STATUS   2u
#define UHX15A_AZ_TARGET   3u
#define UHX15A_AZ_ACTUAL   5u

#define UHX15A_EL_CONTROL  9u
#define UHX15A_EL_STATUS   10u
#define UHX15A_EL_TARGET   11u
#define UHX15A_EL_ACTUAL   13u

#define UHX15A_POSITION_SCALE   1.0f
#define UHX15A_POSITION_OFFSET  0.0f

#define UHX15A_AZ_MIN_DEG  (-180.0f)
#define UHX15A_AZ_MAX_DEG  ( 180.0f)
#define UHX15A_EL_MIN_DEG  ( -90.0f)
#define UHX15A_EL_MAX_DEG  (  90.0f)

#define UHX15A_STATUS_ENABLED           0x0001u
#define UHX15A_STATUS_FIELDBUS_TIMEOUT  0x8000u
```

O header tambem configura timeout, tentativas de leitura, buffers, TCP_NODELAY
e limites. O `uhx15a.c` contem apenas codigo.

Os `#ifndef` permitem sobrescrever IP/porta/unit no teste host sem editar o
header:

```bash
gcc ... -DUHX15A_IP='"127.0.0.1"' -DUHX15A_PORT=1503
```

O header tem checagens de compilacao para garantir que os enderecos usados
cabem em `UHX15A_POLL_COUNT`.

## Integracao no Makefile

Depois de `C_SOURCES` e `C_INCLUDES`:

```make
include lib/tcpprobe/tcpprobe.mk
include lib/mbtcp/mbtcp.mk
include lib/uhx15a/uhx15a.mk
```

`uhx15a.mk` adiciona `uhx15a.c` e `-Ilib/uhx15a`. `mbtcp.mk` adiciona o
transporte e `-Ilib/mbtcp -Ilib/tcpprobe`.

## Uso na aplicacao

```c
#include "uhx15a.h"

#include "FreeRTOS.h"
#include "task.h"

static void control_task(void *argument)
{
    (void)argument;

    if (uhx15a_init() != UHX15A_OK) {
        /* logar uhx15a_last_error() */
        return;
    }

    for (;;) {
        uhx15a_result_t rc = uhx15a_poll();

        if (rc != UHX15A_OK) {
            /* link/timeout/protocolo: telemetria fica invalida */
        } else if (uhx15a_enabled(UHX15A_AXIS_AZIMUTH)) {
            float az = uhx15a_position_deg(UHX15A_AXIS_AZIMUTH);
            float el = uhx15a_position_deg(UHX15A_AXIS_ELEVATION);
            (void)az;
            (void)el;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

Comando de posicao:

```c
uhx15a_result_t rc;

rc = uhx15a_set_target(UHX15A_AXIS_AZIMUTH, 45.0f);
if (rc != UHX15A_OK) {
    /* UHX15A_ERR_RANGE, UHX15A_ERR_TIMEOUT, UHX15A_ERR_MODBUS, ... */
}
```

API publica:

```c
uhx15a_result_t uhx15a_init(void);
uhx15a_result_t uhx15a_poll(void);
uhx15a_result_t uhx15a_set_target(uhx15a_axis_t axis, float position_deg);

bool     uhx15a_online(void);
bool     uhx15a_enabled(uhx15a_axis_t axis);
bool     uhx15a_fieldbus_timeout(uhx15a_axis_t axis);
float    uhx15a_position_deg(uhx15a_axis_t axis);
uint16_t uhx15a_status_word(uhx15a_axis_t axis);
const char *uhx15a_last_error(void);
void     uhx15a_close(void);
```

Unidades da aplicacao: **graus** e **graus/s** quando a velocidade for
adicionada. O driver converte para registradores internamente.

## Migracao do mapa antigo

### Antes

```c
#include "mbtcp_map.h"

mbtcp_cfg_t cfg;
mbtcp_cfg_default(&cfg);
cfg.ip = "192.168.10.4";
cfg.port = 502;
cfg.unit = 0;
mbtcp_init(&cfg);

mbmap_init(&g_map, g_points, POINT_COUNT, g_groups, 1, g_cache);

for (;;) {
    mbmap_tick(&g_map);

    mbval_t position;
    mbmap_read_cached(&g_map, POINT_AXIS1_POSITION_ACTUAL, &position);
    float degrees = position.value;

    vTaskDelay(pdMS_TO_TICKS(20));
}

/* comando */
mbmap_write(&g_map, POINT_AXIS1_POSITION_SETPOINT, 125.0f);
```

### Depois

```c
#include "uhx15a.h"

uhx15a_init();

for (;;) {
    uhx15a_poll();

    float degrees = uhx15a_position_deg(UHX15A_AXIS_AZIMUTH);
    bool enabled = uhx15a_enabled(UHX15A_AXIS_AZIMUTH);

    (void)degrees;
    (void)enabled;

    vTaskDelay(pdMS_TO_TICKS(20));
}

/* comando */
uhx15a_set_target(UHX15A_AXIS_AZIMUTH, 125.0f);
```

### Tabela de equivalencia

| Antes (mapa) | Depois (driver) |
|---|---|
| `mbmap_init(&g_map, ...)` | `uhx15a_init()` |
| `mbmap_tick(&g_map)` | `uhx15a_poll()` |
| `mbmap_write(..., POINT_AXIS1_POSITION_SETPOINT, 125.0f)` | `uhx15a_set_target(UHX15A_AXIS_AZIMUTH, 125.0f)` |
| `mbmap_read_cached(..., POINT_AXIS1_POSITION_ACTUAL, &v); v.value` | `uhx15a_position_deg(UHX15A_AXIS_AZIMUTH)` |
| `v.valid == 0` apos falha de poll | `!uhx15a_online()` |
| ponto de status / bits | `uhx15a_status_word()`, `uhx15a_enabled()`, `uhx15a_fieldbus_timeout()` |
| `mbtcp_open(ip, port, timeout)` | `uhx15a_init()` + config no header |
| `mbtcp_close()` | `uhx15a_close()` |
| `mbtcp_read(unit, 0x03, ...)` | `mbtcp_fc(0x03, ...)` se realmente precisar do transporte |
| `mbtcp_write(unit, address, value)` | `mbtcp_fc(0x06, address, 1, &value)` |
| `mbtcp_write_multi(...)` (FC16) | removido; use FC23/`mbtcp_read_write_registers` ou `mbtcp_raw_pdu` em bancada |

### Removidos

- `mbtcp_map.h/.c`, `mbtcp_codec.h/.c`, `mbtcp_demo.c`
- `mbtcp_open`, `mbtcp_close`
- `mbtcp_read`, `mbtcp_write`, `mbtcp_write_multi`
- FC16 especializada (`write_multiple_registers`)

O protocolo e a validacao de conexao foram preservados: MBAP, transaction ID,
unit ID, timeout/reconexao, excecoes Modbus, stats e mutex.

## Transporte `mbtcp`

A aplicacao normalmente nao inclui `mbtcp.h`. Ele existe para o driver,
testes e bancada.

```c
int  mbtcp_connect(void);
void mbtcp_disconnect(void);
bool mbtcp_is_connected(void);

int mbtcp_fc(uint8_t fc, uint16_t address, uint16_t quantity,
             uint16_t *registers);
```

- `fc == 0x03`: le `quantity` registers para `registers`.
- `fc == 0x06`: escreve `registers[0]`; `quantity` deve ser `1`.
- Outras FCs retornam `MBTCP_EINVAL`.

Exemplos:

```c
uint16_t status;

if (mbtcp_fc(0x03, 2u, 1u, &status) == MBTCP_OK) {
    /* status do azimute (mapa provisorio) */
}

uint16_t control = 1u;
if (mbtcp_fc(0x06, 1u, 1u, &control) == MBTCP_OK) {
    /* habilita o control word do azimute (mapa provisorio) */
}
```

Para FC23:

```c
uint16_t setpoint[2] = { 0x0000u, 0x002Du }; /* 45 em S32 HI_LO */
uint16_t echo;

mbtcp_read_write_registers(3u, 1u, &echo, 3u, 2u, setpoint);
```

`mbtcp_raw_pdu` permite qualquer function code sem aumentar a API publica.
Use apenas em bancada ou quando o mapa real do UHX15A for confirmado.

### Configuracao do transporte

| Campo | Default | Observacao |
|---|---:|---|
| `ip` | — | obrigatorio |
| `port` | 502 | 502 no UHX15A; 1503 no simulador |
| `unit` | 0 | UHX15A usa 0 |
| `timeout_ms` | 1000 | limitado a 60000 ms |
| `retries_read` | 0 | leituras podem repetir em timeout/conexao/envio; nunca em excecao/frame invalido |
| `nodelay` | 1 | TCP_NODELAY |
| `rx_buf` / `tx_buf` | 0 | 0 = default do stack |

`mbtcp_init()` copia a string de IP. Escritas nunca sao repetidas
automaticamente; FC23 tambem nao repete.

### Erros e diagnostico

```c
if (uhx15a_poll() != UHX15A_OK) {
    const char *text = uhx15a_last_error();
    const mbtcp_stats_t *stats = mbtcp_stats();
    (void)text;
    (void)stats;
}
```

`mbtcp_last_exception()`, `mbtcp_strerror()` e `mbtcp_exception_str()` continuam
disponiveis para log. `mbtcp_stats()` expoe `tx_count`, `rx_count`, `timeouts`,
`exceptions`, `frame_errors`, `connect_errors`, `connects`, `retries`,
`latency_ms`, `last_error` e `connected`.

## MQTT: comando sem bloquear o callback

O callback do MQTT nao pode chamar `uhx15a_poll()` nem
`uhx15a_set_target()`: essas funcoes fazem I/O Modbus e podem bloquear por
timeout/rede. O callback apenas enfileira o comando; uma task de processo
executa o driver.

```c
typedef struct {
    uhx15a_axis_t axis;
    float degrees;
} move_command_t;

static QueueHandle_t s_move_commands;

/* callback MQTT: sem I/O */
static void on_move_command(const move_command_t *command)
{
    if (xQueueSend(s_move_commands, command, 0) != pdPASS) {
        /* fila cheia: descartar ou sinalizar */
    }
}

/* task de processo: unica dona do driver */
static void process_task(void *argument)
{
    move_command_t command;

    (void)argument;

    uhx15a_init();

    for (;;) {
        if (xQueueReceive(s_move_commands, &command, 0) == pdPASS) {
            (void)uhx15a_set_target(command.axis, command.degrees);
        }
        (void)uhx15a_poll();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

O repositorio nao traz uma implementacao MQTT; a ponte acima e' o contrato
recomendado.

## Bring-up seguro (UHX15A)

1. **Simulador**: rode `bash tools/run_validation.sh` e confirme o fluxo
   completo.
2. **Leitura no equipamento real**: ajuste `UHX15A_IP`, `UHX15A_PORT`,
   `UHX15A_UNIT_ID` e os enderecos no `uhx15a_config.h`. Execute somente
   `uhx15a_init()` e `uhx15a_poll()`.
3. **Confirme o mapa**: status, enable, fieldbus timeout, tipo, escala, ordem
   das palavras e o comportamento de FC23 com leitura/escrita no mesmo
   endereco. Nao escreva enquanto isso nao estiver confirmado.
4. **Primeira escrita**: use somente `uhx15a_set_target()` com o drive em
   estado seguro. O comando atual nao habilita nem dispara movimento.
5. **Integracao**: comandos por fila/task, nunca no callback MQTT.

## Validacao no PC

Pre-requisito: Python 3, `gcc` e `make`.

```bash
bash tools/run_validation.sh
# outra porta:
UHX_PORT=1603 bash tools/run_validation.sh
```

O script executa:

- `tools/sew_sim.py --profile uhx15a --unit 0`
- `tools/host/test_uhx15a` (driver + transporte)
- `tools/mbtest.py` (oraculo Python independente para FC03/FC06/FC23,
  rejeicao de FC16 e de escrita em registradores somente leitura)
- `tools/host/test_faults_asan` (MBAP invalido, transacao errada, excecao,
  timeout, retry, resposta parcial e reconexao)
- `tools/host/test_uhx15a_asan` (driver contra o simulador)

Compilacao manual:

```bash
make -C tools/host
./tools/host/test_faults
ASAN_OPTIONS=detect_leaks=0 ./tools/host/test_faults_asan
```

O teste do driver usa a porta definida em `UHX_PORT` (default `1503`). O
Makefile passa essa porta como `-DUHX15A_PORT`. O simulador precisa estar
rodando com `--fieldbus-timeout 0` para o teste de timeout de fieldbus.

## Memoria e concorrencia

- `mbtcp.c`: buffers estaticos `s_tx_frame[260]` e `s_rx_frame[260]`, mais
  estado de sessao (~600 bytes).
- `uhx15a.c`: estado dos dois eixos (~32 bytes) e um bloco de 16 registers na
  stack do poll.
- O transporte e' protegido por mutex interno.
- O driver assume uma task de processo dona de `uhx15a_poll()` e
  `uhx15a_set_target()`. Os getters sao leituras simples do ultimo poll; se
  forem chamados de outra task, trate como telemetria eventual.

## Observacoes

- `MBTCP_TRACE` e' `0` por padrao; defina `1` para logar TX/RX em hexadecimal
  via `tcpprobe`.
- Leituras usam o campo `length` do MBAP para determinar o tamanho exato da
  resposta.
- O mapa provisorio e' apenas para simulador. O UHX15A real ainda nao teve o
  mapa, bits, tipos e escala confirmados.
- A sobreposicao de leitura/escrita do FC23 no UHX15A real ainda nao foi
  validada.
- O perfil de movimento (se o drive ou a aplicacao gera a rampa) ainda precisa
  ser definido com o MOVISUITE.
- O build STM32/FreeRTOS alvo ainda nao foi executado neste repositorio.
