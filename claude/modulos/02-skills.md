# Módulo 02 — Skills: procedimentos que carregam só quando precisam

**Tempo:** 35 min · **Entrega:** skill `/embedded-unit-tests` funcional no `capstone/`.

---

## 1. O que uma skill é (e o que não é)

Uma skill é um pacote de instruções com nome e descrição. Claude vê **nome + descrição** em toda sessão; o conteúdo completo só entra em contexto quando a tarefa combina com a descrição (ou quando você chama `/nome`).

```text
.claude/skills/
└── embedded-unit-tests/
    ├── SKILL.md              # obrigatório: nome, descrição, passos
    └── references/
        ├── mocking.md        # carregado só se necessário
        └── edge-cases.md     # carregado só se necessário
```

Isso é **progressive disclosure**: o custo em contexto é quase zero até a skill ser usada. Por isso procedimento longo (release, auditoria, escrita de ADR) vai para skill, não para `CLAUDE.md`.

| Mecanismo | Quando carrega | Garante? | Melhor uso |
|---|---|---|---|
| prompt | só naquele turno | não | pedido pontual |
| `CLAUDE.md` | toda sessão | não | restrições curtas e universais |
| `.claude/rules/*.md` | quando o caminho bate | não | regra de uma pasta |
| **skill** | quando a descrição bate / `/chamada` | não | procedimento com etapas |
| **hook / deny rule** | sempre, pelo harness | **sim** | "nunca" e "sempre" |

> Skill ainda é instrução para o modelo. É ótima para consistência, **não** para imposição. Imposição é Módulo 03.

## 2. `SKILL.md` mínima

```markdown
---
name: embedded-unit-tests
description: Escreve testes unitários em C para código STM32 com Unity/CMock.
  Use quando o usuário pedir teste, TDD, cobertura ou região crítica de firmware.
---

# Embedded unit tests

1. Descubra o alvo: qual `.c`, qual função, qual efeito observável.
2. Escreva PRIMEIRO o teste em `test/test_<modulo>.c`.
3. Mock de hardware: use CMock para HAL; nunca chame `HAL_GPIO_*` no teste.
4. Cubra os casos de borda em `references/edge-cases.md`.
5. Rode `ceedling test:all`. Se falhar, corrija o código — nunca enfraqueça o teste.
6. Ao final, reporte: arquivo do teste, comando rodado, resultado.

## Regras
- Teste roda no host, sem placa.
- Se o teste exigir hardware, pare e pergunte.
```

Nome e descrição são o que o agente usa para escolher. Descrição ruim ("ajuda com testes") faz a skill nunca ser chamada. Descrição boa lista **gatilhos**.

## 3. As três skills que você deve construir primeiro

Baseado no guia prático da Beningo para desenvolvedores embarcados:

### 3.1 `/user-stories`

- Entrada: requisito em linguagem natural.
- Saída: história no formato **INVEST** + critérios **Given-When-Then**.
- Por que importa: se o agente não consegue transformar o requisito em critério testável, o requisito está ambíguo — e é melhor descobrir antes do que depois do flash.
- Require que todo critério tenha um `REQ-###` rastreável.

### 3.2 `/embedded-unit-tests`

- `references/mocking.md`: como isolar HAL, ADC, UART, timers.
- `references/edge-cases.md`: lista viva com rollover de timer de 32 bits (49,7 dias @1 kHz), wraparound de buffer circular, reentrância de ISR, underrun de DMA, perda de IRQ, valores `0`/`NULL`/`UINT32_MAX`.
- Por que importa: o modelo esquece esses casos; a skill não.

### 3.3 `/adr` (Architecture Decision Record)

- Template com contexto, decisão, alternativas, consequências.
- **Seção obrigatória `Compliance`**: "como essa decisão será verificada automaticamente?"
- Por que importa: fecha o loop entre arquitetura e CI. Decisão sem verificação vira opinião.

## 4. Chamando e inspecionando

```text
> /skills                      # lista skills disponíveis
> /embedded-unit-tests test_blink.c
> Use a skill /adr para registrar a decisão de usar OSAL
```

Claude pode iniciar a skill sozinho se a descrição casar com o pedido. Você também pode forçar pelo nome.

## 5. Exercício 02 — criar a skill de testes

No `capstone/`:

```bash
mkdir -p .claude/skills/embedded-unit-tests/references
```

1. Copie o `SKILL.md` acima.
2. Crie `references/edge-cases.md` com 6 casos de borda do seu periférico favorito (timer, I2C, ADC).
3. Crie `references/mocking.md` com o comando de mock do CMock para `HAL_GPIO_WritePin`.
4. Teste:

```text
> Use a skill embedded-unit-tests para criar o teste da função blink_toggle()
```

5. Confira com `/context`: o conteúdo de `edge-cases.md` **não** deve aparecer até a skill ser usada.

## 6. Erros comuns

- **Skill com 500 linhas e tudo dentro do `SKILL.md`.** Divide em `references/`. Carregar tudo de uma vez destrói a vantagem.
- **Descrição vaga.** Use verbos e gatilhos: "use quando o usuário pedir teste, TDD, cobertura...".
- **Skill usada para impor regra de segurança.** Não funciona toda vez. Vai para hook.
- **Skill duplicando `CLAUDE.md`.** A skill ensina *como fazer*; o `CLAUDE.md` diz *o que é verdade no projeto*.

## 7. Checklist

- [ ] `SKILL.md` com `name` e `description` no front matter.
- [ ] Procedimento longo dividido em `references/`.
- [ ] Pelo menos 3 skills: user stories, testes, ADR.
- [ ] A skill de ADR tem seção `Compliance`.
- [ ] Testei chamando a skill explicitamente e deixei o Claude escolher sozinho.

**Próximo:** [Módulo 03 — Economia, acurácia e segurança](03-economia-acuracia-seguranca.md).
