> **Novo material:** [Workshop: Claude Code para firmware STM32 + GitLab](README-claude-code.md) — livro em HTML, módulos, capstone e roteiro de vídeo.

# Guia de Estudos — Arquitetura de Software para Firmware STM32

## 1. Objetivo

Este documento define um roteiro de estudos para desenvolver conhecimentos em arquitetura de software embarcado utilizando C, STM32 e FreeRTOS.

O objetivo é compreender como estruturar projetos de firmware profissionais, escaláveis, modulares e reutilizáveis, evitando que toda a lógica do sistema fique concentrada em arquivos como `main.c`.

A arquitetura desejada deve permitir:

- Separação entre Core e Application.
- Reutilização de módulos entre diferentes projetos STM32.
- Abstração do hardware e do sistema operacional.
- Separação entre drivers, serviços e regras de negócio.
- Comunicação entre módulos por interfaces bem definidas.
- Utilização de callbacks e arquitetura orientada a eventos.
- Gerenciamento de threads, filas e sincronização.
- Facilidade para manutenção, testes e evolução do firmware.

### Resultado esperado

Ao finalizar os estudos, deverá ser possível projetar uma arquitetura na qual a Application utilize as funcionalidades disponibilizadas pelo Core sem precisar conhecer os detalhes de implementação dos drivers, periféricos, protocolos ou RTOS.

---

# 2. Visão geral da arquitetura desejada

A arquitetura de referência será baseada em camadas de abstração.

```text
                    APPLICATION
                         |
                 Application Layer
                         |
          +--------------+--------------+
          |              |              |
       Commands         Tasks        State Machine
          |              |              |
          +--------------+--------------+
                         |
                     SHARED CORE
                         |
       +-----------------+-----------------+
       |                 |                 |
    Services         Middleware          OSAL
       |                 |                 |
       +-----------------+-----------------+
                         |
                  Device Manager
                         |
                 Driver Interfaces
                         |
                    PLATFORM
                         |
               BSP / Hardware Drivers
                         |
                     HARDWARE
                         |
                  STM32 / Peripherals
```

## 2.1. Application

Responsável pela lógica específica do produto.

Exemplos:

- Inicialização da aplicação.
- Criação e organização das tasks.
- Registro dos comandos MQTT.
- Regras de aquisição.
- Processamento dos dados.
- Controle dos estados do equipamento.
- Orquestração dos serviços.

A Application não deve conhecer detalhes de implementação do hardware.

## 2.2. Shared Core

Biblioteca reutilizável que disponibiliza funcionalidades comuns aos projetos.

Exemplos:

- Logger.
- Utils.
- Buffers.
- Queue.
- Thread.
- Mutex.
- Event Dispatcher.
- Command Dispatcher.
- Device Manager.
- Drivers e interfaces.
- Serviços genéricos.

O Core não deve depender da Application.

## 2.3. Platform / BSP

Responsável pelas particularidades da plataforma de hardware.

Exemplos:

- Inicialização de clocks.
- Configuração de GPIO.
- SPI, I2C e UART.
- DMA.
- Timers.
- Interrupções.
- Implementação específica para STM32H7 ou STM32C0.

## 2.4. OSAL

Operating System Abstraction Layer.

Camada que abstrai as funcionalidades do sistema operacional.

Exemplos:

- Criação de threads.
- Delay.
- Mutex.
- Semáforos.
- Filas.
- Notificações.

A ideia é evitar que todos os módulos precisem conhecer diretamente as APIs do FreeRTOS.

---

# 3. Os nove conceitos fundamentais

## 3.1. Layered Architecture — Arquitetura em Camadas

### O que estudar

Arquitetura de software baseada na separação de responsabilidades em diferentes níveis.

Cada camada possui responsabilidades específicas e disponibiliza interfaces para as camadas superiores.

### Conceitos importantes

- Separation of Concerns.
- Abstraction Layers.
- Public Interfaces.
- Internal Implementation.
- Dependency Direction.
- Loose Coupling.

### Aplicação no STM32

Separar o firmware em:

