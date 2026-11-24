# Plano de aula — Claude Code para firmware STM32 + GitLab

**Duração:** 90 minutos · **Público:** desenvolvedores de firmware · **Formato:** 60% demonstração ao vivo, 40% exercício guiado.

---

## 1. Objetivo de aprendizagem

Ao final, o participante deve ser capaz de:

1. explicar as 6 camadas do Claude Code aplicadas a firmware;
2. escrever um `CLAUDE.md` curto e útil;
3. transformar um requisito em skill + teste + evidência;
4. distinguir regra sugerida de regra imposta por hook/deny rule;
5. montar um pipeline GitLab com unit tests e HIL com lock de bancada;
6. rastrear um binário de release até o `REQ-###` de origem;
7. medir e reduzir consumo de tokens sem perder acurácia.

## 2. Pré-requisitos dos participantes

- Notebook com `claude` instalado e autenticado.
- `git`, `glab`, `python3`, `pipx`, `ceedling`.
-Projeto GitLab de teste (ou o `capstone/` deste material).
- Opcional: STM32 + ST-Link para o bloco HIL.

> Se o grupo não tiver hardware, use `pytest --collect-only` e mostre o `.gitlab-ci.yml`; o objetivo é entender o contrato do job, não a tensão no pino.

## 3. Cronograma

| Tempo | Bloco | Demonstração | Exercício |
|---|---|---|---|
| 00–10 | Abertura | O que muda quando o alvo é firmware | — |
| 10–25 | Mapa das 6 camadas + `CLAUDE.md` | `/init` → edição → `/context all` | Ex 1 |
| 25–40 | Skills + custo/acurácia | `/usage`, `/clear`, `/compact` | Ex 2 |
| 40–55 | Segurança determinística | deny rule + Stop hook + `/sandbox` | Ex 3 |
| 55–70 | TDD em C | vermelho → verde com Ceedling | Ex 4 |
| 70–82 | GitLab CI + HIL | `.gitlab-ci.yml`, `flock`, junit | Ex 5 |
| 82–90 | Rastreabilidade + release + recap | commit guard → tag → release | Ex 6 |

## 4. Roteiro do instrutor

### Abertura (10 min)

Frase de abertura: *"IA não é o problema. O problema é ela decidir com informação incompleta e ninguém ter um cheque determinístico para barrar."*

Mostre a tabela de diferenças do Módulo 00. Pergunte: **qual é o custo de um erro aqui?** (Resposta esperada: regravar, reflashar, às vezes rework de placa.)

### Mapa e contexto (15 min)

- Desenhe as 6 camadas no quadro.
- Rode `/init` e **apague metade** do que ele gerou.
- Escreva as seções Commands / Conventions / Gotchas / Verification.
- Rode `/context all` para mostrar o que foi carregado.

### Skills e custo (15 min)

- Mostre `/skills` e a diferença entre `CLAUDE.md` e skill.
- Rode `/usage` **antes** de uma tarefa longa; depois de `/clear`, rode de novo.
- Mostre a linha `Prompt cache (main)` e explique hit rate.
- Demonstre `/compact keep the firmware changes and the test commands`.

### Segurança determinística (15 min)

- Escreva `"nunca edite Drivers/"` no `CLAUDE.md`. Pergunte: isso garante? (Não.)
- Transforme em deny rule em `.claude/settings.json`. Peça a edição e mostre o bloqueio pelo harness.
- Adicione o `Stop` hook com `ceedling test:all` e exit 2.
- Rode `/sandbox`; no Linux mostre `bubblewrap` + `socat` e strict mode.

### TDD (15 min)

- Skill `/embedded-unit-tests` escreve o teste primeiro.
- Mostre o vermelho: `undefined reference to blink_enter_error`.
- Implemente o mínimo. Rode até verde.
- Reforce: o `Stop` hook não deixa terminar antes do verde.

### CI/HIL (12 min)

- Mostre o `.gitlab-ci.yml`: runners docker vs `stm32-bench`.
- Aponte o `flock` e pergunte: o que acontece se dois jobs I2C rodarem juntos?
- Mostre `artifacts: when: always` e `reports: junit`.
- Se houver placa: `st-flash` + pytest ao vivo.

### Rastreabilidade e release (8 min)

- Commit sem padrão → hook barra.
- Commit com `feat(REQ-001): ... #42` → passa.
- `glab mr create` com `Closes #42`.
- Tag `v1.4.2+k7` → release com changelog e evidência.

## 5. Exercícios e gabarito comentado

### Ex 1 — `CLAUDE.md` em 20 linhas

**Entrega:** arquivo com 4 seções. **Gabarito:** nenhuma linha pode ser respondida pelo compilador. Se diz "use Ceedling", ok; se diz "o C tem ponteiros", corte.

### Ex 2 — Medir consumo

**Entrega:** duas capturas de `/usage` (antes e depois de `/clear`). **Gabarito:** cache hit deve subir e a atribuição a MCPs ociosos deve cair.

### Ex 3 — Regra dura

**Entrega:** deny rule + Stop hook funcionando. **Gabarito:** peça ao Claude para editar `Drivers/gpio.c`; o bloqueio tem que vir do harness, não de texto.

### Ex 4 — Vermelho antes do verde

**Entrega:** log do teste falhando antes da implementação. **Gabarito:** se o participante escreveu a implementação primeiro, refaça — o ponto é TDD, não só teste.

### Ex 5 — Pipeline com lock

**Entrega:** `.gitlab-ci.yml` com `flock` e junit. **Gabarito:** `artifacts.when: always` presente; HIL manual ou por label.

### Ex 6 — Cadeia completa

**Entrega:** Issue, commit, MR, tag. **Gabarito:** `glab issue view 42` deve mostrar o MR fechando; o release deve conter o pipeline URL.

## 6. Rubrica de avaliação rápida

| Critério | 0 | 1 | 2 |
|---|---|---|---|
| Contexto | Copiou o repo inteiro no prompt | `CLAUDE.md` longo | <200 linhas, com gotchas |
| Determinismo | Só texto | Hook sem `onFailure` | Hook/deny com block |
| TDD | Código primeiro | Teste depois | Teste primeiro + bordas |
| CI/HIL | HIL em todo push | HIL sem lock | Lock + evidência |
| Rastreabilidade | Sem ID | ID só no commit | Cadeia completa |
| Custo | Nada medido | Mediu uma vez | `/usage` antes/depois documentado |

## 7. Material de apoio para distribuir

- `livro/index.html` — HTML offline, imprimível em PDF.
- `referencias.md` — links com o módulo em que cada fonte é usada.
- `capstone/` — esqueleto para copiar.
- `slides.html` — projeção com setas; `P` imprime.

## 8. Perguntas que costumam aparecer

**"Isso funciona no VS Code?"** Sim, mesma config. A única diferença é onde você vê o modo de permissão.

**"Vou precisar de MCP do GitLab?"** Só se quiser operações ricas em MRs. Para `issue list`/`mr create`, `glab` custa menos contexto.

**"Por que não usar GitHub Actions?"** O projeto é GitLab. A lógica é a mesma; a casca muda (ver apêndice).

**"Claude Ship resolve a revisão?"** É Team/Enterprise, ligado por Owner, cobrado à parte e indisponível com ZDR/HIPAA/CMEK. `/code-review` local funciona em qualquer plano.

**"Como gravar a aula?"** Veja `video/roteiro.md` e `video/prompt-notebooklm.md`.
