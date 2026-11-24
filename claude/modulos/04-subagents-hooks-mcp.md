# Módulo 04 — Subagents, hooks e integração (MCP vs CLI)

**Tempo:** 35 min · **Entrega:** reviewer somente-leitura + hook de proteção + `glab` funcionando.

---

## 1. Subagents: paralelismo e olhar independente

Subagents não servem só para "proteger o contexto" — eles hoje são usados principalmente para **paralelismo** e **independência de julgamento**.

```text
projeto/
└── .claude/agents/
    ├── reviewer.md        # Read, Grep, Glob — não edita
    └── hil-analyst.md     # lê logs de falha da bancada
```

```markdown
---
name: hil-analyst
description: Analisa logs de testes HIL e falhas de hardware; nunca grava firmware.
tools: Read, Grep, Glob
model: haiku
---

Leia artifacts/hil/*.log. Agrupe falhas por causa raiz provável.
Termine com no máximo 10 linhas e liste o REQ afetado.
```

### Quando usar

| Situação | Use |
|---|---|
| Busca ampla no repo | Explore (nativo) |
| Revisão sem viés | subagent que não viu a conversa / `/code-review` |
| Log longo que não deve poluir a conversa | subagent em background, pedir resumo curto |
| Tarefa simples e repetitiva | `model: haiku` |
| Precisa de contexto que só a conversa tem | fork da conversa atual |

### Custo

Cada subagent faz **suas próprias requisições** e conta contra o mesmo limite. Um agente novo precisa coletar contexto que você já tinha — às vezes é **mais lento** que fazer no lugar. Por isso:

- peça resumo curto de volta;
- use Haiku para trabalho simples;
- verifique a fatia de consumo em `/usage`;
- desligue agentes/teammates quando terminarem.

Agent Teams são desabilitados por padrão (`CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1`). Se ligar: Sonnet para teammates, times pequenos, prompts de spawn focados — eles carregam `CLAUDE.md`, MCP e skills automaticamente.

## 2. Hooks: pontos do loop

| Evento | Uso típico em firmware |
|---|---|
| `SessionStart` | checar ferramentas (`arm-none-eabi-gcc`, `st-flash`, `glab`) e avisar se faltar |
| `PreToolUse` | bloquear edição em `Drivers/**`; reescrever comando de teste para `tail -100` |
| `PostToolUse` | formatar C com `clang-format` após `Edit`/`Write` |
| `Stop` | exigir `ceedling test:all` verde antes de encerrar |
| `SessionEnd` | salvar resumo de contexto em `.docs/` (nunca commitado) |

### Exemplo: formatador determinístico

```json
{
  "hooks": {
    "PostToolUse": [
      {
        "matcher": "Edit|Write",
        "hooks": [
          { "command": "clang-format -i \"$FILE\" 2>/dev/null || true" }
        ]
      }
    ]
  }
}
```

Prefira um script seu a um one-liner frágil. E lembre: se a formatação é **preferência**, fica no prompt; se é **gate**, vira hook com `onFailure: "block"`.

### Cuidados

- Hook do tipo `prompt`/`agent` **não é determinístico** — usa modelo para decidir.
- `workspace trust` decide quando hooks de um repo recém-clonado começam a rodar.
- `.claude/settings.json` versionado afeta todo mundo que clonar. Deny rules valem imediatamente; allow rules esperam o trust dialog.

## 3. MCP vs CLI: a decisão certa para GitLab

MCP é excelente quando você precisa de **ferramentas com significado** (criar issue com campos, ler MR com threading, consultar docs internas). O custo é que nome/instruções das tools entram no contexto.

CLI é melhor quando o comando é simples e você quer contexto mínimo:

```bash
# prefira isto para operações corriqueiras
glab issue list --label REQ-001
glab mr create --fill --title "REQ-001: piscar LED de status"
glab mr view 42

# MCP só onde a riqueza de dados compensa
/mcp
```

Regras práticas:

1. Definições de tools MCP são **deferidas por padrão** — o nome entra, o schema completo entra quando usado.
2. `/mcp` mostra os servidores; desligue os que não usa.
3. `/usage` mostra o percentual atribuído a cada servidor — dado real para decidir.
4. Em sandbox, **MCP local roda fora dele**. Trate servidor MCP como código confiável seu.

Exemplo de `.mcp.json` mínimo (somente se você decidir que precisa):

```json
{
  "mcpServers": {
    "gitlab": {
      "command": "npx",
      "args": ["-y", "@gitlab/mcp-server"],
      "env": { "GITLAB_PERSONAL_ACCESS_TOKEN": "$GITLAB_TOKEN" }
    }
  }
}
```

> Confira o nome do pacote e as variáveis na documentação atual do GitLab antes de usar. O ponto aqui é o *padrão*, não a flag exata.

## 4. Worktrees e paralelismo seguro

Quando quiser que duas tarefas não pisotem o mesmo diretório:

```bash
git worktree add -b feat/REQ-002 ../study-REQ-002
cd ../study-REQ-002
claude
```

Cada worktree tem seu próprio `CLAUDE.md` efetivo e seu próprio sessão. O sandbox permite escrita no `.git` compartilhado para `git commit`, mas **nega** `hooks/` e `config` dentro dele.

## 5. Tarefas agendadas

A Overview oficial lista scheduled tasks na nuvem, no desktop e `/loop`. Aplicação natural em firmware:

```text
/schedule 0 3 * * * Analise os artifacts de HIL da noite, agrupe falhas por causa raiz e abra uma issue com o REQ afetado. Não grave firmware.
```

Cuidado: cada disparo envia o contexto da sessão. Use uma sessão dedicada, curta, com subagent Haiku e orçamento observado em `/usage`.

## 6. Exercício 04

1. Crie `.claude/agents/reviewer.md` com `tools: Read, Grep, Glob`.
2. Peça uma mudança pequena e depois: *"use o reviewer para checar contra SPEC.md"*.
3. Crie um `PreToolUse` que bloqueie `Edit|Write` em `Drivers/**` (use `exit 2`).
4. Rode `glab issue list` **pelo Claude** e compare com a versão MCP em `/usage`.
5. Anote qual das duas consumiu mais contexto.

## 7. Checklist

- [ ] Subagent reviewer não consegue editar (teste pedindo para ele editar).
- [ ] Hook `PostToolUse` ou `PreToolUse` funcionando e visível em `/hooks`.
- [ ] `glab` autenticado e usado pelo agente.
- [ ] Entendi quando MCP compensa e quando CLI é melhor.
- [ ] Nenhuma tarefa agendada rodando sem necessidade.

**Próximo:** [Módulo 05 — TDD e testes unitários em C para STM32](05-tdd-unitario-stm32.md).