1. Application.
2. Services.
3. Middleware.
4. Device Manager.
5. Drivers.
6. BSP / Platform.
7. Hardware.

### Exercício prático

Criar um projeto com três camadas:

```text
project/
├── application/
├── core/
│   ├── services/
│   └── drivers/
└── platform/
```

A Application deve controlar um GPIO sem acessar diretamente os registradores do STM32.

---

## 3.2. Modular Architecture — Arquitetura Modular

### O que estudar

Como dividir um software em módulos independentes, com responsabilidades bem definidas.

Um módulo deve possuir:

- Interface pública.
- Implementação privada.
- Responsabilidade específica.
- Dependências explícitas.
- Baixo acoplamento.

### Conceitos importantes

- Modular Programming.
- Encapsulation.
- Information Hiding.
- Cohesion.
- Coupling.
- Reusable Components.

### Aplicação no STM32

Exemplo:

```text
core/
├── logger/
│   ├── logger.h
│   └── logger.c
│
├── buffer/
│   ├── buffer.h
│   └── buffer.c
│
├── mqtt/
│   ├── mqtt.h
│   └── mqtt.c
│
└── adc/
    ├── adc.h
    └── adc.c
```

Cada módulo deve poder ser utilizado independentemente dos demais, respeitando suas dependências.

### Exercício prático

Implementar Logger, Buffer e GPIO como módulos independentes.

---

## 3.3. API Design — Projeto de Interfaces

### O que estudar

Como criar interfaces públicas consistentes para comunicação entre módulos.

Em C, os arquivos `.h` representam uma parte importante do contrato entre os módulos.

### Conceitos importantes

- Header Files.
- Public API.
- Private API.
- Opaque Pointers.
- Structs.
- Function Pointers.
- Error Handling.
- API Versioning.

### Aplicação no STM32

Exemplo:

```c
// core_adc.h

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
} CoreADCConfig;

bool core_adc_init(const CoreADCConfig *config);

bool core_adc_start(void);

bool core_adc_stop(void);

bool core_adc_read(uint8_t channel, int32_t *value);
```

A Application conhece apenas essas funções.

A implementação interna pode utilizar SPI, DMA, GPIO e registradores.

### Exercício prático

Criar uma API para um módulo ADC e outra para Logger.

O objetivo é permitir que a Application utilize as funcionalidades sem conhecer os arquivos `.c` internos.

---

## 3.4. Dependency Inversion — Inversão de Dependência

### O que estudar

Princípio de projeto segundo o qual módulos de alto nível não devem depender diretamente de implementações de baixo nível.

Ambos devem depender de abstrações.

É um dos princípios SOLID.

### Conceitos importantes

- SOLID Principles.
- Dependency Inversion Principle (DIP).
- Dependency Injection.
- Interface-Based Design.
- Function Pointers.
- Hardware Abstraction.

### Aplicação no STM32

Imagine um serviço que utiliza um ADC.

Uma implementação fortemente acoplada seria:

```c
void acquisition_start(void)
{
    ads131m08_start();
}
```

O serviço depende diretamente de um driver específico.

Uma alternativa é definir uma interface:

```c
typedef struct {

    bool (*init)(void);

    bool (*start)(void);

    bool (*read)(int32_t *buffer);

    bool (*stop)(void);

} ADC_Interface;
```

O serviço recebe essa interface:

```c
void acquisition_init(
    const ADC_Interface *adc
);
```

Assim, o serviço não precisa conhecer a implementação específica do ADC.

### Exercício prático

Criar uma interface genérica de ADC e implementar duas versões simuladas ou específicas de hardware.

O serviço de aquisição deve funcionar sem precisar ser modificado.

---

## 3.5. Callback Pattern — Padrão de Callback

### O que estudar

Como utilizar ponteiros para funções para permitir que um módulo execute uma função registrada por outro módulo.

### Conceitos importantes

- Function Pointers.
- Callback Registration.
- Callback Invocation.
- Event Handlers.
- Observer Pattern.
- Event Dispatcher.

### Aplicação no STM32

Exemplo de registro de callback:

