# PoC Modbus TCP para STM32H743

Servidor Modbus TCP portátil para uma NUCLEO-H743ZI2 (STM32H743ZIT6, PHY
LAN8742A, RMII). A prova de conceito (PoC) substitui o link RS232/PLC atual
por Modbus TCP. O núcleo Modbus é o **FreeModbus** (BSD-3-Clause,
vendorizado sem alterações em `third_party/freemodbus`); os ports
`ports/posix` e `ports/lwip` escritos aqui fazem todo o I/O TCP, sem HAL e
sem depender de RTOS, para que possa ser reutilizado em um firmware de
produção sem HAL posteriormente.

## O que está implementado

- Servidor Modbus TCP na porta **502**, Unit ID **1** por padrão.
- Stack escrava FreeModbus (vendorizada, sem alterações).
- Códigos de função:
  - **FC03** Ler Registradores Holding
  - **FC04** Ler Registradores de Entrada
  - **FC06** Escrever Registrador Único
  - **FC16** Escrever Múltiplos Registradores
- Tratamento correto do MBAP do Modbus TCP no port:
  - identificador de protocolo deve ser 0
  - ID de transação é ecoado
  - Unit ID é filtrado (ou aceita qualquer unidade com
    `MB_PORT_TCP_ACCEPT_ANY_UNIT`)
  - quadros TCP fragmentados e agrupados são tratados
  - respostas de exceção: `01` função ilegal, `02` endereço de dados
    ilegal, `03` valor de dado ilegal
- Mapa de demonstração: **100 registradores holding** e **100 registradores
  de entrada**, com endereços de protocolo baseados em zero. PLC `40001`
  mapeia para o endereço holding `0`; PLC `30001` mapeia para o endereço de
  entrada `0`.
- Um cliente Modbus TCP por vez. Um novo cliente é aceito depois que a
  conexão anterior fecha.
- Dois ports do FreeModbus (mesmo desenho, `port.h` + `portevent.c` +
  `porttcp.c`):
  - `ports/posix` para validação no host/PC com sockets Linux
  - `ports/lwip` para o alvo STM32 usando LwIP netconn
- Todo o I/O acontece dentro de `eMBPoll()`: a aplicação é apenas um loop
  `eMBTCPInit -> eMBEnable -> eMBPoll`, sem filas nem tarefas extras.
- Suporte a alvo sem HAL:
  - configuração de clock HSI/PLL nos registradores RCC
  - configuração de GPIO RMII
  - mapeamento pela MPU da memória DMA da ETH como não cacheável
  - driver de anel MAC/DMA da ETH com RX/TX baseado em cópia
  - driver MDIO e PHY LAN8742A
  - `ethernetif.c` do LwIP
  - modelos de configuração FreeRTOS/LwIP
  - tarefas FreeRTOS nativas (sem CMSIS-RTOS)

O núcleo FreeModbus vendorizado não contém **nenhum** header de HAL, LwIP,
FreeRTOS ou POSIX — a configuração (`port.h`) e o I/O (port) são nossos.
Exemplos com HAL foram usados apenas como referência para valores de
clock, pinagem, PHY e DMA.

## Estrutura do repositório

```
third_party/freemodbus/  núcleo FreeModbus vendorizado (BSD, sem alterações)
ports/posix/             port FreeModbus: sockets Linux (somente host)
ports/lwip/              port FreeModbus: LwIP netconn (alvo)
app/app_regs.c,h         mapa de registradores de demonstração
tests/test_mb_regs.c      testes unitários do mapa/callbacks
tests/host/main_posix.c  servidor host POSIX
tools/modbus_test.py     validação black-box Modbus TCP
target/board/            clock, delay, GPIO e MPU sem HAL
target/eth/              driver MDIO, LAN8742A, MAC/DMA da ETH
target/net/              ethernetif LwIP, lwipopts.h, arch/cc.h
target/app/              main e hooks do FreeRTOS
target/FreeRTOSConfig.h  modelo de configuração do FreeRTOS
target/poc_sources.cmake lista de fontes para um projeto CMake existente
docs/                    guias: port sem HAL; migração FreeModbus-Plus-TCP
```

