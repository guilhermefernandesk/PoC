# drivers/ — PCA9535, LEDs e botões

Este diretório contém o acesso ao expansor I2C PCA9535 usado pelos LEDs RGB
pelos botões da placa.

O código original (311 linhas em `pca.c`) era, na prática, **dois drivers em
camadas dentro de um único arquivo**. Este README explica como essa
manipulação de structs e ponteiros funcionava e por que ela foi simplificada.

---

## 1. O handle do PCA9535 — o "objeto" do chip

```c
typedef struct {
    I2C_HandleTypeDef *hi2c;   /* ponteiro para o barramento HAL */
    uint8_t address;           /* endereço já deslocado (0x40 | offset) */
    uint8_t output_state[2];   /* cópia local do registrador de saída */
    uint8_t config_state[2];   /* cópia local do registrador de direção */
} PCA9535_Handle_t;
```

Cada variável desse tipo representava **um chip PCA9535 físico** e existiam
duas:

```c
PCA9535_Handle_t io_exp;   /* endereço 0x23 */
PCA9535_Handle_t io_exp2;  /* endereço 0x21 */
```

Todas as funções recebiam `PCA9535_Handle_t *dev`. Esse ponteiro é o `this`
implícito: ele carrega o barramento, o endereço e o cache. Por isso o mesmo
código servia para os dois chips — bastava trocar o ponteiro.

`PCA9535_Init(&io_exp, &hi2c4, PCA_ADDR_0)` guardava `&hi2c4` em `hi2c`,
montava o endereço com `0x40 | (offset << 1)` e inicializava os caches em
`0xFF` (todas as linhas como entrada, saídas em nível alto).

Os `0x03` e `0x01` passados como `addr_offset` eram o valor dos pinos físicos
A2, A1, A0 do chip, somados à base `0x20`. Na versão atual isso virou o
endereço 7 bits direto (`0x23` e `0x21`), muito mais claro.

---

## 2. O cache e o read-modify-write

O PCA9535 tem um registrador por porta, mas vários LEDs compartilham a mesma
porta (LD3 e LD4 em P0; LD7 e LD8 em P1). Para mudar **um** pino sem mexer nos
outros, o código fazia:

```c
uint8_t pin_mask = (1U << pin_r) | (1U << pin_g) | (1U << pin_b);
port_val &= ~pin_mask;                       /* limpa só os 3 bits do LED */
port_val |= ...;                             /* coloca os novos valores */
PCA9535_WritePort(dev, port, port_val);      /* uma transação I2C só */
```

O `port_val` vinha de `dev->output_state[port]`, uma cópia local do registrador.
Isso evitava uma leitura extra no barramento e permitia acender o LD4 sem
apagar o LD3.

O mesmo esquema era usado para direção: `SetPinDirection()` alterava um bit de
`config_state[port]` e chamava `SetPortDirection()` para escrever o byte inteiro
de uma vez.

**Problema:** o cache era atualizado *antes* da chamada HAL. Se a escrita
falhasse, a cópia local ficava diferente do que estava no chip.

---

## 3. A camada LED — um descritor que aponta para o handle

```c
typedef struct {
    PCA9535_Handle_t *pca_dev;  /* qual expansor */
    PCA9535_Port_t port;        /* P0 ou P1 */
    uint8_t pin_r, pin_g, pin_b;/* onde o LED está ligado */
    uint8_t active_high;        /* polaridade elétrica */
} PCA_RGB_LED_t;
```

Esse struct **não era o LED em si**: era só o mapa de ligação. Ele apontava para
o handle do PCA:

```
led_LD4_status (descritor)
   .pca_dev ──► io_exp (handle)
                   .hi2c ──► hi2c4 (HAL)
                   .output_state[2]  <- cache
                   .config_state[2]  <- cache
```

Daí vinha a cadeia que parecia complicada:

```c
led->pca_dev->output_state[led->port]
```