```c
typedef void (*MqttCallback)(
    const char *payload
);

void mqtt_register_callback(
    const char *command,
    MqttCallback callback
);
```

A Application registra:

```c
mqtt_register_callback(
    "start_acquisition",
    acquisition_start_callback
);
```

Quando uma mensagem MQTT correspondente chega, o módulo MQTT executa o callback registrado.

### Exercício prático

Criar um sistema de registro de callbacks para:

- START.
- STOP.
- RESET.
- GET_STATUS.

O dispatcher deve identificar o comando e executar a função correspondente.

---

## 3.6. Event-Driven Architecture — Arquitetura Orientada a Eventos

### O que estudar

Arquitetura na qual os módulos reagem a eventos em vez de depender exclusivamente de chamadas diretas entre funções.

### Conceitos importantes

- Event-Driven Programming.
- Event Dispatcher.
- Event Queue.
- Observer Pattern.
- Publisher / Subscriber.
- Command Pattern.
- Asynchronous Communication.

### Aplicação no STM32

Exemplo de fluxo:

```text
MQTT Message
     |
     v
MQTT Callback
     |
     v
Command Dispatcher
     |
     v
FreeRTOS Queue
     |
     v
Application Task
     |
     v
Acquisition Service
```

O callback MQTT recebe a mensagem, interpreta o comando e envia uma estrutura para uma fila.

A task responsável pela aquisição recebe o comando e executa a operação.

### Exercício prático

Criar um Event Dispatcher que permita:

- Registrar eventos.
- Registrar handlers.
- Publicar eventos.
- Distribuir eventos aos módulos interessados.

Implementar uma fila para comunicação entre tasks.

---

## 3.7. RTOS Abstraction — Abstração do Sistema Operacional

### O que estudar

Como abstrair funcionalidades do RTOS para evitar dependência direta do FreeRTOS em todos os módulos.

### Conceitos importantes

- RTOS.
- Task Management.
- Scheduling.
- Queues.
- Mutexes.
- Semaphores.
- Event Groups.
- Task Notifications.
- Critical Sections.
- Synchronization.

### Aplicação no STM32

Criar uma camada OSAL:

```text
core/
└── osal/
    ├── osal_thread.h
    ├── osal_thread.c
    ├── osal_queue.h
    ├── osal_queue.c
    ├── osal_mutex.h
    └── osal_mutex.c
```

Exemplo de API:

```c
typedef void (*OSAL_ThreadFunction)(void *argument);

bool osal_thread_create(
    const char *name,
    OSAL_ThreadFunction function,
    void *argument,
    uint32_t stack_size,
    uint32_t priority
);

void osal_delay(uint32_t milliseconds);
```

A implementação pode utilizar FreeRTOS internamente.

### Exercício prático

Implementar uma abstração de:

- Thread.
- Queue.
- Mutex.
- Delay.

Criar duas tasks que trocam informações através de uma fila.

---

## 3.8. State Machines — Máquinas de Estados

### O que estudar

Como representar o comportamento de um equipamento por meio de estados e transições.

É especialmente importante em firmware embarcado, pois evita lógica complexa baseada em vários booleanos independentes.

### Conceitos importantes

- Finite State Machine (FSM).
- State Transition.
- Events.
- Guards.
- Entry / Exit Actions.
- Hierarchical State Machines.

### Aplicação no STM32

Exemplo de estados de um equipamento de aquisição:

```text
          INITIALIZATION
                 |
                 v
               IDLE
                 |
            START COMMAND
                 |
                 v
             ACQUIRING
                 |
             STOP COMMAND
                 |
                 v
               IDLE

        Qualquer estado pode
        apresentar uma falha.

                 |
                 v
               ERROR
```

### Exercício prático

Implementar uma máquina de estados com:

- INITIALIZING.
- IDLE.
- ACQUIRING.
- PROCESSING.
- ERROR.

Os eventos devem provocar as transições de estado.

---

## 3.9. Design Patterns — Padrões de Projeto

### O que estudar

Soluções recorrentes para problemas comuns de arquitetura e organização de software.