Cada port tem três arquivos: `port.h` (configuração da stack + tipos +
seções críticas), `portevent.c` (cola de eventos; roda o pool TCP dentro de
`eMBPoll`) e `porttcp.c` (listener, framing MBAP, filtro de Unit ID, 1
cliente por vez).

## 1. Validação no host primeiro

Requisitos: Linux, CMake, um compilador C e Python 3.

```bash
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host -j
ctest --test-dir build-host --output-on-failure
```

Resultado esperado: `mb_regs` passa.

Inicie o servidor POSIX (a porta 5020 evita precisar de `root`):

```bash
./build-host/mb_server_posix 5020
```

Em outro terminal:

```bash
python3 tools/modbus_test.py --host 127.0.0.1 --port 5020
```

Resultado esperado:

```
PASS FC03 + FC16
PASS FC04
PASS FC06
PASS exceptions
PASS fragmented frame
PASS coalesced frames
PASS wrong unit ignored
all Modbus TCP tests passed
```

O script grava valores conhecidos antes de lê-los, portanto é seguro
executá-lo repetidamente contra o mapa mantido em RAM.

Teste opcional com cliente de terceiros:

```bash
mbpoll -m tcp -a 1 -t 4 -r 1 -c 10 127.0.0.1 -p 5020
```

## 2. Mapa Modbus e premissas do PLC

Valores padrão:

| Item | Valor |
| --- | --- |
| Porta do servidor | 502 |
| ID de unidade (Unit ID) | 1 |
| Registradores holding | 100, endereços 0..99 |
| Registradores de entrada | 100, endereços 0..99 |
| Valor inicial do holding no endereço `i` | `i` |
| Valor inicial da entrada no endereço `i` | `1000 + i` |
| Clientes TCP | 1 por vez |

Para um PLC que exibe `40001`, use o endereço de protocolo `0`. O mapa do
PLC ainda não é conhecido; altere `app/app_regs.c` quando o mapa real
estiver disponível. Se o PLC usar valores de 32 bits, implemente
explicitamente a ordem das palavras necessária; não presuma que dois
registradores consecutivos sejam suficientes.

O PoC intencionalmente não implementa FC01, FC02, FC05, FC15, broadcast,
registros de arquivo, diagnósticos ou identificação de dispositivo.
Adicione esses recursos apenas depois que o mapa do PLC confirmar que são
necessários.

## 3. Configuração de hardware do alvo

NUCLEO-H743ZI2:

- MCU: STM32H743ZIT6, LQFP144
- PHY: LAN8742A, RMII, endereço PHY 0
- Reset do PHY: NRST da placa / ponte de solda `SB74`, como na placa Nucleo
- PC conectado diretamente ao conector Ethernet da placa

### Árvore de clock

Implementada em `target/board/clock.c` usando registradores RCC/PWR/FLASH:

| Sinal | Valor |
| --- | --- |
| HSI | 64 MHz |
| PLL1 M | 32 |
| PLL1 N | 400 |
| PLL1 P | 2 |
| PLL1 Q | 2 |
| PLL1 R | 2 |
| SYSCLK | 400 MHz |
| HCLK / AXI | 200 MHz |
| APB1/APB2/APB3/APB4 | 100 MHz |
| Escalonamento de tensão | VOS1, alimentação LDO |
| Latência da Flash | 4 estados de espera, `WRHIGHFREQ=3` |

`SystemCoreClock` é atualizado para 400 MHz após a troca.

### Pinagem RMII

| Pino | Sinal | Função alternativa |
| --- | --- | --- |
| PA1 | ETH_REF_CLK | AF11 |
| PA2 | ETH_MDIO | AF11, pull-up |
| PA7 | ETH_CRS_DV | AF11 |
| PC1 | ETH_MDC | AF11 |
| PC4 | ETH_RXD0 | AF11 |
| PC5 | ETH_RXD1 | AF11 |
| PG11 | ETH_TX_EN | AF11 |
| PG13 | ETH_TXD0 | AF11 |
| PB13 | ETH_TXD1 | AF11 |