O módulo LED não sabia nada de I2C; delegava tudo para o driver PCA através
desse ponteiro. As quatro instâncias eram apenas dados:

```c
led_LD4_status = { .pca_dev=&io_exp,  .port=P0, .pin_r=2, .pin_g=0, .pin_b=1, ... };
led_LD3_status = { .pca_dev=&io_exp,  .port=P0, .pin_r=5, .pin_g=3, .pin_b=4, ... };
led_LD8_status = { .pca_dev=&io_exp2, .port=P1, .pin_r=7, .pin_g=5, .pin_b=6, ... };
led_LD7_status = { .pca_dev=&io_exp2, .port=P1, .pin_r=4, .pin_g=3, .pin_b=3, ... };
```

> O LD7 estava com `pin_g` e `pin_b` no mesmo pino (3). O correto é `pin_b = 2`.

---

## 4. O que `PCA_RGB_SetColor()` fazia

Exemplo: `PCA_RGB_SetColor(&led_LD4_status, RGB_COLOR_GREEN)`:

1. `color = 0b001` → extração dos bits lógicos: R=0, G=1, B=0
   (`color & 0x04`, `0x01`, `0x02`).
2. Como `active_high = 0` (cátodo no PCA), invertia: R=1, G=0, B=1 —
   nível baixo acende.
3. Montava `pin_mask = (1<<2)|(1<<0)|(1<<1) = 0b00000111`.
4. Pegava `output_state[P0]` do cache, limpava esses 3 bits e recolocava R e B
   em 1, G em 0 → `0b11111110`.
5. `PCA9535_WritePort()` escrevia o byte inteiro em P0 e atualizava o cache.

Ou seja: **muitas rotações/shifts para converter cor lógica → bits físicos →
nível elétrico**, tudo para produzir um único byte.

---

## 5. Init e blink

`test_i2c_init()` inicializava os dois handles, chamava `PCA_RGB_Init()` nos
quatro LEDs (que escrevia o estado "apagado" antes de configurar os pinos como
saída, para evitar piscar) e configurava os dois pinos de botão como entrada.

`test_i2c_blink()` usava `PCA9535_ReadPin()` — que lia a porta inteira e
testava `1 << pin` — e, conforme o nível lido, chamava `PCA_RGB_SetColor()`
com VERDE/VERMELHO/OFF.

---

## 6. Por que foi simplificado

Pontos que geravam confusão:

- tudo no mesmo arquivo, com apenas 3 protótipos no header;
- `0x03`/`0x01` como "offset" em vez do endereço 7 bits;
- inversão de polaridade e extração de bits da cor em runtime;
- `active_high` + `pin_r/pin_g/pin_b` + enum de cor + máscaras;
- cache atualizado antes da chamada HAL;
- comentário dos botões dizendo "pressionado = nível 0" enquanto o código
  comparava com `GPIO_PIN_SET` (nível 1).

---

## 7. Como ficou

| Arquivo | Conteúdo |
|---|---|
| `pca_config.h` | endereços, direções e valores binários nomeados |
| `pca.h` / `pca.c` | `pca_init`, `pca_dir`, `pca_out`, `pca_in`; enums `pca_port_t` e `pca_dir_t` |
| `test_i2c.c` | `test_i2c_init()` e `test_i2c_blink()` |
| `scan_i2c.c` | varredura do barramento, isolada como diagnóstico |

A aplicação escreve direto o byte final da porta:

```c
pca_out(&io_exp, PCA_PORT0, LD4_GREEN);
```

Cada constante em `pca_config.h` já contém o valor completo de P0 ou P1,
incluindo o estado dos LEDs vizinhos e a polaridade ativo-baixo. Não há mais
descritor de LED, máscara calculada, inversão em runtime nem validação. O
desenvolvedor é responsável por escolher o valor binário correto.

Os botões continuam com nível ativo 1 (pressionado = `1`) e o B do LD7 em
P1.2. A API de porta usa `PCA_PORT0` / `PCA_PORT1` em vez de `0` / `1`.