Não é necessário implementar todos os padrões existentes. O importante é reconhecer quando cada um resolve um problema real.

### Padrões prioritários

| Pattern | Aplicação no firmware |
|---|---|
| Singleton | Instância única de um serviço, quando realmente necessária |
| Factory | Criação de drivers ou dispositivos |
| Observer | Notificação de eventos |
| Strategy | Alternar implementações de hardware |
| Command | Representar comandos como estruturas |
| Adapter | Adaptar APIs diferentes a uma interface comum |
| Facade | Disponibilizar uma API simplificada do Core |
| State | Organizar máquinas de estados |

### Aplicação no STM32

Exemplo de Facade:

```c
// core.h

bool core_init(void);

bool core_start(void);

void core_process(void);
```

A Application utiliza uma interface simplificada, enquanto o Core organiza internamente seus módulos.

### Exercício prático

Implementar:

1. Facade para inicialização do Core.
2. Strategy para alternar implementações de ADC.
3. Command para representar comandos MQTT.
4. Observer para distribuir eventos.

---

# 4. Conceitos complementares importantes

Além dos nove conceitos principais, estudar os seguintes assuntos ajudará a compreender arquiteturas embarcadas profissionais.

| Conceito | Descrição |
|---|---|
| BSP — Board Support Package | Recursos e inicialização específicos da placa |
| HAL — Hardware Abstraction Layer | Abstração dos periféricos de hardware |
| LL — Low Layer | Acesso de baixo nível aos periféricos |
| OSAL — Operating System Abstraction Layer | Abstração do RTOS |
| Middleware | Componentes intermediários, como MQTT e protocolos |
| Device Manager | Gerenciamento de dispositivos registrados |
| Driver | Implementação da comunicação com o hardware |
| Service Layer | Funcionalidades construídas sobre drivers e outros módulos |
| Dependency Injection | Fornecimento explícito de dependências |
| Unit Testing | Testes de módulos individualmente |
| Mocking | Simulação de dependências para testes |
| CMake | Organização e compilação dos módulos |
| CI/CD | Automatização de compilação e testes |

## 4.1. Driver versus Device Manager versus Service

É importante compreender a diferença:

**Driver**

Conhece os detalhes de comunicação com o hardware.

Exemplo: configurar o ADS131M08 através de SPI.

**Device Manager**

Gerencia os dispositivos disponíveis, seu registro, inicialização, configuração e estado.

Exemplo: verificar se o ADS131M08 está registrado e pronto para utilização.

**Service**

Implementa funcionalidades utilizando dispositivos e outros módulos.

Exemplo: gerenciar a aquisição de amostras de um ADC.

**Application**

Define o comportamento específico do produto.

Exemplo: iniciar aquisição quando receber determinado comando MQTT.

---

# 5. Arquitetura de referência para o projeto

A seguinte estrutura pode ser utilizada como ponto de partida para um projeto com Core reutilizável e Application independente.

```text
firmware/
│
├── shared-core/
│   │
│   ├── include/
│   │   ├── core.h
│   │   ├── core_config.h
│   │   └── core_types.h
│   │
│   ├── src/
│   │   │
│   │   ├── services/
│   │   │   ├── acquisition/
│   │   │   └── sensor/
│   │   │
│   │   ├── drivers/
│   │   │   ├── adc/
│   │   │   ├── spi/
│   │   │   ├── uart/
│   │   │   └── gpio/
│   │   │
│   │   ├── communication/
│   │   │   ├── mqtt/
│   │   │   └── can/
│   │   │
│   │   ├── device_manager/
│   │   │
│   │   ├── osal/
│   │   │   ├── thread/
│   │   │   ├── queue/
│   │   │   └── mutex/
│   │   │
│   │   ├── events/
│   │   ├── utils/
│   │   └── core.c
│   │
│   └── CMakeLists.txt
│
├── platform/
│   ├── stm32h7/
│   │   ├── bsp/
│   │   ├── drivers/
│   │   └── osal/
│   │
│   └── stm32c0/
│       ├── bsp/
│       ├── drivers/
│       └── osal/
│
└── application/
    │
    ├── include/
    │
    ├── src/
    │   ├── main.c
    │   ├── app.c
    │   ├── commands/
    │   │   └── mqtt_commands.c
    │   │
    │   ├── tasks/
    │   │   ├── acquisition_task.c
    │   │   ├── processing_task.c
    │   │   └── communication_task.c
    │   │
    │   └── state_machine/
    │
    └── CMakeLists.txt
```