O RMII é selecionado via `SYSCFG->PMCR.EPIS_SEL = RMII` em
`target/board/gpio.c`.

### Memória DMA da Ethernet e cache

O driver usa uma janela fixa de 32 KiB na SRAM D2:

| Endereço | Uso |
| --- | --- |
| `0x30000000` | 4 descritores de RX, 16 bytes cada |
| `0x30000100` | 4 descritores de TX, 16 bytes cada |
| `0x30000200` | 4 buffers de RX, 1536 bytes cada |
| `0x30001A00` | 4 buffers de TX, 1536 bytes cada |

`target/board/mpu.c` mapeia `0x30000000..0x30007FFF` como memória normal,
não cacheável, não compartilhável e com acesso total. O D-cache pode
permanecer habilitado; o caminho de dados da Ethernet não tem manutenção de
cache porque descritores e buffers ficam nessa região não cacheável.

O driver usa descritores normais de 16 bytes (`DMACCR.DSL = 0`), rajadas de
32 beats, buffers de RX de 1536 bytes e modo store-and-forward (armazenar e
encaminhar) nas duas filas MTL.

### PHY

`target/eth/lan8742.c`:

- faz soft reset do PHY via BCR (espera de reset de 500 ms no pior caso)
- inicia a auto-negociação
- espera até cerca de 3 segundos pelo link após o reset
- lê o BSR para link, 100 Mbit e full-duplex
- `lan8742_read_id()` pode ser usado no depurador para confirmar
  `ID1=0x0007`, `ID2=0xC130` (combinado `0x0007C130`)

O MDIO usa os registradores MDIO do MAC ETH do H7 com divisor de clock
`/102` (~1,96 MHz com HCLK de 200 MHz).

### Padrões de rede

Em `target/app/main.c`:

```text
IP      192.168.1.10
mask    255.255.255.0
gateway 0.0.0.0
port    502
unit    1
MAC     02:00:00:00:00:01
```

O endereço MAC é administrado localmente e deve ser alterado se houver mais
de uma placa na mesma rede.

## 4. Integração no projeto STM32 gerado

O repositório não contém um projeto STM32Cube gerado. Gere um projeto
localmente com a extensão STM32 para VS Code / STM32CubeMX para a
NUCLEO-H743ZI2 e depois adicione os arquivos daqui.

> Para portar este desenho para um projeto **Makefile + CMSIS-OS v1 +
> FreeRTOS-Plus-TCP** (em vez de CMake + FreeRTOS nativo + LwIP), use o
> guia completo em
> [`docs/MIGRACAO_FREEMODBUS_FREERTOS_PLUS_TCP.md`](docs/MIGRACAO_FREEMODBUS_FREERTOS_PLUS_TCP.md).

### 4.1 O que manter do projeto gerado

Mantenha estas partes geradas/do fornecedor:

- header de dispositivo CMSIS e `system_stm32h7xx.c`
- arquivo de startup e linker script
- fontes do kernel FreeRTOS e porta Cortex-M7
- fontes do LwIP
- porta FreeRTOS do LwIP (`sys_arch.c` e `arch/sys_arch.h` do diretório
  contrib FreeRTOS do LwIP, ou o equivalente gerado pelo CubeMX)

**Não** chame a inicialização da HAL gerada no caminho do PoC:

- `HAL_Init()`
- `SystemClock_Config()` do exemplo com HAL
- `MX_GPIO_Init()`
- `MX_ETH_Init()`
- `MX_LWIP_Init()` gerado
- `ethernetif.c` gerado para o netif do PoC

O PoC inicializa esses blocos diretamente nos registradores. Arquivos-fonte
da HAL podem permanecer no projeto para código gerado não relacionado, mas
os arquivos entregues do PoC não chamam a HAL.

### 4.2 Adicionar as fontes do PoC

