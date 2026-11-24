# Mock de hardware com CMock

## Regra

Nenhum teste em `test/` inclui `stm32f1xx_hal.h`. O mock é gerado a partir do header e substitui a implementação real.

## Configuração mínima em `project.yml`

```yaml
:cmock:
  :mock_prefix: mock_
  :plugins:
    - ignore
    - callback
    - expect_any_args
  :treat_externs: :include
```

## Mock de uma HAL de GPIO

Dado `app/gpio_port.h`:

```c
void gpio_port_write(uint16_t pin, int value);
int  gpio_port_read(uint16_t pin);
```

O CMock gera `mock_gpio_port.h` com:

```c
void gpio_port_write_Expect(uint16_t pin, int value);
void gpio_port_write_Stub(uint16_t pin, int value);
int  gpio_port_read_ExpectAndReturn(uint16_t pin, int cmock_retval);
```

No teste:

```c
#include "mock_gpio_port.h"

static GpioPort port = {
    .write = gpio_port_write_Stub,
    .read  = gpio_port_read_Stub,
};
```

## Quando o mock esconde o bug

Mockar a HAL não prova que o pino está certo. Por isso todo comportamento que depende de nível elétrico, tempo ou barramento precisa de teste HIL (`tests/hil/`). Se você mockou o tempo, você testou a lógica — não o timing.
