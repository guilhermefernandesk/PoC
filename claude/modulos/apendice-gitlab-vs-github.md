# Apêndice — GitLab vs GitHub Actions (e referência rápida)

Este workshop prioriza **GitLab** porque o projeto de destino é GitLab + STM32. O material oficial trata os dois como superfícies de primeira classe, mas os detalhes operacionais diferem.

---

## 1. Comparação curta

| Assunto | GitLab | GitHub Actions |
|---|---|---|
| Pipeline | `.gitlab-ci.yml` | `.github/workflows/*.yml` |
| Unidade de trabalho | `job` dentro de `stage` | `job` dentro de `workflow` |
| Revisão de código | MR, `glab mr` | PR, `gh pr` |
| Runner com hardware | `tags: [stm32-bench]` + self-hosted runner | `runs-on: [self-hosted, stm32-bench]` |
| Artefatos | `artifacts:` com `reports: junit:` | `actions/upload-artifact` + `dorny/test-reporter` |
| Lock de bancada | `flock` no runner ou Device Cloud | `flock` no runner ou serviço externo |
| Revisão por IA na nuvem | integração documentada via GitLab CI/CD | Claude Ship (Team/Enterprise) e GitHub Action |
| Comentários inline | MR review via `glab`/API | `REVIEW.md` **só** no serviço gerenciado do GitHub |
| Triagem de Issues | automação via GitLab CI + `glab` | GitHub Action + `gh` |
| Agendamento | pipeline `schedule` ou `/schedule` | `schedule:` no workflow |

**Regra de ouro:** a lógica TDD/HIL/rastreabilidade é **independente do forge**. O que muda é a casca (comandos `glab` vs `gh`, sintaxe do YAML, onde o junit aparece).

## 2. Equivalências de comando

```bash
# listar pipeline
glab ci status                       |  gh run list

# ver log
glab ci trace <job-id>               |  gh run view --log

# criar MR / PR
glab mr create --fill                |  gh pr create --fill

# comentar
glab mr note 17 -m "HIL verde"       |  gh pr comment 17 -b "HIL verde"

# baixar artefato
glab ci artifacts <job-id>           |  gh run download <run-id>
```

## 3. Sobre `/code-review` local

- Funciona em **qualquer plano**, dentro do terminal, revisando o diff local.
- Em GitLab, para comentar diretamente na MR você depende de `glab` autenticado: `/code-review --comment !123`.
- `REVIEW.md` é específico do serviço gerenciado do GitHub; **não** é um formato universal de relatório.
- Claude Ship (review gerenciado) é recurso Team/Enterprise, ligado por Owner, cobrado à parte e indisponível com ZDR/HIPAA/CMEK. Não faça plano dependendo dele sem confirmar.

## 4. Referência rápida de comandos do material

```text
# Claude Code
/quickstart  /model  /context  /usage  /clear  /compact  /skills
/hooks  /mcp  /code-review  /verify  /rewind  /sandbox  /doctor
Shift+Tab        alterna plan mode
Esc              para a ação atual
f (em /usage)    aplica recomendações automáticas
ultrathink      Deep reasoning só na próxima mensagem

# Firmware
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ceedling test:all
ceedling test:test_blink
ceedling gcov:all
st-flash --reset write build/firmware.bin 0x8000000
pytest -q tests/hil --junitxml=reports/hil/results.xml
flock /tmp/stm32-bench.lock -c "..."

# GitLab
glab auth login
glab issue create --title "REQ-001: ..." --label REQ-001
glab mr create --fill
glab ci status
glab release create v1.4.2+k7 build/firmware.bin --notes "..."
```

## 5. Glossário

| Termo | Significado aqui |
|---|---|
| **Harness** | O programa Claude Code, distinto do modelo. É ele que executa hooks e permissões. |
| **Determinístico** | Executado pelo harness sempre, independente da decisão do modelo. |
| **Progressive disclosure** | Carregar nome/descrição agora, conteúdo completo só quando necessário. |
| **HIL** | Hardware-in-the-Loop: teste executado contra a placa real. |
| **SIL/PIL** | Software-/Processor-in-the-Loop: etapas intermediárias antes do hardware. |
| **CMock** | Gera mocks a partir de headers C; usado com Unity/Ceedling. |
| **Stop hook** | Hook que roda quando Claude tenta encerrar o turno; exit 2 bloqueia. |
| **Sentinel** | Valor falso mostrado ao comando no lugar da credencial real (modo `mask`). |
| **REQ-###** | Identificador estável de requisito, usado do spec ao release. |

## 6. Leitura adicional

- [Overview oficial](https://code.claude.com/docs/en/overview) — porta de entrada.
- [llms.txt](https://code.claude.com/docs/llms.txt) — índice completo e sempre atualizado.
- [Referências consolidadas](../referencias.md) — todas as fontes com o módulo em que são usadas.