`target/poc_sources.cmake` exporta a lista de fontes. No `CMakeLists.txt`
do firmware, depois que o target do firmware existir:

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/target/poc_sources.cmake)
target_sources(your_firmware_target PRIVATE ${POC_SOURCES})
target_include_directories(your_firmware_target PRIVATE ${POC_INCLUDE_DIRS})
```

Substitua `your_firmware_target` pelo target criado pelo projeto gerado.
Garanta que o projeto gerado também exponha:

- define de dispositivo `STM32H743xx`
- diretórios de include do núcleo/dispositivo CMSIS
- include do FreeRTOS e da porta Cortex-M7 `portable/GCC/ARM_CM7/r0p1`
- `src/include` do LwIP
- include da contrib FreeRTOS do LwIP (`arch/sys_arch.h`)
- `sys_arch.c`

Certifique-se de que os diretórios de include do PoC `target` e `target/net`
venham antes do diretório `Core/Inc` gerado / diretório do alvo LwIP. Caso
contrário, arquivos gerados desatualizados `FreeRTOSConfig.h`, `lwipopts.h`
ou `arch/cc.h` podem sobrescrever silenciosamente
`target/FreeRTOSConfig.h`, `target/net/lwipopts.h` e
`target/net/arch/cc.h`. Substituir as cópias geradas também é uma opção.

### 4.3 Substituir o ponto de entrada da aplicação

`target/app/main.c` substitui o `Core/Src/main.c` gerado para o PoC.
Ele faz, em ordem:

1. `board_clock_init()`
2. `board_delay_init()`
3. `board_mpu_init()`
4. habilita CP10/CP11 em `SCB->CPACR` (contexto FPU do FreeRTOS)
5. habilita I-cache e D-cache
6. `board_gpio_init()`
7. `app_regs_init()`
8. `tcpip_init(NULL, NULL)`
9. `netif_add(..., ethernetif_init, tcpip_input)`
10. cria a tarefa de polling do netif e a tarefa Modbus
11. inicia o escalonador do FreeRTOS

`tcpip_input` é importante: os quadros recebidos são enfileirados para a
thread tcpip do LwIP em vez de serem processados na tarefa do netif.

Se você mantiver o `main.c` gerado, chame a mesma sequência e não chame a
inicialização HAL/LwIP gerada.

### 4.4 Usar as opções LwIP do PoC

Use `target/net/lwipopts.h` ou mescle estas configurações obrigatórias no
`lwipopts.h` gerado:

```c
#define NO_SYS 0
#define LWIP_NETCONN 1
#define LWIP_TCP 1
#define LWIP_SO_RCVTIMEO 1   /* exigido por ports/lwip/porttcp.c */
#define LWIP_DHCP 0
#define LWIP_DNS 0
#define LWIP_IPV6 0
#define MEM_SIZE (64u * 1024u)
#define TCP_MSS 1460
#define TCPIP_THREAD_STACKSIZE 2048
#define TCPIP_THREAD_PRIO 3
#define LWIP_FREERTOS_THREAD_STACKSIZE_IS_STACKWORDS 1
```

`LWIP_FREERTOS_THREAD_STACKSIZE_IS_STACKWORDS=1` faz a porta FreeRTOS
contrib do LwIP interpretar `TCPIP_THREAD_STACKSIZE` como palavras do
FreeRTOS (8 KiB), não bytes. Se o `sys_arch.c` gerado pelo CubeMX já passa o
valor diretamente para `xTaskCreate`, ele também está em palavras, então o
valor é consistente.

O heap do LwIP é uma alocação `.bss` normal na AXI SRAM cacheável. Apenas os
descritores e buffers DMA da ETH ficam na janela D2 não cacheável. O port
LwIP usa objetos estáticos, então o caminho Modbus não precisa de heap da
libc nem de `_sbrk` no alvo.

### 4.5 Usar a configuração FreeRTOS do PoC

Use `target/FreeRTOSConfig.h` ou mescle estes valores:

```c
#define configCPU_CLOCK_HZ 400000000u
#define configTICK_RATE_HZ 1000u
#define configTOTAL_HEAP_SIZE (64u * 1024u)
#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configENABLE_FPU 1
#define configENABLE_MPU 0
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
```

A porta FreeRTOS do LwIP exige `INCLUDE_vTaskDelay` e
`INCLUDE_vTaskSuspend`; sem elas, `sys_arch.c` interrompe a compilação
intencionalmente com `#error`.

