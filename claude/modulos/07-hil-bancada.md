# Módulo 07 — HIL: fechando o loop entre o agente e o hardware

**Tempo:** 35 min · **Entrega:** teste HIL com lock, log e evidência no MR.

---

## 1. O problema estrutural

> "Compila, passa no unit test, grava o firmware... e falha."

O artigo *AI in hardware-in-the-loop testing: the loop embedded never closed* resume bem: o agente não vê o sinal, o rail de 3,3 V, a prioridade de IRQ sob carga ou o NACK do I2C. Enquanto o loop parar no `gcc`, ele está adivinhando.

HIL é o mecanismo que transforma **suposição** em **observação**.

## 2. As quatro camadas do harness

O Embedded AI Harness do Andreas Spiess (vídeo Nov/2026, ESP32) organiza o loop em quatro camadas. O alvo muda, o desenho não:

```text
HOST    → compila, roda unit tests, gera o binário, decide o próximo passo
TARGET  → STM32 com ST-Link; grava e inicializa
BENCH   → Raspberry Pi / Nanopi: fontes, cargas, analisador lógico, relays, UART
FIELD   → telemetria do equipamento já instalado (opcional, valioso)
```

Com essa separação, o agente consegue receber **evidência estruturada** de cada camada em vez de um log gigante.

Camada adicional recomendada: **fault injection** — cortar o rail, injetar NACK, resetar por watchdog. É o que separa "teste bonito" de "teste que sobrevive à produção".

## 3. Test farm com GitLab Runner

Baseado na série DevOps for Embedded (stupid-projects), com STM32F103:

```text
Nanopi (gitlab-runner, tag stm32-bench)
 ├── /dev/ttyUSB0        → UART do STM32
 ├── ST-Link USB         → st-flash
 ├── GPIO expander I2C   → mede o pino sob teste
 └── lock file           → /tmp/stm32-bench.lock
```

Regras de ouro:

1. **Um job por placa por vez.** I2C e flash não são reentrantes.
2. **Sempre resetar para estado conhecido** antes do teste.
3. **Sempre desligar a carga** no `after_script`.
4. **Gravar o log mesmo na falha** (`artifacts: when: always`).
5. **Não compartilhar runner de build com runner de bancada.**

## 4. Exemplo de teste HIL em pytest

```python
# tests/hil/test_gpio.py
import subprocess, time, serial, pytest

UART = "/dev/ttyUSB0"
BIN = "build/firmware.bin"
ADDR = "0x8000000"

@pytest.fixture(scope="session", autouse=True)
def flash_and_boot():
    subprocess.run(
        ["st-flash", "--reset", "write", BIN, ADDR], check=True
    )
    time.sleep(0.5)
    yield
    # after: desliga periféricos / volta ao estado seguro

def test_led_blinks_at_500ms():
    with serial.Serial(UART, 115200, timeout=3) as ser:
        edges = []
        start = time.monotonic()
        while time.monotonic() - start < 3 and len(edges) < 4:
            line = ser.readline().decode(errors="replace").strip()
            if line.startswith("LED:"):
                edges.append((line, time.monotonic()))
        assert len(edges) >= 4, f"arestas insuficientes: {edges}"
        deltas = [t1 - t0 for (_, t0), (_, t1) in zip(edges, edges[1:])]
        assert all(0.45 <= d <= 0.55 for d in deltas), deltas
```

No `.gitlab-ci.yml`:

```yaml
hil:gpio:
  stage: hil
  tags: [stm32-bench]
  when: manual
  dependencies: [flash:bench]
  script:
    - flock /tmp/stm32-bench.lock -c "pytest -q tests/hil --junitxml=reports/hil/results.xml"
  artifacts:
    when: always
    paths: [reports/hil/]
    reports:
      junit: reports/hil/results.xml
```

## 5. O que o agente deve receber de volta

Não despeje o log cru. Use um script de resumo (o mesmo padrão do hook de filtro):

```bash
# scripts/hil-summary.sh
grep -E 'FAIL|ERROR|assert|timeout|NACK|watchdog' reports/hil/*.log \
  | tail -n 60
```

E no prompt do agente:

```text
Analise reports/hil/summary.txt.
Para cada falha: REQ afetado, evidência observada, causa raiz provável.
Não proponha mudança em Drivers/. Não grave firmware.
Termine em no máximo 10 linhas.
```

Isso é o `hil-analyst` do Módulo 04. O custo fica baixo (Haiku + resumo) e o sinal alto.

## 6. Fault injection que vale a pena

| Falha injetada | O que você valida |
|---|---|
| Cortar 3,3 V por 10 ms | brownout, estado inicial, reset limpo |
| NACK no I2C | retry com backoff, timeout, liberação do barramento |
| Watchdog estourado | recuperação e logging da causa |
| DMA underrun | dado travado/repetido detectado |
| UART saturado | perda de mensagem sem corromper o estado |

Cada injetado deve ter um `REQ-###` correspondente. Se não tem, você está testando curiosidade, não requisito.

## 7. Exercício 07

1. No `capstone/`, rode `pytest -q tests/hil` (pode ser com `--collect-only` se não houver placa).
2. Adicione `flock` ao job e rode duas pipelines em paralelo — o segundo deve esperar.
3. Quebre o delay para 300 ms e confirme que o teste falha com deltas fora da faixa.
4. Peça ao `hil-analyst` para resumir a falha e abrir uma issue com `REQ-001` via `glab`.

## 8. Checklist

- [ ] Runner de bancada separado, com tag.
- [ ] Lock de hardware implementado e testado.
- [ ] Reset para estado conhecido antes de cada teste.
- [ ] Limpeza no `after_script`.
- [ ] Log e junit salvos mesmo na falha.
- [ ] Resumo estruturado voltando para o agente.
- [ ] Pelo menos um caso de fault injection ligado a um REQ.

**Próximo:** [Módulo 08 — Rastreabilidade](08-rastreabilidade.md).
