# Módulo 05 — TDD e testes unitários em C para STM32

**Tempo:** 40 min · **Entrega:** teste vermelho→verde no host + `Stop` hook exigindo a suíte.

---

## 1. Por que testar no host

O firmware roda no STM32, mas a **lógica** não precisa. Separe:

```text
app/                     ← regras de negócio: testável no host
  blink.c                   (sem HAL direta)
platform/stm32f1/        ← HAL, registradores, st-link
test/                    ← Unity + CMock, roda no PC
```

O serviço recebe uma interface, não a HAL:

```c
// app/gpio_port.h
typedef struct {
    void (*write)(uint16_t pin, int value);
    int  (*read)(uint16_t pin);
} GpioPort;

void blink_init(const GpioPort *port);
void blink_toggle(void);
int  blink_is_error(void);
```

No host, você injeta um mock. Na placa, você injeta a HAL. **A regra não muda.**

## 2. Ceedling em 10 minutos

```bash
pipx install ceedling
ceedling new blink          # cria estrutura (ou use o capstone/)
cd blink
ceedling test:all
```

`project.yml` mínimo:

```yaml
:project:
  :use_exceptions: FALSE
  :use_test_preprocessor: TRUE
  :build_root: build
  :release_build: TRUE

:paths:
  :test:
    - test/**
  :source:
    - app/**
  :include:
    - app/**

:plugins:
  :load_paths: []
  :enabled:
    - stdout_pretty_tests_report
    - module_generator
```

Estrutura:

```text
blink/
├── app/
│   ├── blink.c
│   └── blink.h
├── test/
│   ├── test_blink.c
│   └── mocks/
│       └── mock_gpio_port.c   (gerado pelo CMock)
└── project.yml
```

## 3. Ciclo TDD com o agente

### 3.1 Vermelho

Prompt:

```text
Use a skill embedded-unit-tests.
Crie test/test_blink.c para REQ-001.
Critérios: LED alterna a cada 500ms no IDLE; fica aceso contínuo em ERROR.
A porta GPIO é injetada via GpioPort. Mock de write.
Rode ceedling test:test_blink e mostre a falha. Não implemente blink.c ainda.
```

Teste esperado:

```c
#include "unity.h"
#include "blink.h"
#include "mock_gpio_port.h"

static GpioPort port;

void setUp(void) {
    port.write = gpio_port_write_Stub;   // gerado pelo CMock
    port.read  = gpio_port_read_Stub;
    blink_init(&port);
}

void tearDown(void) {}

void test_blink_starts_idle_with_led_off(void) {
    gpio_port_write_Expect(LED_PIN, 0);
    // primeira chamada acontece no init
    TEST_ASSERT_FALSE(blink_is_error());
}

void test_blink_error_keeps_led_on(void) {
    blink_enter_error();
    for (int i = 0; i < 3; i++) {
        gpio_port_write_Expect(LED_PIN, 1);
        blink_toggle();
    }
    TEST_ASSERT_TRUE(blink_is_error());
}
```

Na primeira rodada: `undefined reference to blink_enter_error`. **Isso é o sucesso do passo.**

### 3.2 Verde

```text
Agora implemente o mínimo em app/blink.c para passar no teste.
Sem malloc. Sem printf. Sem HAL. Rode ceedling test:all até ficar verde.
```

```c
#include "blink.h"

static const GpioPort *g_port;
static int g_error;
static int g_level;

void blink_init(const GpioPort *port) {
    g_port = port;
    g_error = 0;
    g_level = 0;
    g_port->write(LED_PIN, g_level);
}

void blink_enter_error(void) {
    g_error = 1;
}

void blink_toggle(void) {
    g_level = g_error ? 1 : (g_level ? 0 : 1);
    g_port->write(LED_PIN, g_level);
}

int blink_is_error(void) { return g_error; }
```

### 3.3 Refactor com verificação

O `Stop` hook do Módulo 03 garante que nenhum turno termina com a suíte vermelha. O agente pode refatorar, mas **não consegue dizer "pronto"** sem `ceedling test:all` passar.

## 4. Casos de borda que o modelo esquece

Guarde isto em `.claude/skills/embedded-unit-tests/references/edge-cases.md`:

| Domínio | Caso | Por que importa |
|---|---|---|
| Timer 32-bit @1 kHz | rollover em 49,7 dias | comparação com valor absoluto quebra |
| Buffer circular | `head == tail` com cheio vs vazio | off-by-one clássico |
| ISR | reentrância / variável `volatile` | corrupção silenciosa |
| DMA | underrun/overrun | dado travado ou repetido |
| ADC | 0, `UINT32_MAX`, canal desconectado | leitura fantasy |
| I2C | NACK, clock stretching, busy | travamento do barramento |
| Reset | watchdog, brownout | estado inicial inválido |

> O artigo da Beningo sobre skills cita explicitamente rollover de timer de 32 bits, wraparound de buffer circular e reentrância de ISR. O artigo da BTC mostra o outro lado: **se a IA não consegue gerar o teste a partir do requisito, o requisito está ambíguo.**

## 5. Mock de HAL sem perder a sanidade

Se você precisar testar algo que chama `HAL_GPIO_WritePin`, configure o CMock para gerar o mock do header da HAL — **mas nunca inclua a HAL real no teste**:

```yaml
:cmock:
  :mock_prefix: mock_
  :plugins:
    - ignore
    - callback
    - expect_any_args
  :treat_externs: :include
```

Regra para o agente (vai no `CLAUDE.md`):

```markdown
- Em test/, nunca inclua stm32f1xx_hal.h. Use mock_*.h.
- Se um teste precisar de hardware real, ele NÃO é unitário: mova para tests/hil/.
```

## 6. Cobertura com propósito

```bash
ceedling gcov:all
ceedling utils:gcov
```

Não persiga 100%. Priorize:

1. todo ramo de máquina de estados;
2. todo cálculo com timer/contador;
3. toda conversão de unidade;
4. tratamento de erro de periférico.

Cobertura de `Drivers/` gerado não agrega valor — é código de terceiros.

## 7. Exercício 05

1. No `capstone/`, rode `ceedling test:all` — deve falhar (o teste existe, a implementação não).
2. Peça ao Claude para implementar o mínimo. Confira que ele **não** mexeu em `Drivers/`.
3. Peça: *"adicione o caso de rollover 0xFFFFFFFF → 0 do timer"*. O agente deve escrever o teste primeiro.
4. Tente finalizar uma mensagem com a suíte vermelha (quebre uma asserção). O `Stop` hook deve devolver o erro e não deixar terminar.

## 8. Checklist

- [ ] Teste roda no host, sem placa.
- [ ] Regra de negócio não inclui header de HAL.
- [ ] Escrevi o teste **antes** da implementação.
- [ ] `edge-cases.md` tem pelo menos 6 entradas.
- [ ] `Stop` hook barra turno com teste vermelho.
- [ ] Cobertura focada em lógica, não em código gerado.

**Próximo:** [Módulo 06 — CI/CD no GitLab](06-gitlab-ci.md).