O PoC usa chamadas nativas do FreeRTOS. Desabilite o CMSIS-RTOS v2 ou
remova suas definições de `SVC_Handler`/`PendSV_Handler`/`SysTick_Handler`
se entrarem em conflito com a porta Cortex-M do FreeRTOS.

A configuração fornecida mapeia os handlers do FreeRTOS para os nomes do
startup CMSIS:

```c
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
```

### 4.6 Reservar a SRAM D2 no linker script

O driver da ETH usa endereços fixos `0x30000000..0x30007FFF`. Certifique-se
de que o linker script gerado **não** coloca `.data`, `.bss`, heap ou stack
nessa faixa. Se o linker tiver uma região `RAM_D2`, remova-a das alocações
normais ou reserve os primeiros 32 KiB para a ETH.

O restante do firmware pode usar as regiões geradas (AXI SRAM, DTCM,
D3 SRAM) como antes. O heap do FreeRTOS e o heap do LwIP devem permanecer na
AXI SRAM cacheável.

### 4.7 Nomes dos vetores de startup

O arquivo de startup gerado deve exportar `SVC_Handler`, `PendSV_Handler` e
`SysTick_Handler`. A porta Cortex-M7 do FreeRTOS assume esses nomes através
das macros em `target/FreeRTOSConfig.h`. Remova quaisquer definições geradas
desses três handlers em `stm32h7xx_it.c`/CMSIS-RTOS e não compile também um
kernel CMSIS-RTOS v2 que os defina.

## 5. Compilar e gravar

O comando exato depende do projeto STM32 gerado no VS Code. Com o wrapper
`stm32` usado neste workspace:

```bash
stm32 build
stm32 flash
```

Para um projeto CMake genérico:

```bash
cmake -S . -B build-target -DCMAKE_BUILD_TYPE=Release
cmake --build build-target -j
```

Use o linker script e o arquivo de startup gerados. A lista de fontes do PoC
é C11 independente de compilador. Neste workspace, todas as fontes do alvo
entregues foram compiladas e linkadas com `arm-none-eabi-gcc` contra CMSIS,
FreeRTOS e LwIP em uma imagem mínima temporária de firmware; a execução em
hardware continua sendo a etapa de validação pendente.

## 6. Configuração de rede do PC para cabo direto

O PC deve estar na mesma sub-rede da placa. Exemplo para a interface `eth0`:

```bash
sudo ip link set eth0 up
sudo ip addr add 192.168.1.2/24 dev eth0
ip route get 192.168.1.10
```

`ip route get` deve mostrar `dev eth0`, não Wi-Fi. Se mostrar uma interface
Wi-Fi, desative temporariamente o Wi-Fi ou adicione uma rota mais específica:

```bash
sudo ip route add 192.168.1.10/32 dev eth0
ping -I eth0 192.168.1.10
```

Não adicione gateway padrão no link da placa. A placa usa
`gateway 0.0.0.0`; o PC precisa apenas da rota direta.

## 7. Ordem de validação

Use esta ordem para que uma falha seja atribuível a uma camada:

1. Build no host e CTest passam.
2. Testes black-box em Python passam no servidor POSIX.
3. Firmware do alvo compila e é gravado.
4. O ID do PHY é `0x0007C130` (chamada a `lan8742_read_id()` no depurador
   ou inspeção dos registradores de ID do PHY).
5. O link está ativo, 100 Mbit, full-duplex (`lan8742_poll` / BSR).
6. `ping 192.168.1.10` a partir do PC funciona.
7. A porta TCP 502 aceita conexões (`nc -vz 192.168.1.10 502`).
8. Execute a mesma validação em Python contra a placa:

   ```bash
   python3 tools/modbus_test.py --host 192.168.1.10 --port 502
   ```