Observação: essa é uma estrutura conceitual. Nem todo projeto precisa de todos esses diretórios. A implementação deve acompanhar as responsabilidades reais dos módulos.

---

# 6. Roteiro de estudo progressivo

O roteiro está dividido em cinco etapas. A ordem foi escolhida para que os conhecimentos sejam construídos progressivamente.

## Etapa 1 — Fundamentos de modularização em C

**Objetivo:** aprender a organizar o código em módulos independentes.

Estudar:

- Headers e arquivos `.c`.
- Include Guards.
- `static` e visibilidade.
- `struct` e `typedef`.
- Function Pointers.
- Encapsulamento em C.
- Organização de bibliotecas.

Prática:

Criar um pequeno Core contendo:

- Logger.
- Buffer.
- Utils.
- GPIO.

Resultado esperado:

A Application consegue utilizar os módulos sem conhecer suas implementações internas.

## Etapa 2 — Arquitetura em camadas

**Objetivo:** compreender como organizar as responsabilidades do firmware.

Estudar:

- Layered Architecture.
- Modular Architecture.
- API Design.
- Dependency Inversion.
- BSP.
- HAL e LL.
- Driver Interfaces.
- Device Manager.
- Service Layer.

Prática:

Criar um driver ADC com uma interface genérica e um serviço de aquisição independente do hardware.

Resultado esperado:

O serviço de aquisição funciona sem conhecer os detalhes do ADC utilizado.

## Etapa 3 — Concorrência e FreeRTOS

**Objetivo:** organizar a execução de diferentes funcionalidades simultaneamente.

Estudar:

- RTOS.
- Task Scheduling.
- Queues.
- Mutex.
- Semaphore.
- Event Groups.
- Task Notifications.
- OSAL.
- Critical Sections.

Prática:

Criar:

- Acquisition Task.
- Processing Task.
- Communication Task.

Utilizar filas para comunicação entre as tasks.

Resultado esperado:

As tasks trabalham independentemente e se comunicam através de mecanismos controlados.

## Etapa 4 — Callbacks e arquitetura orientada a eventos

**Objetivo:** criar um sistema flexível de comunicação entre módulos.

Estudar:

- Callback Pattern.
- Event Dispatcher.
- Observer Pattern.
- Command Pattern.
- Event Queue.
- Publisher / Subscriber.
- Message Handling.

Prática:

Criar um sistema de comandos MQTT.

Comandos:

- START_ACQUISITION.
- STOP_ACQUISITION.
- RESET_DEVICE.
- GET_STATUS.

Fluxo esperado:

```text
MQTT
 |
 v
Callback
 |
 v
Command Dispatcher
 |
 v
Command Queue
 |
 v
Application Task
 |
 v
Service
 |
 v
Driver
```

Resultado esperado:

A comunicação MQTT não depende diretamente da implementação dos serviços de aquisição.

## Etapa 5 — Qualidade, testes e escalabilidade

**Objetivo:** preparar o Core para ser utilizado em diferentes projetos.

Estudar:

- Unit Testing.
- Integration Testing.
- Mock Drivers.
- Dependency Injection.
- CMake.
- Static Libraries.
- Git Submodules.
- CI/CD.
- Documentation.
- Software Architecture Diagrams.

Prática:

1. Compilar o Core independentemente da Application.
2. Criar testes para Logger, Buffer e Command Dispatcher.
3. Simular drivers sem precisar de hardware.
4. Criar implementações específicas para STM32H7 e STM32C0.
5. Automatizar build e testes.
6. Documentar as APIs.

Resultado esperado:

Um Core reutilizável, testável e independente das regras de negócio de cada produto.

