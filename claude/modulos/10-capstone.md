# Módulo 10 — Capstone: montando o sistema completo

**Tempo:** 60 min · **Entrega:** o diretório `capstone/` copiado para um projeto GitLab STM32 real.

---

## 1. Objetivo

Reunir tudo em um esqueleto reutilizável. O capstone não é um firmware completo — é a **configuração que faz o firmware completo ser desenvolvido com segurança, rastreabilidade e custo consciente**.

```text
capstone/
├── CLAUDE.md                         ← restrições curtas e universais
├── SPEC.md                           ← REQ-001 com critérios testáveis
├── .gitlab-ci.yml                    ← build → unit → static → flash → hil → release
├── .claude/
│   ├── settings.json                 ← deny rules + Stop hook
│   ├── rules/generated.md            ← só carrega em Drivers/**
│   ├── agents/reviewer.md            ← Read, Grep, Glob
│   └── skills/
│       ├── user-stories/SKILL.md
│       ├── embedded-unit-tests/SKILL.md
│       └── release-notes/SKILL.md
├── scripts/
│   ├── check-traceability.sh
│   ├── filter-test-output.sh
│   └── block-generated.sh
├── app/
│   ├── blink.c
│   ├── blink.h
│   └── version.h
├── test/test_blink.c
├── tests/hil/test_gpio.py
└── docs/
    ├── decisions/ADR-007.md
    └── ops/verification.md
```

## 2. Sequência de execução guiada

```bash
cd /workspace/projetos/study/capstone
claude
```

Cole no Claude, em ordem:

```text
1. Leia SPEC.md e CLAUDE.md. Explique o que você não conseguiria descobrir só com o código.

2. Use a skill user-stories para transformar REQ-001 em critérios Given-When-Then com rastreabilidade.

3. Use a skill embedded-unit-tests para escrever o teste de blink_enter_error(). Mostre o vermelho. Rode ceedling test:test_blink.

4. Implemente o mínimo em app/blink.c. Rode ceedling test:all até verde.

5. Escreva o ADR-007 usando a skill /adr, com a seção Compliance apontando para o job unit:ceedling.

6. Faça o commit seguindo a convenção. Mostre o comando exato.

7. Crie a Issue com glab e a MR com Closes #42.

8. Use o agente reviewer para auditar: todo REQ tem teste, Issue e evidência?

9. Mostre o diff final e o comando para disparar o job HIL manual.
```

Em cada passo, o agente deve encontrar uma das camadas do Módulo 00. Se alguma etapa "passar batido" (ex.: ele esquece o commit guard), a auditoria do capstone encontrou um gap — é isso que você corrige.

## 3. Matriz de cobertura do capstone

| Camada | Onde está | Como provar |
|---|---|---|
| Contexto | `CLAUDE.md`, `SPEC.md`, `.claude/rules/` | `/context all` + `<200 linhas` |
| Skills | 3 `SKILL.md` + `references/` | `/skills`, `/context` antes/depois |
| Isolamento | `reviewer.md`, sandbox strict | tentar editar com o reviewer |
| Hooks | deny `Drivers/**`, Stop teste, filtro de log | `/hooks`, `--debug-file` |
| Integração | `glab` noCI e local | `glab ci status` |
| Orquestração | `.gitlab-ci.yml`, tag de release | artefatos + junit no MR |
| Rastreabilidade | `REQ-001 → #42 → MR → v1.4.2+k7` | `check-traceability.sh` |
| Custo | `/usage` antes/depois | cache hit > 80%, sem MCP ocioso |

## 4. Auditoria final (o que um revisor deve perguntar)

1. **Existe alguma regra escrita só em texto que deveria ser hook?** Se sim, mova.
2. **`CLAUDE.md` tem mais de 200 linhas?** Migre procedimentos para skills.
3. **O agente consegue editar código gerado?** Se consegue, o deny rule falhou.
4. **Um turno termina com teste vermelho?** Se termina, o Stop hook falhou.
5. **O HIL roda em todo push?** Se roda, você está queimando bancada à toa.
6. **Dois jobs I2C podem rodar juntos?** Se podem, você vai corromper a bancada.
7. **Dá para rastrear o binário até o REQ?** Se não dá, a release é órfã.
8. **`/usage` mostra >30% em um MCP que você usa uma vez por semana?** Desligue.

## 5. Extensões opcionais

### Agent SDK

Quando a CLI não basta — por exemplo, um serviço que responde a um webhook do GitLab e abre Issue automaticamente a partir de falha HIL:

```python
from claude_agent_sdk import query

async for message in query(
    prompt="Resuma este log de HIL e abra uma Issue com o REQ afetado.",
    options={"allowed_tools": ["Read", "Grep"]},
):
    print(message)
```

Use quando houver **orquestração programática**, não para substituir a CLI.

### Dynamic workflows

A documentação traz workflows para orquestrar muitos subagents (ex.: auditoria de todos os REQs em paralelo). É poderoso e caro: cada agente tem contexto próprio. Só entre nesse módulo depois de medir `/usage` com o capstone simples.

## 6. Entregáveis para o workshop

Ao final, cada participante leva:

- [ ] `capstone/` funcional no próprio projeto GitLab;
- [ ] livro `livro/index.html` (offline, imprimível);
- [ ] plano de aula e slides;
- [ ] vídeo curto (NotebookLM) + vídeo completo (Claude Code);
- [ ] um `efficiency-and-security.md` com `/usage` antes/depois e a lista de regras que viraram hook.

Modelo para esse último arquivo:

```markdown
| Métrica | Antes | Depois |
|---|---|---|
| Cache hit (`/usage`) | 41% | 92% |
| Tokens por turno (spinner) | 18k | 3k |
| MCP ativos | 5 | 1 (glab via CLI) |
| Regras só em texto | 9 | 2 |
| Hooks/deny rules | 0 | 4 |
| Turnos terminando com teste vermelho | 3 | 0 |
```

## 7. Checklist final

- [ ] Capstone roda `ceedling test:all` verde.
- [ ] Pipeline GitLab passa até `unit` e `static`.
- [ ] HIL manual com lock e evidência.
- [ ] Commit guardado por script.
- [ ] Release com tag versionada e changelog.
- [ ] `/usage` medido e registrado.
- [ ] Auditoria das 8 perguntas respondida por escrito.

**Próximo:** [Apêndice — GitLab vs GitHub Actions e referência rápida](apendice-gitlab-vs-github.md).