9. Execute um teste de soak mais longo (por exemplo, polling FC03/FC16
   repetido por 10–30 minutos) e verifique timeouts, desconexões e
   crescimento de memória/stack.

Se o ping falhar, verifique nesta ordem: link do PHY, REF_CLK RMII, região
de memória do MAC/DMA, atributos de MPU/cache, IP/máscara e se a rota do PC
usa a interface Ethernet. Se o ping funcionar mas a porta 502 falhar,
verifique `TCPIP_THREAD_STACKSIZE`, `MEM_SIZE`, `sys_arch.c` e a stack da
tarefa Modbus.

## 8. Integração com o PLC

Presume-se que o PLC seja o mestre Modbus TCP. Ele deve conectar em
`192.168.1.10:502` e usar Unit ID 1 (ou o valor configurado).

Antes de alterar o mapa, colete da documentação do PLC/RS232:

- Unit ID
- Códigos de função usados
- Endereços dos registradores e se são baseados em 0 ou em 1
- Larguras de dados e ordem das palavras para valores de 32 bits
- Intervalo de polling e timeout de resposta
- Se o PLC abre uma conexão persistente ou reconecta

Depois altere apenas:

- `app/app_regs.c` para o mapa/dados
- `ports/lwip/port.h` para Unit ID, porta e códigos de função, se
  necessário
- `target/app/main.c` para IP, se necessário

O núcleo FreeModbus e o enquadramento TCP do port não precisam mudar.

## 9. Limitações conhecidas

- O código do alvo não foi executado em hardware físico neste workspace; o
  caminho do host e as verificações de compilação/link ARM estão completos,
  mas PHY/link/ping e interoperabilidade com o PLC precisam ser validados na
  placa.
- Um cliente Modbus TCP por vez.
- Apenas FC03/FC04/FC06/FC16 (outros códigos respondem exceção 01; habilite
  em `ports/*/port.h` e implemente o callback se o PLC precisar).
- O FreeModbus é single-instance: uma stack Modbus por firmware.
- Apenas IPv4 estático; sem DHCP, DNS, IPv6, TLS ou autenticação.
- Sem armazenamento persistente de registradores; os valores são reiniciados
  no reboot.
- RX/TX são baseados em cópia com 4 descritores cada. Isso é simples e
  robusto, mas não é o projeto de maior throughput.
- Sem interrupção de Ethernet; a tarefa do netif faz polling a cada 2 ms
  (~500 quadros/s no limite superior). Isso é suficiente para polling de PLC,
  mas não para um servidor de alta taxa de uso geral.
- O projeto CubeMX gerado pode ainda conter arquivos da HAL; o código do PoC
  entregue não os chama. Remova a inicialização da HAL do caminho do PoC.

## 10. Referências usadas para configuração

- FreeModbus (cwalter-at/freemodbus, commit `a9ec789`, BSD-3-Clause) —
  núcleo vendorizado em `third_party/freemodbus`
- Headers de dispositivo CMSIS do STM32H7 (`stm32h743xx.h`) para definições
  de registradores
- Exemplos LwIP do STM32CubeH7 para valores de ETH/LwIP do H7
- STM32H7-LwIP-Examples `STM32H743_Nucleo_ETH` para RMII da NUCLEO-H743ZI2,
  endereço do PHY, layout dos descritores e dimensionamento do LwIP
- Manual do usuário da NUCLEO-H743ZI2 para pinos RMII e ligação do LAN8742A
- Datasheet do LAN8742A para registradores BCR/BSR/ID
- Modbus Application Protocol V1.1b3 e Modbus Messaging on TCP/IP V1.0b

Os exemplos de referência da HAL foram usados apenas para valores. Nenhum
código-fonte da HAL faz parte do núcleo FreeModbus, dos ports POSIX/LwIP ou
dos drivers do alvo. A única dependência de terceiros é o próprio
FreeModbus (BSD-3-Clause).
