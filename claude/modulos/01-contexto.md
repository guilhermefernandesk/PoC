# Módulo 01 — Contexto que não cabe no código

**Tempo:** 30 min · **Entrega:** um `CLAUDE.md` curto e verificável.

---

## 1. A regra de ouro

> Escreva no `CLAUDE.md` **somente o que o modelo não consegue descobrir lendo o repo**.

Se o arquivo repete o que já está no `CMakeLists.txt`, ele só custa tokens e atenção em toda requisição. A documentação oficial recomenda **menos de 200 linhas** e sugere `/init` como ponto de partida — o trabalho é quase sempre **remover** linhas.

## 2. Estrutura que funciona para firmware

```markdown
# Commands
- Build:      cmake --build build -j
- Unit tests: ceedling test:all
- One test:   ceedling test:test_blink
- Flash:      st-flash --reset write build/firmware.bin 0x8000000

# Conventions
- Regras de negócio ficam em app/; nunca em Drivers/
- HAL gerada pelo CubeMX é somente leitura: nunca editar Drivers/STM32F1xx_HAL_Driver/
- Sem malloc/printf em código de produção; use osal_log()

# Gotchas
- Rev C do ADC exige 1µs extra após CS (ERRATA-042), ver docs/errata.md
- O timer de 32-bit vira em 49,7 dias; sempre compare com Δ, nunca com absoluto
- O job de I2C na bancada não pode rodar em paralelo (ver .gitlab-ci.yml)

# Verification
- Antes de dizer "pronto": ceedling test:all && ./scripts/shellcheck.sh
```

Quatro seções, ~20 linhas. É o suficiente para mudar o comportamento do agente.

Compare com o que a Beningo usa no artigo sobre Claude Code para firmware: orçamento de RAM/Flash, imagem STM32L476RG, escolha RTOS vs bare-metal, proibição de `malloc`/`printf`, referência à errata do I2C. Tudo aquilo é **informação ausente do código**.

## 3. Alimente o contexto no prompt, não só no arquivo

| O que você tem | Como passar |
|---|---|
| Um arquivo específico | `@path/para/arquivo.c` (cole o conteúdo efetivo na conversa) |
| Uma captura de tela do oscilloscope / erro do CubeIDE | cole a imagem |
| Saída de comando longa | `cat build.log \| claude -p "explique a primeira falha real"` |
| Uma decisão tomada em reunião | escreva em `docs/decisions/NNN-titulo.md` e referencie no prompt |

Prompt específico economiza rodadas. `"o ADC lê 0xFF no canal 2, mas o canal 1 está OK; aqui está o analisador lógico"` é melhor que `"o ADC está ruim"`.

## 4. `SPEC.md` antes de implementar

O fluxo que mais reduz retrabalho:

```text
sessão A (brainstorm)  →  SPEC.md escrita
        ↓
sessão B (limpa)       →  implementação a partir da spec
        ↓
sessão C (revisor)     →  subagent compara diff contra SPEC.md
```

A doc de best practices recomenda explicitamente: depois que a spec existe, **abra uma sessão nova** para construir. O sessão A já está contaminada pelas tentativas de interpretação; a B começa com a definição de pronto por escrito.

Exemplo mínimo de `SPEC.md`:

```markdown
# REQ-001 — Piscar LED de status

Critérios de aceite:
- [ ] LED alterna a cada 500 ms ± 10% com o sistema em IDLE
- [ ] LED fica aceso contínuo em ERROR
- [ ] A lógica de temporização não usa busy-wait
- [ ] Existe teste unitário para a transição IDLE→ERROR

Fora de escopo: PWM, MQTT, modos de baixo consumo.
```

## 5. Regras por caminho: `.claude/rules/`

Quando a regra só vale em parte do repo, não polua o `CLAUDE.md`:

```markdown
<!-- .claude/rules/generated.md -->
---
paths:
  - "Drivers/**"
  - "Middlewares/**"
---

Código gerado pelo CubeMX. Nunca editar diretamente.
Se precisar de uma mudança, altere o .ioc e regenere.
```

O front matter `paths:` faz o arquivo ser carregado só quando Claude trabalha naquele trecho. É o mesmo mecanismo de progressive disclosure das skills, aplicado a regras.

## 6. Preferências pessoais não vão para o repo

```bash
# suas preferências, ignoradas pelo Git
echo 'CLAUDE.local.md' >> .gitignore
```

Use `CLAUDE.local.md` para coisas como "prefira respostas curtas" ou "sempre mostre o comando antes de rodar". O que é do time fica no `CLAUDE.md`; o que é seu fica no `.local`.

## 7. Exercício 01

1. Rode `/init` no `capstone/`.
2. Apague metade do que ele gerou.
3. Escreva as seções Commands / Conventions / Gotchas / Verification com o que você respondeu no Exercício 00.
4. Rode:

```text
/context all
/doctor
/doctor prompt-audit
```

5. Confira: o `CLAUDE.md` tem menos de 200 linhas e nenhuma linha que o compilador já diria?

## 8. Armadilhas

- **`IMPORTANT:` em caixa alta não é mecanismo de enforcement.** Se a linha continua sendo ignorada, ela é regra dura → vai para hook (Módulo 03).
- **`CLAUDE.md` + `AGENTS.md` duplicados.** Funciona, mas sincronize. Se um diz "use Ceedling" e outro diz "use CppUTest", o `/doctor prompt-audit` ajuda a encontrar a contradição.
- **Colocar o procedimento de release inteiro no `CLAUDE.md`.** Isso é skill (Módulo 02).

## 9. Checklist

- [ ] `CLAUDE.md` < 200 linhas.
- [ ] Tem comando exato de teste e de flash.
- [ ] Tem pelo menos uma gotcha de hardware/datasheet.
- [ ] Regras de caminho estão em `.claude/rules/` com `paths:`.
- [ ] `SPEC.md` escrito antes da implementação.

**Próximo:** [Módulo 02 — Skills](02-skills.md).