---

# 7. Projeto final de aprendizado

Ao concluir as etapas, implementar um projeto experimental de firmware para STM32.

## Equipamento de referência

Sistema de aquisição de vibração com:

- STM32H7.
- FreeRTOS.
- ADC externo ADS131M08.
- Comunicação MQTT.
- Aquisição via SPI.
- Transferência de dados por DMA.

## Funcionalidades

1. Inicialização dos dispositivos.
2. Gerenciamento dos estados dos dispositivos.
3. Registro de comandos MQTT.
4. Inicialização e controle das tasks.
5. Aquisição de amostras.
6. Processamento dos dados.
7. Comunicação entre módulos.
8. Tratamento de falhas.
9. Recuperação de dispositivos.

## Critérios de conclusão

- [ ] Application não acessa registradores diretamente.
- [ ] Application não depende da implementação interna dos drivers.
- [ ] Core possui interfaces públicas bem definidas.
- [ ] Drivers são independentes das regras de negócio.
- [ ] OSAL abstrai as operações do FreeRTOS utilizadas pelos módulos.
- [ ] Comandos MQTT são encaminhados por uma estrutura de dispatch.
- [ ] Operações demoradas são executadas em tasks apropriadas.
- [ ] Comunicação entre tasks utiliza mecanismos do RTOS.
- [ ] Device Manager controla o estado dos dispositivos registrados.
- [ ] Serviços podem ser testados sem o hardware real.
- [ ] O Core pode ser reutilizado em outro projeto STM32.

---

# 8. Referências para estudo

Priorizar documentação oficial e materiais que apresentem exemplos práticos.

| Referência | Assunto | Link |
|---|---|---|
| FreeRTOS | Tasks, Queues e sincronização | https://www.freertos.org/Documentation/ |
| Zephyr RTOS | Arquitetura modular e subsistemas | https://docs.zephyrproject.org/latest/ |
| STMicroelectronics | CMSIS, HAL, LL e STM32 | https://www.st.com/ |
| Memfault Interrupt | Arquitetura e qualidade de firmware | https://interrupt.memfault.com/blog/ |
| Embedded Artistry | Arquitetura e desenvolvimento embarcado | https://embeddedartistry.com/ |
| CMake | Organização de projetos C | https://cmake.org/documentation/ |
| Unity | Framework de testes para C | https://github.com/ThrowTheSwitch/Unity |

## Livros recomendados

1. **Making Embedded Systems — Elecia White:** fundamentos de arquitetura e desenvolvimento de sistemas embarcados.
2. **Embedded Systems Architecture — Daniele Lacamera:** organização e arquitetura de sistemas embarcados.
3. **Design Patterns: Elements of Reusable Object-Oriented Software — Gang of Four:** fundamentos de padrões de projeto.
4. **Test Driven Development for Embedded C — James W. Grenning:** testes automatizados e desenvolvimento de firmware testável.

---

# 9. Estratégia de aprendizado

A recomendação é não estudar todos os conceitos apenas teoricamente.

Utilizar o seguinte ciclo:

1. Estudar o conceito.
2. Identificar um problema real no firmware.
3. Implementar uma pequena PoC em C.
4. Testar a implementação.
5. Documentar a arquitetura.
6. Avaliar se a abstração realmente reduziu o acoplamento.
7. Incorporar ao Shared Core somente depois de validar a solução.

## Princípios que devem orientar o desenvolvimento

- Preferir simplicidade a abstrações desnecessárias.
- Evitar dependências circulares.
- Não misturar regras de negócio com drivers.
- Não permitir que o Core dependa da Application.
- Definir contratos claros entre módulos.
- Evitar criar uma camada de abstração sem uma necessidade concreta.
- Manter o código testável sem hardware sempre que possível.
- Priorizar reutilização real, não apenas compartilhamento de arquivos.

**Objetivo final:** desenvolver capacidade de projetar arquiteturas de firmware embarcado escaláveis, modulares e reutilizáveis, utilizando C e FreeRTOS, com separação clara entre Application, Core, Platform e Hardware.