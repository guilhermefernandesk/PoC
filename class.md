# Aula: Modbus TCP, conversando com um CLP e integrando com MQTT

Material de estudo prático, escrito a partir do PoC deste repositório
(servidor Modbus TCP escravo em FreeModbus). Todos os exemplos de frame e
endereço batem com o mapa de demonstração daqui: **holding[i] = i**,
**input[i] = 1000 + i**, Unit ID **1**, porta **502** (host: 5020).

Índice:

1. [Modbus em 5 minutos](#1-modbus-em-5-minutos)
2. [O modelo de dados: o que o CLP expõe](#2-o-modelo-de-dados-o-que-o-clp-expõe)
3. [Modbus TCP na prática: MBAP + PDU byte a byte](#3-modbus-tcp-na-prática-mbap--pdu-byte-a-byte)
4. [Como conversar com um CLP (o roteiro)](#4-como-conversar-com-um-clp-o-roteiro)
5. [Prática com o servidor deste PoC](#5-prática-com-o-servidor-deste-poc)
6. [MQTT: o que é e onde entra](#6-mqtt-o-que-é-e-onde-entra)
7. [Segurança](#7-segurança)
8. [Troubleshooting rápido](#8-troubleshooting-rápido)
9. [Exercícios](#9-exercícios)
10. [Referências](#10-referências)

---

## 1. Modbus em 5 minutos

Modbus nasceu em 1979 na Modicon (que criou o CLP) como protocolo de
comunicação entre CLPs. É **voluntariamente simples**: nada de handshake
complexo, descoberta de serviços ou schemas — só requisição e resposta com
bytes bem definidos. Por isso sobrevive até hoje: praticamente todo CLP,
inversor, medidor e gateway industrial fala Modbus.

Conceitos essenciais:

- **Master / slave** (em TCP: client / server). O master é quem pergunta —
  o CLP ou o software SCADA. O slave (nosso PoC) só responde quando é
  endereçado. Um slave **nunca** fala sozinho.
- **Transportes**: RTU (serial RS232/RS485, binário + CRC), ASCII (serial,
  texto) e **TCP** (Ethernet, porta 502). O protocolo de aplicação é o
  mesmo; muda o "envelope".
- **Registro** = 16 bits. É a menor unidade de dados útil do Modbus.
  Valores de 32 bits usam **dois** registradores (e a ordem das palavras é
  o maior pesadelo de quem integra — aula disso adiante).
- **Unit ID**: endereço lógico do slave (1..247). Em serial, identifica o
  equipamento no barramento. Em TCP puro é quase cerimonial (o IP já
  identifica), mas **importa muito com gateways** (seção 4).

Por que ainda usar em 2026? Porque a indústria troca de protocolo na
velocidade da indústria: o CLP do turno de produção fala Modbus, e o
custo de trocar é alto. O caminho moderno é usar Modbus na ponta (CLP) e
**MQTT** na integração (nuvem/dashboards) — exatamente a arquitetura da
seção 6.

---

## 2. O modelo de dados: o que o CLP expõe

O Modbus organiza tudo em **quatro tabelas**, cada uma com até 65536
posições (endereços 0..65535):

| Tabela | Tamanho | Acesso | Notação clássica | FC de leitura | FC de escrita |
| --- | --- | --- | --- | --- | --- |
| Coils (bobinas) | 1 bit | leitura/escrita | `0x` | 01 | 05, 15 |
| Discrete inputs | 1 bit | só leitura | `1x` | 02 | — |
| Input registers | 16 bits | só leitura | `3x` | 04 | — |
| Holding registers | 16 bits | leitura/escrita | `4x` | 03 | 06, 16 |

Traduzindo a notação antiga (que você vai encontrar em documentação de
CLP):

- `40001` = **holding register** de endereço **0** (a notação usa base 1)
- `30001` = **input register** de endereço **0**
- `00001` = coil de endereço **0**
- `10001` = discrete input de endereço **0**

### O erro clássico: off-by-one

O **protocolo** usa endereço base 0. A **notação humana** (40001) usa
base 1. A maioria das ferramentas aceita os dois e some/algebra isso por
você (no `mbpoll`, `-r 1` com `-t 4` lê o `40001`, que é o endereço PDU
0). Mas documentação de fabricante às vezes mistura as convenções —
quando a leitura "andar um registrador", esse é o motivo. Regra de bolso:

> Se você pediu o `40001` e recebeu o que parece ser o `40002`, some ou
> subtraia 1 do endereço e teste de novo.

### No PoC

O mapa de demonstração tem **100 holding + 100 input**:

- Holding `0..99` (PLC `40001..40100`) inicializa com `0..99`
- Input `0..99` (PLC `30001..30100`) inicializa com `1000..1099`

---

## 3. Modbus TCP na prática: MBAP + PDU byte a byte

Em TCP, cada mensagem Modbus é um **ADU** (Application Data Unit):

```
+----------------------- MBAP header (7 bytes) --------------------+-- PDU -------+
| Transaction ID (2) | Protocol ID (2) | Length (2) | Unit ID (1)  | FC (1) | dados |
+--------------------+-----------------+-------------+--------------+---------+------+
```

O master (client TCP) abre a conexão na **porta 502** do slave, escreve o
ADU e lê a resposta. Sem framing extra: o TCP já entrega bytes íntegros;
o MBAP `length` delimita a mensagem.

### O cabeçalho MBAP, campo a campo

| Offset | Campo | Tamanho | Valor/significado |
| --- | --- | --- | --- |
| 0 | **TID** | 2 | Transaction ID — número que o master escolhe e o slave **ecoa**. Serve para casar resposta com requisição |
| 2 | **PID** | 2 | Protocol ID — **sempre 0** para Modbus (era a extensão para outros protocolos) |
| 4 | **LEN** | 2 | Quantidade de bytes a partir do **Unit ID** inclusive (UID + PDU) |
| 6 | **UID** | 1 | Endereço do slave (1..247). Em gateway serial, é roteado ao equipamento atrás dele |
| 7 | **FC** | 1 | Function code — a "pergunta" |

O PDU (FC + dados) tem no máximo **253 bytes**.

### Exemplo real: FC03 lendo 4 holding registers do endereço 0

Requisição (12 bytes):

```
00 01  00 00  00 06  01  03  00 00  00 04
 TID    PID    LEN   UID FC  addr   qty
```

- TID `0001`: transação 1
- PID `0000`: Modbus
- LEN `0006` = UID(1) + FC(1) + addr(2) + qty(2) = 6
- UID `01`: slave 1
- FC `03`: ler holding registers
- addr `0000`: endereço 0 (o `40001`)
- qty `0004`: 4 registradores

Resposta (15 bytes):

```
00 01  00 00  00 0B  01  03  08  00 00 00 01 00 02 00 03
 TID    PID    LEN   UID FC  BC  dados (8 bytes = 4 registradores)
```

- O TID/PID/UID **ecoam** a requisição (foi assim que o PoC valida que a
  resposta é da transação certa)
- LEN `000B` = 11 = UID(1) + FC(1) + byte count(1) + 8
- BC `08`: 8 bytes de dados
- Dados `00 00 | 00 01 | 00 02 | 00 03`: os valores 0, 1, 2, 3 —
  **big-endian**, 2 bytes por registrador

### Os function codes que você vai encontrar

| FC | Nome | Lê/escreve | Neste PoC |
| --- | --- | --- | --- |
| 01 | Read Coils | lê bits | desabilitado → exceção 01 |
| 02 | Read Discrete Inputs | lê bits | desabilitado → exceção 01 |
| **03** | **Read Holding Registers** | lê 16 bits | ✔ |
| **04** | **Read Input Registers** | lê 16 bits | ✔ |
| 05 | Write Single Coil | escreve bit | desabilitado → exceção 01 |
| **06** | **Write Single Register** | escreve 16 bits | ✔ |
| 08 | Diagnostics | — | não suportado |
| 15 (0x0F) | Write Multiple Coils | escreve bits | desabilitado |
| **16 (0x10)** | **Write Multiple Registers** | escreve 16 bits | ✔ |
| 23 (0x17) | Read/Write Multiple Registers | lê+escreve | desabilitado |
| 43 (0x2B) | Encapsulated Interface (MEI) | identificação | não |

> Nem todo slave implementa todos os FCs — e isso **não é erro**. A
> resposta correta é a exceção 01 (illegal function). O PoC deliberadamente
> deixa coils/desligados para demonstrar exatamente esse comportamento.

### Exceções: o "não" educado do Modbus

Quando o slave não consegue atender, ele responde com o FC **+ 0x80** e um
código de exceção:

| Código | Nome | Causa típica |
| --- | --- | --- |
| 01 | Illegal Function | FC não implementado (ex.: FC05 no PoC) |
| 02 | Illegal Data Address | endereço fora do mapa (ex.: ler o registrador 1000) |
| 03 | Illegal Data Value | quantidade inválida (0), byte count errado, registradores demais (> 125 na leitura) |
| 04 | Slave Device Failure | o slave quebrou ao processar |
| 06 | Busy | o slave está ocupado, tente de novo |

Frame de exceção (FC05 contra o PoC, que não tem coils):

```
requisição:  00 04 00 00 00 06 01 05 00 00 FF 00
resposta:    00 04 00 00 00 03 01 85 01
                                 ^^ ^^
                          FC 05+80  código 01
```

Repare: LEN cai para 3 (UID + FC + código). **Uma exceção é uma resposta
válida** — só é erro de protocolo quando o slave não responde nada
(timeout), responde fora de ordem ou fecha a conexão.

---

## 4. Como conversar com um CLP (o roteiro)

### Passo 0 — Levante o mapa do CLP

Antes de escrever uma linha de código, extraia da documentação/da pessoa
responsável pelo CLP:

1. **Quais FCs** ele usa (normalmente só 03 e 16 para dados; 04 para
   medição).
2. **Endereços e base** (base 0 ou a notação 4xxxx? confira sempre!).
3. **Tipos e ordem de palavras** para valores de 32 bits — floats,
   contadores longos etc. ocupam 2 registradores e há 4 convenções no
   mercado (veja tabela abaixo).
4. **Unit ID** correto (normalmente 1 para CLP com TCP nativo).
5. **Ritmo de polling** e timeout esperado.
6. Se o CLP é **master ou slave**: CLPs pequenos costumam ser master e
   *varrer* os slaves (o PoC é slave, feito para esse caso). Se o seu CLP
   é slave, o papel se inverte: seu software é o master.

### Word order de 32 bits (o pesadelo)

O valor `0x12345678` em dois registradores consecutivos pode chegar de 4
jeitos:

| Nome | Registrador alto | Registrador baixo | Onde aparece |
| --- | --- | --- | --- |
| **ABCD** (big-endian) | `1234` | `5678` | maioria, Siemens via conversor |
| **CDAB** (word swap) | `5678` | `1234` | muitos Allen-Bradley, alguns Schneider |
| **BADC** (byte swap) | `3412` | `7856` | menos comum |
| **DCBA** (little-endian) | `7856` | `1234` | embarcados little-endian |

Teste escrevendo um valor conhecido (ex.: `0x12345678`) e vendo como vem
de volta. Não presuma — o PoC exige a mesma atenção se o dado do CLP for
32 bits.

### Unit ID e gateways

- **CLP com Ethernet nativa**: UID costuma ser 1 e o core nem valida.
- **Gateway Modbus TCP → RTU** (muito comum): o UID da requisição TCP é
  **traduzido no endereço do slave serial** atrás do gateway. Se o
  medidor é o slave 5 na rede RS485, você manda UID 5 pela rede TCP.
- UID 0 (ou 255) em RTU é **broadcast** (sem resposta). Em TCP, alguns
  gateways usam 255 como "o próprio gateway".

### Regras de ouro do master

1. **Uma requisição pendente por conexão**. Muitos slaves (incluindo o
   PoC) não fazem pipelining — espere a resposta antes de mandar a
   próxima. O TID existe para tolerar quem desobedece, mas não abuse.
2. **Timeout e reconexão**: comece com 1000 ms de timeout e trate
   desconexão como evento normal (reconecta e segue).
3. **Trate exceção como dado**: capture o código (01/02/03) e logue — é o
   seu melhor diagnóstico de mapa errado.
4. **Agrupe leituras**: 1 FC03 de 20 registradores é melhor que 20 FC03 de
   1 (limite: 125 registradores por leitura).
5. **Escrita de 32 bits**: use FC16 (múltiplos) para escrever as duas
   palavras na mesma transação — evita o valor "pela metade" entre
   leituras.
6. **Não fique abrindo/fechando conexão** a cada poll se o CLP é lento
   para aceitar conexões; reutilize a conexão (o PoC aceita 1 cliente por
   vez e mantém a sessão).

---

## 5. Prática com o servidor deste PoC

O repositório tem um servidor Modbus TCP de verdade para você bater nele
(host, sem hardware):

```bash
cmake -S . -B build-host && cmake --build build-host -j
./build-host/mb_server_posix 5020
```

### 5.1 mbpoll (cliente de linha de comando)

```bash
# Ler 10 holding (40001..40010) — deve mostrar 0..9
mbpoll -m tcp -a 1 -t 4 -r 1 -c 10 -1 127.0.0.1 -p 5020

# Escrever 1234 no 40001
mbpoll -m tcp -a 1 -t 4 -r 1 127.0.0.1 -p 5020 1234

# Confirmar a escrita
mbpoll -m tcp -a 1 -t 4 -r 1 -c 10 -1 127.0.0.1 -p 5020

# Ler input registers (30001..30010) — deve mostrar 1000..1009
mbpoll -m tcp -a 1 -t 3 -r 1 -c 10 -1 127.0.0.1 -p 5020

# Endereço fora do mapa (40101) → exceção 02
mbpoll -m tcp -a 1 -t 4 -r 101 -1 127.0.0.1 -p 5020

# FC05 (coil) → exceção 01 (função não implementada aqui)
mbpoll -m tcp -a 1 -t 0 -r 1 -1 127.0.0.1 -p 5020

# Unit ID errado (9) → timeout, sem resposta
mbpoll -m tcp -a 9 -t 4 -r 1 -1 127.0.0.1 -p 5020
```

`-t 4` = holding (FC03), `-t 3` = input (FC04), `-t 0` = coil (FC01/05),
`-a` = Unit ID, `-r` = referência (base 1), `-c` = quantidade, `-1` = uma
leitura só.

### 5.2 Python com pymodbus (o jeito normal de programar)

```python
from pymodbus.client import ModbusTcpClient

client = ModbusTcpClient("127.0.0.1", port=5020)
client.connect()

# FC03: 4 holding a partir do endereço 0 (40001)
rr = client.read_holding_registers(address=0, count=4, slave=1)
print(rr.registers)          # [0, 1, 2, 3]

# FC06: escreve 0xABCD no endereço 5 (40006)
client.write_register(address=5, value=0xABCD, slave=1)

# FC04: input registers
rr = client.read_input_registers(address=0, count=4, slave=1)
print(rr.registers)          # [1000, 1001, 1002, 1003]

# FC16: escreve 3 valores de uma vez a partir do endereço 10
client.write_registers(address=10, values=[11, 22, 33], slave=1)

client.close()
```

> `slave=` é o argumento do pymodbus 3.x; versões antigas usam `unit=`.
> Trate sempre `rr.isError()` — exceção Modbus não levanta exception
> Python, vem como erro lógico.

### 5.3 Python com socket puro (para entender o protocolo)

É assim que `tools/modbus_test.py` faz — sem biblioteca, só bytes:

```python
import socket
import struct

sock = socket.create_connection(("127.0.0.1", 5020), timeout=2)

# FC03, endereço 0, quantidade 4  ->  00 01 00 00 00 06 01 03 00 00 00 04
pdu = struct.pack(">BHH", 3, 0, 4)                 # FC, addr, qty
adu = struct.pack(">HHHB", 1, 0, len(pdu) + 1, 1) + pdu  # TID, PID, LEN, UID
sock.sendall(adu)

resp = sock.recv(260)
tid, pid, length, uid = struct.unpack(">HHHB", resp[:7])
fc, byte_count = resp[7], resp[8]
values = struct.unpack(f">{byte_count // 2}H", resp[9:9 + byte_count])
print(f"TID={tid} UID={uid} FC={fc} valores={values}")
sock.close()
```

### 5.4 O que a suíte de testes do PoC valida (e você pode usar de aula)

`python3 tools/modbus_test.py --host 127.0.0.1 --port 5020` roda:

| Teste | O que ensina |
| --- | --- |
| FC03 + FC16 | escrita múltipla e leitura de volta, big-endian |
| FC04 | leitura de input registers com valores conhecidos (pega off-by-one!) |
| FC06 | escrita de registrador único + eco |
| exceptions | 01 (FC não existe), 02 (endereço), 03 (quantidade) |
| fragmented frame | TCP entrega os bytes em pedaços — o framing MBAP recompõe |
| coalesced frames | duas mensagens no mesmo segmento TCP |
| wrong unit ignored | UID errado é **ignorado em silêncio** (sem fechar conexão) |

Os dois últimos são os mais instrutivos: Modbus sobre TCP **não** tem
mensagem delimitada pela conexão — quem separa frames é o par
LEN/framing do protocolo.

---

## 6. MQTT: o que é e onde entra

### 6.1 O modelo

MQTT (Message Queuing Telemetry Transport) é o padrão de fato para levar
dados de campo até nuvem/dashboards. Modelo **publish/subscribe** através
de um **broker**:

```
   [CLP/médidor]                    [Broker MQTT]                [Consumidores]
   (Modbus TCP)                      mosquitto/                  dashboard, banco,
        ^                              EMQX...                    regra, alerta
        |                         topicos e filas
   [Gateway]  --publish-->  fabrica/linha1/clp1/temp  --subscribe-->  vários
```

Quem produz **publica** em um **tópico** (ex.: `fabrica/linha1/clp1/holding`).
Quem consome **assina** um padrão de tópicos (ex.: `fabrica/linha1/#`).
Produtor e consumidor **não se conhecem** — o broker entrega. Isso muda a
topologia: com Modbus, o master precisa saber IP/endereço de cada slave; com
MQTT, todo mundo só conhece o broker.

Conceitos que você precisa dominar:

| Conceito | O que faz |
| --- | --- |
| **Tópico** | hierarquia com `/` e wildcards (`+` um nível, `#` tudo abaixo) |
| **QoS 0/1/2** | no máximo 1 vez / pelo menos 1 vez / exatamente 1 vez (custo cresce) |
| **Retained** | broker guarda a última mensagem do tópico e entrega ao novo assinante na hora |
| **Last Will (LWT)** | mensagem que o broker publica **por você** se a conexão cair — avisa "offline" |
| **Keepalive** | ping periódico que detecta cliente morto (dispara o LWT) |
| **Clean session** | descarta ou mantém assinaturas/mensagens QoS1 entre reconexões |

### 6.2 Modbus TCP vs MQTT — não são concorrentes

| Aspecto | Modbus TCP | MQTT |
| --- | --- | --- |
| Papel | protocolo de **campo**: ler/escrever registradores do CLP | **transporte** de mensagens/eventos |
| Modelo | requisição-resposta (master puxa) | pub/sub (device empurra) |
| Payload | binário fixo, 16 bits por registrador | livre (JSON, binário, Sparkplug B) |
| Endereço do dado | endereço do registrador | tópico |
| Determinismo | ciclos de polling previsíveis | best-effort (QoS 0/1/2) |
| Escrita no CLP | nativa (FC06/16) | não existe — só mensagens (precisa de gateway ou CLP que assine) |
| Segurança | **nenhuma** (sem auth/cripto) | usuário/senha, TLS (porta 8883) |
| Local típico | chão de fábrica, LAN | ponta ⇒ nuva/IIoT, WAN |

A pergunta certa não é "qual usar", é **qual em cada camada**: Modbus TCP
na ponta (o CLP fala Modbus), MQTT na integração (levar esses dados aonde
precisam chegar). O componente que liga os dois é o **gateway/ponte
Modbus→MQTT**.

### 6.3 Arquitetura de integração (o padrão do mercado)

```
[CLP] ──Modbus TCP──> [ponte/gateway] ──MQTT──> [broker] ──> [dashboard/nuvem/regras]
                        (este PoC +              (mosquitto,
                         script da seção 6.4)      EMQX, HiveMQ...)
```

- O **PoC deste repositório é o lado escravo**: o CLP master lê/escreve os
  registradores nele. Mas o mesmo código de **master** (seção 5) serve para
  a ponta inversa: ler um CLP escravo e publicar no broker.
- Um gateway pode ser um script de 50 linhas (aqui embaixo), um industrial
  (redes com protocolos múltiplos) ou um **edge gateway** com Sparkplug B.

### 6.4 Ponte Modbus→MQTT na prática (Python)

```bash
pip install pymodbus "paho-mqtt>=2"
# broker local para testar:
sudo apt install mosquitto mosquitto-clients
```

```python
"""Ponte Modbus TCP -> MQTT: lê holding do CLP e publica em JSON."""
import json
import time

import paho.mqtt.client as mqtt
from pymodbus.client import ModbusTcpClient

CLP_HOST = "192.168.1.10"       # no PoC: 127.0.0.1:5020
TOPICO = "fabrica/linha1/clp1"

modbus = ModbusTcpClient(CLP_HOST, port=5020)

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
# Last Will: se esta ponte cair, o broker avisa o mundo
mqttc.will_set(f"{TOPICO}/status", "offline", qos=1, retain=True)
mqttc.connect("localhost", 1883, keepalive=30)
mqttc.loop_start()
mqttc.publish(f"{TOPICO}/status", "online", qos=1, retain=True)

while True:
    rr = modbus.read_holding_registers(address=0, count=4, slave=1)
    if not rr.isError():
        payload = json.dumps({
            "ts": time.time(),
            "registradores": rr.registers,      # [0, 1, 2, 3] no PoC
        })
        mqttc.publish(f"{TOPICO}/holding", payload, qos=1)
    time.sleep(1.0)
```

Consumindo do outro lado:

```bash
mosquitto_sub -h localhost -t "fabrica/linha1/#" -v
# fabrica/linha1/clp1/status online
# fabrica/linha1/clp1/holding {"ts": 1762..., "registradores": [0, 1, 2, 3]}
```

Detalhes que fazem essa ponte ficar *produção-grade*:

1. **Tópico com hierarquia estável**: `site/area/equipamento/variável` —
   o wildcard `#` permite "tudo da linha 1".
2. **Status com retained + LWT**: assinante novo já nasce sabendo se a
   ponte está viva; se ela morrer, o broker publica `offline` por ela.
3. **QoS 1 para medições** (chega pelo menos uma vez; idempotente no
   consumidor) e QoS 0 para rajadas de alta frequência aceitáveis de perder.
4. **Carimbando `ts`** no produtor — o horário do broker/consumidor não é
   o horário da medição.
5. **MQTT não sabe o que é registrador**: o payload é seu contrato. JSON
   legível para dashboards; **Sparkplug B** quando a interoperabilidade
   entre fornecedores importa (define nascimento/morte de dispositivo,
   tipos, e evita gap de dados em QoS 0 — é o padrão industrial de facto
   sobre MQTT).

### 6.5 E o caminho de volta (comando via MQTT)?

Para **comandar** o equipamento a partir de um consumidor MQTT (nuvem →
chão de fábrica):

```
[App] --publish--> fabrica/linha1/clp1/cmd/setpoint   [ponte assina /cmd/#]
                                                          |
                                                          +--> FC06 no CLP (write_register)
```

A ponte assina o tópico de comando, valida (range, permissão!), escreve no
CLP via Modbus e publica o valor lido de volta como confirmação
(*report-by-exception*). **Valide sempre o comando antes de escrever no
CLP** — qualquer um que souber o tópico consegue publicar nele.

---

## 7. Segurança

Modbus TCP de 1979-2006 não tem **nenhuma** autenticação nem criptografia:

- Quem alcançar a porta 502 lê e escreve tudo. Mantenha a rede Modbus
  **isolada** (VLAN, firewall, sem rota para a internet).
- Nunca exponha a porta 502 na internet — use a arquitetura da seção 6
  (ponte + MQTT) para tirar dados da rede de campo.
- No MQTT de produção: TLS na **8883**, usuário/senha (ou certificado),
  e ACLs de tópico no broker (o dispositivo só publica/assina o que deve).
- Para acesso remoto legítimo ao CLP: VPN, nunca porta aberta.

---

## 8. Troubleshooting rápido

| Sintoma | Causa provável | Ação |
| --- | --- | --- |
| Timeout (sem resposta nenhuma) | IP/porta errados, Unit ID filtrado, slave não implementou | `nc -vz host 502`; teste UID 1; confira filtro do slave |
| Resposta `01` | FC não implementado | consulte a tabela de FCs do equipamento |
| Resposta `02` | endereço fora do mapa / base 0 vs 1 | some/subtraia 1 do endereço e reteste |
| Resposta `03` | quantidade 0 ou acima do limite, byte count errado | até 125 regs por FC03/04, 123 por FC16 |
| Valor 32 bits "quebrado" | word order | teste com 0x12345678 (seção 4) |
| Leitura "anda" um registrador | base de endereçamento | mesma correção do 02 |
| Funciona às vezes, depois para | conexão reutilizada errada, duas transações simultâneas | 1 requisição pendente por conexão; verifique TID |
| Frames se misturam | seu master não respeita LEN/framing | leia exatamente `LEN + 6` bytes por resposta |
| MQTT: valor "congelado" | produtor morto e sem LWT | configure Last Will + keepalive |
| MQTT: assinante novo sem dado | sem retained | publique status/dados com retain=True |

---

## 9. Exercícios

Use o servidor do PoC (`./build-host/mb_server_posix 5020`) como cobaia:

1. **Decodificar na mão**: mande `00 01 00 00 00 06 01 03 00 00 00 04`
   com `nc 127.0.0.1 5020` e decodifique a resposta byte a byte no papel
   (TID, PID, LEN, UID, FC, BC, dados).
2. **Off-by-one**: leia `40001` com mbpoll e com pymodbus
   (`address=0`). Confirme que os dois batem. Depois leia `address=1` e
   veja o que muda.
3. **Exceções**: provoque 01 (FC05), 02 (endereço 101+), 03 (quantidade
   inválida — escreva um script) e identifique cada uma no hex da resposta.
4. **Unit ID**: com UID 2, confirme o timeout silencioso; com
   `MB_PORT_TCP_ACCEPT_ANY_UNIT=1` em `ports/posix/port.h` (rebuild),
   confirme que passa a responder.
5. **Fragmentação**: com um script Python, envie o ADU em 3 pedaços com
   `sleep` entre eles; veja o servidor responder mesmo assim.
6. **Ponte MQTT**: rode a ponta da seção 6.4 apontando para o
   `127.0.0.1:5020` e o mosquitto local; consuma com `mosquitto_sub` e
   derrube a ponte para ver o LWT publicar `offline`.
7. **Estresse**: 2 clientes simultâneos — observe que o segundo espera
   (comportamento documentado: 1 cliente por vez).

---

## 10. Referências

- **Modbus Application Protocol Specification V1.1b3** — a fonte
  canônica dos FCs e exceções: <https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf>
- **MODBUS Messaging on TCP/IP Implementation Guide V1.0b** — MBAP,
  porta 502, Unit ID/gateways: <https://modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1.0b.pdf>
- **MQTT 5.0 / 3.1.1 spec** (OASIS): <https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html>
- **Sparkplug B specification** (Eclipse): <https://sparkplug.eclipse.org/>
- Neste repositório: `README.md` (arquitetura do PoC),
  `tools/modbus_test.py` (contrato do protocolo em testes),
  `docs/MIGRACAO_FREEMODBUS_FREERTOS_PLUS_TCP.md` (port do FreeModbus).
